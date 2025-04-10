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

    // Handle materials
    for (int i = 0; i < scene->mNumMaterials; ++i) {
        auto mat = scene->mMaterials[i];
        aiColor3D base_color;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_BASE_COLOR, base_color)) {
            if (AI_SUCCESS != mat->Get(AI_MATKEY_COLOR_DIFFUSE, base_color)) {
                base_color = aiColor3D(0.5f, 0.2f, 0.85f);
            }
        }

        float roughness;
        if (AI_SUCCESS != mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
            roughness = 1;

        materials.push_back(Material{
            .base_color = vec3(base_color.r, base_color.g, base_color.b),
            .roughness = roughness
        });
    }

    if(materials.empty()) {
        printf("Scene has no materials. Default to diffuse\n");
        materials.push_back(Material {.base_color = vec3(0.8f), .roughness = 1});
    } else {
        printf("Loaded %zu materials\n", this->materials.size());
    }
    for (const auto& mat: materials)
        printf("MAT (%f,%f,%f) r=%f\n", mat.base_color[0], mat.base_color[1], mat.base_color[2], mat.roughness);
    offload(device, materials, materials_gpu);

    has_camera = false;
    if (scene->HasCameras()) {
        const auto camera = scene->mCameras[0];
        loaded_camera = Camera {
            .position = vec3{camera->mPosition.x, camera->mPosition.y, camera->mPosition.z},
            .rotation = {.yaw = 0, .pitch = 0}, // TODO
            .fov = camera->mHorizontalFOV,
        };
        has_camera = true;
    }
}

Model::~Model() {
    shd_rn_destroy_buffer(triangles_gpu);
    shd_rn_destroy_buffer(materials_gpu);
}