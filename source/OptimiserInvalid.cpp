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
#include <set>

using namespace std;

void Optimiser::checkInvalidImages() noexcept
{
    // Loop through and remove all invalid images
    set<cgltf_image*> removedImages;
    for (cgltf_size i = 0; i < dataCGLTF->images_count; ++i) {
        cgltf_image* image = &dataCGLTF->images[i];
        if (!isValid(image)) {
            removedImages.insert(image);
        }
    }

    // Remove any found invalid images
    for (auto& i : removedImages | views::reverse) {
        printWarning("Removed invalid image: "s + getName(*i));
        removeImage(i);
    }
}

void Optimiser::checkInvalidTextures() noexcept
{
    // Loop through and remove all invalid textures
    set<cgltf_texture*> removedTextures;
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture* texture = &dataCGLTF->textures[i];
        if (!isValid(texture)) {
            removedTextures.insert(texture);
        }
    }

    // Remove any found invalid textures
    for (auto& i : removedTextures | views::reverse) {
        printWarning("Removed invalid texture: "s + getName(*i));
        removeTexture(i);
    }
}

void Optimiser::checkInvalidMaterials() noexcept
{
    // Loop through and remove all invalid materials
    set<cgltf_material*> removedMaterials;
    for (cgltf_size i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material* material = &dataCGLTF->materials[i];
        if (!isValid(material)) {
            removedMaterials.insert(material);
            continue;
        }
    }

    // Remove any found invalid meshes
    for (auto& i : removedMaterials | views::reverse) {
        printWarning("Removed invalid material: "s + getName(*i));
        removeMaterial(i);
    }
}

void Optimiser::checkInvalidMeshes() noexcept
{
    // Loop through and remove all invalid meshes
    set<cgltf_mesh*> removedMeshes;
    for (cgltf_size i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh* mesh = &dataCGLTF->meshes[i];
        if (!isValid(mesh)) {
            removedMeshes.insert(mesh);
            continue;
        }
    }

    // Remove any found invalid meshes
    for (auto& i : removedMeshes | views::reverse) {
        printWarning("Removed invalid mesh: "s + getName(*i));
        removeMesh(i);
    }
}

void Optimiser::passInvalid() noexcept
{
    // Order of operations must be performed bottom up
    checkInvalidImages();
    checkInvalidTextures();
    checkInvalidMaterials();
    checkInvalidMeshes();
}
