//#include <cstdint>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <cassert>
#include <vector>

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
                                            );

    assert(scene);
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
        }
    }

    //tris.resize(256);
    this->triangles = std::move(tris);
    printf("Loaded %zu triangles\n", this->triangles.size());
    offload(device, triangles, triangles_gpu);

    // --------------- Special lights
    if (scene->HasLights()) {
        vec3 env_color = vec3(0);
        for (int i = 0; i < scene->mNumLights; ++i) {
            const auto light = scene->mLights[i];
            if (light->mType == aiLightSource_AMBIENT) {
                env_color = vec3(light->mColorAmbient[0], light->mColorAmbient[1], light->mColorAmbient[2]);
            }
        }
        if (env_color[0] > 0 || env_color[1] > 0 || env_color[2] > 0)
            emitters.push_back(Emitter{ .emission = env_color });
    }
    if (emitters.empty()) {
        printf("No light given for scene. Adding default environment light.\n");
        emitters.push_back(Emitter{ .emission = vec3(10/pi) });
    }
    printf("Loaded %zu emitters\n", this->emitters.size());
    for (const auto& emitter: emitters)
        printf("LIGHT (%f,%f,%f)\n", emitter.emission[0], emitter.emission[1], emitter.emission[2]);
    offload(device, emitters, emitters_gpu);

    // --------------- Handle materials
    for (int i = 0; i < scene->mNumMaterials; ++i) {
        auto mat = scene->mMaterials[i];
        aiColor3D base_color;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_BASE_COLOR, base_color)) {
            if (AI_SUCCESS != mat->Get(AI_MATKEY_COLOR_DIFFUSE, base_color))
                base_color = aiColor3D(1.0f, 0.0f, 1.0f);
        }

        float roughness;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
            roughness = 1;

        float metallic;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic))
            metallic = 0;

        float ior=1;

        float transmission;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmission))
            transmission = 0;

        vec3 emission = vec3(0);
        if (AI_SUCCESS != mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
            emission = vec3(0);
        
        float emission_strength = 0;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emission_strength))
            emission_strength = 0;
        
        emission = emission_strength * emission;
        if (emission[0] + emission[1] + emission[2] > 0) {
// TODO:
        }

        materials.push_back(Material{
            .base_color = vec3(base_color.r, base_color.g, base_color.b),
            .roughness = roughness,
            
            .ior = ior,
            .metallic = metallic,
            .transmission = transmission,
            
            .emission = emission
        });
    }

    if (materials.empty()) {
        printf("Scene has no materials. Default to diffuse\n");
        materials.push_back(Material {.base_color = vec3(0.8f), .roughness = 1});
    } else {
        printf("Loaded %zu materials\n", this->materials.size());
    }
    for (const auto& mat: materials)
        printf("MAT (%f,%f,%f) r=%f\n", mat.base_color[0], mat.base_color[1], mat.base_color[2], mat.roughness);
    offload(device, materials, materials_gpu);

    // --------------- Camera
    has_camera = false;
    if (scene->HasCameras()) {
        const auto camera = scene->mCameras[0];
        loaded_camera = Camera {
            .position = vec3{camera->mPosition.x, camera->mPosition.y, camera->mPosition.z},
            .rotation = {.yaw = 0, .pitch = 0}, // TODO
            .fov = camera->mHorizontalFOV,
        };
        printf("CAMERA eye=(%f,%f,%f)\n", loaded_camera.position[0], loaded_camera.position[1], loaded_camera.position[2]);
        has_camera = true;
    }
}

Model::~Model() {
    shd_rn_destroy_buffer(triangles_gpu);
    shd_rn_destroy_buffer(materials_gpu);
    shd_rn_destroy_buffer(emitters_gpu);
}