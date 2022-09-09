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

#include <ranges>

using namespace std;

void Optimiser::checkDuplicateImages() noexcept
{
    // Check for duplicate images
    map<cgltf_image*, cgltf_image*> imageDuplicates;
    for (cgltf_size i = 0; i < dataCGLTF->images_count; ++i) {
        cgltf_image* image = &dataCGLTF->images[i];
        for (cgltf_size j = i + 1; j < dataCGLTF->images_count; ++j) {
            cgltf_image* image2 = &dataCGLTF->images[j];
            if (*image == *image2) {
                imageDuplicates[image2] = image;
                // Check if image is itself a replacement
                if (auto pos = imageDuplicates.find(image); pos != imageDuplicates.end()) {
                    imageDuplicates[image2] = pos->second;
                }
            }
        }
    }
    // Update textures to remove duplicate images
    for (size_t i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture& texture = dataCGLTF->textures[i];
        if (auto pos = imageDuplicates.find(texture.image); pos != imageDuplicates.end()) {
            texture.image = pos->second;
        }
        if (auto pos = imageDuplicates.find(texture.basisu_image); pos != imageDuplicates.end()) {
            texture.image = pos->second;
        }
    }
    // Remove duplicate images
    for (auto& i : imageDuplicates | views::reverse) {
        auto current = i.first;
        auto current2 = i.second;
        printWarning("Removed duplicate image: "s + getName(*current) + ", " + getName(*current2));
        removeImage(current);
        // Update pointers for move
        for (auto& j : imageDuplicates) {
            if (j.second > current) {
                j.second = j.second - 1;
            }
        }
    }
}

void Optimiser::checkDuplicateTextures() noexcept
{
    // Check for duplicate textures
    map<cgltf_texture*, cgltf_texture*> textureDuplicates;
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture* texture = &dataCGLTF->textures[i];
        for (cgltf_size j = i + 1; j < dataCGLTF->textures_count; ++j) {
            cgltf_texture* texture2 = &dataCGLTF->textures[j];
            if (*texture == *texture2) {
                textureDuplicates[texture2] = texture;
                // Check if texture is itself a replacement
                if (auto pos = textureDuplicates.find(texture); pos != textureDuplicates.end()) {
                    textureDuplicates[texture2] = pos->second;
                }
            }
        }
    }
    // Update materials to remove duplicate textures
    for (size_t i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& material = dataCGLTF->materials[i];
        runOverMaterialTextures(material, [&](cgltf_texture*& p, bool, bool, bool = false) {
            if (auto pos = textureDuplicates.find(p); pos != textureDuplicates.end()) {
                p = pos->second;
            }
        });
    }
    // Remove duplicate textures
    for (auto& i : textureDuplicates | views::reverse) {
        auto current = i.first;
        auto current2 = i.second;
        printWarning("Removed duplicate texture: "s + getName(*current) + ", " + getName(*current2));
        removeTexture(current);
        // Update pointers for move
        for (auto& j : textureDuplicates) {
            if (j.second > current) {
                j.second = j.second - 1;
            }
        }
    }
}

void Optimiser::checkDuplicateMaterials() noexcept
{
    // Check for duplicate materials
    map<cgltf_material*, cgltf_material*> materialDuplicates;
    for (cgltf_size i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material* material = &dataCGLTF->materials[i];
        for (cgltf_size j = i + 1; j < dataCGLTF->materials_count; ++j) {
            cgltf_material* material2 = &dataCGLTF->materials[j];
            if (*material == *material2) {
                materialDuplicates[material2] = material;
                // Check if material is itself a replacement
                if (auto pos = materialDuplicates.find(material); pos != materialDuplicates.end()) {
                    materialDuplicates[material2] = pos->second;
                }
            }
        }
    }
    // Update meshes to remove duplicate materials
    for (size_t i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh& mesh = dataCGLTF->meshes[i];
        for (cgltf_size j = 0; j < mesh.primitives_count; ++j) {
            cgltf_primitive& prim = mesh.primitives[j];
            if (auto pos = materialDuplicates.find(prim.material); pos != materialDuplicates.end()) {
                prim.material = pos->second;
            }
        }
    }
    // Remove duplicate materials
    for (auto& i : materialDuplicates | views::reverse) {
        auto current = i.first;
        auto current2 = i.second;
        printWarning("Removed duplicate material: "s + getName(*current) + ", " + getName(*current2));
        removeMaterial(current);
        // Update pointers for move
        for (auto& j : materialDuplicates) {
            if (j.second > current) {
                j.second = j.second - 1;
            }
        }
    }
}

void Optimiser::checkDuplicateMeshes() noexcept
{
    // TODO
}

void Optimiser::passDuplicate() noexcept
{
    // Order of operations must be performed bottom up
    checkDuplicateImages();
    checkDuplicateTextures();
    checkDuplicateMaterials();
    checkDuplicateMeshes();
}
