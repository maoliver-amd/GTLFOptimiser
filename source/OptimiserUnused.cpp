/**
 * Copyright Matthew Oliver
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Optimiser.h"
#include "Shared.h"
#include "SharedCGLTF.h"

#include <set>

using namespace std;

void Optimiser::checkUnusedImages() noexcept
{
    // Loop through all textures and check for unused images
    set<cgltf_image*> removedImages;
    set<cgltf_image*> validImages;
    for (size_t i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture& texture = dataCGLTF->textures[i];
        if (texture.basisu_image != nullptr) {
            validImages.insert(texture.basisu_image);
        }
        if (texture.image != nullptr) {
            validImages.insert(texture.image);
        }
    }
    // Loop through all images and compare against the found ones
    for (cgltf_size i = 0; i < dataCGLTF->images_count; ++i) {
        cgltf_image* image = &dataCGLTF->images[i];
        if (!validImages.contains(image)) {
            removedImages.insert(image);
        }
    }

    // Remove any found unused images
    for (auto& i : removedImages) {
        printWarning("Removed unused image: "s + getName(*i));
        if (i->uri != nullptr) {
            // Delete file from disk
            string imageFile = rootFolder + i->uri;
            remove(imageFile.c_str());
        }
        removeImage(i);
    }
}

void Optimiser::checkUnusedTextures() noexcept
{
    // Loop through all materials and check for unused textures
    set<cgltf_texture*> removedTextures;
    set<cgltf_texture*> validTextures;
    for (size_t i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& material = dataCGLTF->materials[i];
        runOverMaterialTextures(material, [&](cgltf_texture*& p, bool, bool, bool = false) {
            if (p != nullptr) {
                if (p->image != nullptr || p->basisu_image != nullptr) {
                    // Note: Assumes that invalid images have already been removed
                    validTextures.insert(p);
                }
            }
        });
    }
    // Loop through all textures and compare against the found ones
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture* texture = &dataCGLTF->textures[i];
        if (!validTextures.contains(texture)) {
            removedTextures.insert(texture);
        }
    }
    // Remove any found unused textures
    for (auto& i : removedTextures) {
        printWarning("Removed unused texture: "s + getName(*i));
        removeTexture(i);
    }
}

void Optimiser::checkUnusedMaterials() noexcept
{
    // Loop through all meshes and check for unused materials
    set<cgltf_material*> validMaterials;
    for (size_t i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh& mesh = dataCGLTF->meshes[i];
        for (cgltf_size j = 0; j < mesh.primitives_count; ++j) {
            cgltf_primitive& prim = mesh.primitives[j];
            if (prim.material != nullptr) {
                validMaterials.insert(prim.material);
            }
        }
    }
    // Loop through all materials and compare against the found ones
    set<cgltf_material*> removedMaterials;
    for (cgltf_size i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material* material = &dataCGLTF->materials[i];
        if (!validMaterials.contains(material)) {
            removedMaterials.insert(material);
        }
    }

    // Remove any found unused materials
    for (auto& i : removedMaterials) {
        printWarning("Removed unused material: "s + getName(*i));
        removeMaterial(i);
    }
}

void Optimiser::checkUnusedMeshes() noexcept
{
    // Loop through all nodes and check for unused meshes
    set<cgltf_mesh*> removedMeshes;
    // TODO

    // Remove any found invalid meshes
    for (auto& i : removedMeshes) {
        printWarning("Removed unused mesh: "s + getName(*i));
        removeMesh(i);
    }
}

void Optimiser::passUnused() noexcept
{
    // Order of operations must be performed bottom up
    checkUnusedMeshes();
    checkUnusedMaterials();
    checkUnusedTextures();
    checkUnusedImages();
}
