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

void Optimiser::checkInvalidImages() noexcept
{
    // Loop through and remove all unsupported images
    // TODO: Support packed textures
    set<cgltf_image*> removedImages;
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture& texture = dataCGLTF->textures[i];
        if (texture.image == nullptr && texture.basisu_image == nullptr) {
            continue;
        }
        auto check = [&](cgltf_image* image) {
            if (image != nullptr) {
                // TODO: support packed textures
                if (image->uri == nullptr) {
                    removedImages.insert(image);
                }
            }
        };
        check(texture.image);
        check(texture.basisu_image);
    }

    // Remove any found invalid images
    for (auto& i : removedImages) {
        removeImage(i);
    }
}

void Optimiser::checkInvalidTextures() noexcept
{
    // Nothing to do here
}

void Optimiser::checkInvalidMaterials() noexcept
{
    // Nothing to do here
}

void Optimiser::checkInvalidMeshes() noexcept
{
    // Loop through all meshes and remove any with no primitives attached
    set<cgltf_mesh*> removedMeshes;
    for (cgltf_size i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh& mesh = dataCGLTF->meshes[i];
        if (mesh.primitives_count == 0) {
            removedMeshes.insert(&mesh);
            continue;
        }
    }

    // Remove any found invalid meshes
    for (auto& i : removedMeshes) {
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
