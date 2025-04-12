//#include <cstdint>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <cassert>
#include <vector>
#include <unordered_set>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "model.h"

using namespace shady;

Model::Model(const char* path, Device* device) {
    Assimp::Importer importer;

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene* scene = importer.ReadFile( path,
                                                aiProcess_Triangulate
                                              | aiProcess_PreTransformVertices
                                              | aiProcess_SortByPType
                                              | aiProcess_GenSmoothNormals
                                              | aiProcess_RemoveRedundantMaterials
                                            );
                                        
    if (scene == nullptr) {
        printf("ERROR: Import of \"%s\" failed: %s\n", path, importer.GetErrorString());
        std::abort(); // TODO: Proper tools should catch this
    }

    // --------------- Special lights
    vec3 env_color = vec3(0);
    if (scene->HasLights()) {
        for (int i = 0; i < scene->mNumLights; ++i) {
            const auto light = scene->mLights[i];
            if (light->mType == aiLightSource_AMBIENT) {
                env_color = vec3(light->mColorAmbient[0], light->mColorAmbient[1], light->mColorAmbient[2]);
            }
        }

    }
    emitters.push_back(Emitter{ .emission = env_color, .prim_id = -1 });

    // --------------- Handle materials
    std::unordered_set<int> emissive_materials;
    for (int i = 0; i < scene->mNumMaterials; ++i) {
        auto mat = scene->mMaterials[i];
        aiColor3D base_color;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_BASE_COLOR, base_color)) {
            if (AI_SUCCESS != mat->Get(AI_MATKEY_COLOR_DIFFUSE, base_color))
                base_color = aiColor3D(1.0f, 0.0f, 1.0f);
        }

        int base_color_tex = -1;
        aiString base_color_tex_path;
        if (AI_SUCCESS == mat->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &base_color_tex_path)) {
            if (std::filesystem::path(base_color_tex_path.C_Str()).is_relative())
                base_color_tex_path = (std::filesystem::path(path).parent_path() / base_color_tex_path.C_Str()).generic_string();

            int w, h, c;
            stbi_uc* data = stbi_load(base_color_tex_path.C_Str(), &w, &h, &c, 4);
            
            // Linearize
            for(size_t i = 0; i < w * h * 4; ++i) {
                float v = data[i] / 255.0f;
                
                if (v <= 0.04045f)
                    v = v / 12.92f;
                else
                    v = powf((v + 0.055f) / 1.055f, 2.4f);

                data[i] = v * 255;
            }

            if (data != nullptr) {
                // Copy to our big buffer... not super effective, but works
                const size_t start = texture_data.size();
                const size_t end = start + w * h * 4;
                texture_data.resize(end);

                memcpy(texture_data.data() + start, data, end - start);
                stbi_image_free(data);

                base_color_tex = textures.size();
                textures.push_back(TextureDescriptor {
                    .byte_offset = (unsigned int)start,
                    .width  = w,
                    .height = h
                });
            } else {
                printf("Could not load image '%s'\n", base_color_tex_path.C_Str());
            }
        }

        float roughness = 1;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
            roughness = 1;

        float metallic = 0;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic))
            metallic = 0;

        float specular = 0;
        aiColor3D specularColor;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_SPECULAR, specularColor)) {
            specular = color_average(vec3(specularColor.r, specularColor.g, specularColor.b));
        } else{
            if (AI_SUCCESS != mat->Get(AI_MATKEY_SPECULAR_FACTOR, specular))
                specular = 0;
        }

        float transmission = 0;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmission))
            transmission = 0;

        vec3 emission = vec3(1);
        if (AI_SUCCESS != mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
            emission = vec3(1);
        
        float emission_strength = 0;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emission_strength))
            emission_strength = 0;
        
        emission = emission_strength * emission;
        if (color_average(emission) > 0)
            emissive_materials.insert(materials.size());

        materials.push_back(Material{
            .base_color = vec3(base_color.r, base_color.g, base_color.b),
            .base_color_tex = base_color_tex,

            .roughness = fmaxf(roughness * roughness, 1e-4f), // We store squared version
            .roughness_tex = -1, // TODO
            
            .ior = 1.50f,//(2/(1-sqrtf(0.08f * specular))-1),
            .metallic = metallic,
            .transmission = transmission,
            
            .emission = emission
        });
    }

    if (materials.empty()) {
        printf("Scene has no materials. Default to diffuse\n");
        materials.push_back(Material {.base_color = vec3(0.8f), .roughness = 1, .ior = 1, .metallic = 0, .transmission = 0, .emission = vec3(0)});
    } else {
        printf("Loaded %zu materials\n", this->materials.size());
    }
    for (const auto& mat: materials)
        printf("MAT c=(%f,%f,%f) r=%f, m=%f, n=%f, t=%f\n", mat.base_color[0], mat.base_color[1], mat.base_color[2], mat.roughness, mat.metallic, mat.ior, mat.transmission);
    offload(device, materials, materials_gpu);

    // --------------- Triangles
    std::vector<Triangle> tris;
    for (int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];
        int mat_id = scene->HasMaterials() ? mesh->mMaterialIndex : 0;
        for (int j = 0; j < mesh->mNumFaces; j++) {
            auto& face = mesh->mFaces[j];
            assert(face.mNumIndices == 3);
            auto v0 = mesh->mVertices[face.mIndices[0]];
            auto v1 = mesh->mVertices[face.mIndices[1]];
            auto v2 = mesh->mVertices[face.mIndices[2]];

            aiVector3D n0 = aiVector3D(0,0,1);
            aiVector3D n1 = aiVector3D(0,0,1);
            aiVector3D n2 = aiVector3D(0,0,1);
            if (mesh->HasNormals()) {
                n0 = mesh->mNormals[face.mIndices[0]];
                n1 = mesh->mNormals[face.mIndices[1]];
                n2 = mesh->mNormals[face.mIndices[2]];
            }

            aiVector3D t0 = aiVector3D(0);
            aiVector3D t1 = aiVector3D(0);
            aiVector3D t2 = aiVector3D(0);
            if (mesh->HasTextureCoords(0)) {
                t0 = mesh->mTextureCoords[0][face.mIndices[0]];
                t1 = mesh->mTextureCoords[0][face.mIndices[1]];
                t2 = mesh->mTextureCoords[0][face.mIndices[2]];
            }

            Triangle tri {
                .prim_id = (int32_t)tris.size(), // TODO: Better 64bit
                .mat_id = mat_id,
                // Vertices
                .v0 = { v0.x, v0.y, v0.z },
                .v1 = { v1.x, v1.y, v1.z },
                .v2 = { v2.x, v2.y, v2.z },
                // Normals
                .n0 = { n0.x, n0.y, n0.z },
                .n1 = { n1.x, n1.y, n1.z },
                .n2 = { n2.x, n2.y, n2.z },
                // Texture Coords (we only support 2D)
                .t0 = { t0.x, t0.y },
                .t1 = { t1.x, t1.y },
                .t2 = { t2.x, t2.y },
            };
            tris.push_back(tri);

            if (emissive_materials.contains(mat_id)) {
                float area = tri.get_area();
                if (area > 0)
                    emitters.push_back(Emitter{ .emission = vec3{materials[mat_id].emission[0], materials[mat_id].emission[1], materials[mat_id].emission[2]}, .prim_id = tri.prim_id});
            }
        }
    }

    this->triangles = std::move(tris);
    printf("Loaded %zu triangles\n", this->triangles.size());
    offload(device, triangles, triangles_gpu);

    // --------------- Lights
    if (emitters.empty() || (emitters.size() == 1 && color_average(emitters.at(0).emission) == 0)) {
        printf("No light given for scene. Adding default environment light.\n");
        const Emitter env{ .emission = vec3(10/M_PI), .prim_id = -1 };
        if(emitters.empty())
            emitters.push_back(env);
        else
            emitters[0] = env;
    }
    printf("Loaded %zu emitters\n", this->emitters.size());
    for (const auto& emitter: emitters)
        printf("LIGHT (%f,%f,%f) %i\n", emitter.emission[0], emitter.emission[1], emitter.emission[2], emitter.prim_id);
    offload(device, emitters, emitters_gpu);

    // -------------- Upload textures
    if (textures.empty()) {
        textures_gpu = nullptr;
        texture_data_gpu = nullptr;
    } else {
        offload(device, textures, textures_gpu);
        offload(device, texture_data, texture_data_gpu);
    }

    // --------------- Camera
    loaded_camera.position = vec3(0,0,0);
    loaded_camera.rotation.yaw = 0;
    loaded_camera.rotation.pitch = 0;
    loaded_camera.fov = 1.04719755f; // In radians (60deg)
    if (scene->HasCameras()) {
        const auto camera = scene->mCameras[0];
        loaded_camera = Camera {
            .position = vec3{camera->mPosition.x, camera->mPosition.y, camera->mPosition.z},
            .rotation = {.yaw = 0, .pitch = 0}, // TODO
            .fov = camera->mHorizontalFOV,
        };
        printf("CAMERA eye=(%f,%f,%f)\n", loaded_camera.position[0], loaded_camera.position[1], loaded_camera.position[2]);
    }
}

Model::~Model() {
    shd_rn_destroy_buffer(triangles_gpu);
    shd_rn_destroy_buffer(materials_gpu);
    shd_rn_destroy_buffer(emitters_gpu);
    if (textures_gpu)
        shd_rn_destroy_buffer(textures_gpu);
    if (texture_data_gpu)
        shd_rn_destroy_buffer(texture_data_gpu);
}