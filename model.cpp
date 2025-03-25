

//#include <cstdint>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <assert.h>

#include "model.h"

Model::Model(std::string& path, shd::Device* device) {
    Assimp::Importer importer;

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene* scene = importer.ReadFile( path,
                                              aiProcess_CalcTangentSpace       |
                                              aiProcess_Triangulate            |
                                              aiProcess_JoinIdenticalVertices  |
                                              aiProcess_PreTransformVertices |
                                              aiProcess_SortByPType);

    assert(scene);
    size_t total_tri_count = 0;
    size_t total_indices_count = 0;
    for (int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];
        total_tri_count += mesh->mNumVertices;
        total_indices_count += mesh->mNumFaces * 3;
    }
    this->triangles_count = total_tri_count;
    size_t buffer_size = total_tri_count *= sizeof(Triangle);

    this->triangles = shd::shd_rn_allocate_buffer_device(device, buffer_size);
    for (int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];
        total_tri_count += mesh->mNumVertices;
    }
}

Model::~Model() {

}