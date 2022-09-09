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

#include <map>
#include <ranges>
#include <set>

using namespace std;

void Optimiser::removeImage(cgltf_image* image) noexcept
{
    // Remove the image from the images list
    cgltf_size imagePos = 0;
    while (true) {
        cgltf_image* current = &dataCGLTF->images[imagePos];
        if (current == image) {
            // Remove image from gltf
            cgltf_remove_image(dataCGLTF.get(), current);
            memmove(current, current + 1, (dataCGLTF->images_count - imagePos - 1) * sizeof(cgltf_image));
            --dataCGLTF->images_count;
            dataCGLTF->images[dataCGLTF->images_count] = {0};
            break;
        }
        if (++imagePos >= dataCGLTF->images_count) {
            break;
        }
    }

    // Loop through all textures and set any matching pointers to null
    set<cgltf_texture*> removedTextures;
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture* current = &dataCGLTF->textures[i];
        if (current->image == image) {
            current->image = nullptr;
        }
        if (current->basisu_image == image) {
            current->basisu_image = nullptr;
        }
        if (!isValid(current)) {
            removedTextures.insert(current);
        }
    }
    // Remove any found invalid textures
    for (auto& i : removedTextures | views::reverse) {
        printWarning("Removed invalidated texture: "s + getName(*i));
        removeTexture(i);
    }

    // Loop through all textures and update image pointers to compensate for list change
    if (imagePos >= dataCGLTF->images_count) {
        return;
    }
    cgltf_image* current = &dataCGLTF->images[imagePos];
    for (cgltf_size k = 0; k < dataCGLTF->textures_count; ++k) {
        cgltf_texture& texture = dataCGLTF->textures[k];
        if (texture.image > current) {
            texture.image = texture.image - 1;
        }
        if (texture.basisu_image > current) {
            texture.basisu_image = texture.basisu_image - 1;
        }
    }
}

void Optimiser::removeTexture(cgltf_texture* texture) noexcept
{
    // Check for orphaned images
    map<cgltf_image*, bool> orphanedImages;
    if (texture->image != nullptr) {
        orphanedImages.emplace(texture->image, false);
    }
    if (texture->basisu_image != nullptr) {
        orphanedImages.emplace(texture->basisu_image, false);
    }
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture& current = dataCGLTF->textures[i];
        if (auto pos = orphanedImages.find(current.image); pos != orphanedImages.end()) {
            pos->second = true;
        }
        if (auto pos = orphanedImages.find(current.basisu_image); pos != orphanedImages.end()) {
            pos->second = true;
        }
    }
    for (auto& i : orphanedImages) {
        if (!i.second) {
            removeImage(i.first);
        }
    }

    // Remove the texture from the textures list
    cgltf_size texPos = 0;
    while (true) {
        cgltf_texture* current = &dataCGLTF->textures[texPos];
        if (current == texture) {
            cgltf_remove_texture(dataCGLTF.get(), current);
            memmove(current, current + 1, (dataCGLTF->textures_count - texPos - 1) * sizeof(cgltf_texture));
            --dataCGLTF->textures_count;
            dataCGLTF->textures[dataCGLTF->textures_count] = {0};
            break;
        }
        if (++texPos >= dataCGLTF->textures_count) {
            break;
        }
    }

    // Loop through all materials and set any matching pointers to null
    set<cgltf_material*> removedMaterials;
    for (cgltf_size j = 0; j < dataCGLTF->materials_count; ++j) {
        cgltf_material* current = &dataCGLTF->materials[j];
        runOverMaterialTextures(*current, [&](cgltf_texture*& p, bool, bool, bool = false) {
            if (p == texture) {
                p = nullptr;
            }
        });
        if (!isValid(current)) {
            removedMaterials.insert(current);
        }
    }
    // Remove any found invalid materials
    for (auto& i : removedMaterials | views::reverse) {
        printWarning("Removed invalidated material: "s + getName(*i));
        removeMaterial(i);
    }

    // Loop through all materials and update texture pointers to compensate for list change
    if (texPos >= dataCGLTF->textures_count) {
        return;
    }
    cgltf_texture* current = &dataCGLTF->textures[texPos];
    for (cgltf_size k = 0; k < dataCGLTF->materials_count; ++k) {
        cgltf_material& material = dataCGLTF->materials[k];
        runOverMaterialTextures(material, [&](cgltf_texture*& p, bool, bool, bool = false) {
            if (p > current) {
                p = p - 1;
            }
        });
    }
}

void Optimiser::removeMaterial(cgltf_material* material) noexcept
{
    // Check for orphaned textures
    map<cgltf_texture*, bool> orphanedTextures;
    runOverMaterialTextures(*material, [&](cgltf_texture*& p, bool, bool, bool = false) {
        if (p != nullptr) {
            orphanedTextures.emplace(p, false);
        }
    });
    for (cgltf_size i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& current = dataCGLTF->materials[i];
        runOverMaterialTextures(current, [&](cgltf_texture*& p, bool, bool, bool = false) {
            if (auto pos = orphanedTextures.find(p); pos != orphanedTextures.end()) {
                pos->second = true;
            }
        });
    }
    for (auto& i : orphanedTextures) {
        if (!i.second) {
            removeTexture(i.first);
        }
    }

    // Remove the material from the materials list
    cgltf_size matPos = 0;
    while (true) {
        cgltf_material* current = &dataCGLTF->materials[matPos];
        if (current == material) {
            cgltf_remove_material(dataCGLTF.get(), current);
            memmove(current, current + 1, (dataCGLTF->materials_count - matPos - 1) * sizeof(cgltf_material));
            --dataCGLTF->materials_count;
            dataCGLTF->materials[dataCGLTF->materials_count] = {0};
            break;
        }
        if (++matPos >= dataCGLTF->materials_count) {
            break;
        }
    }

    // Loop through all primitives and set any matching pointers to null
    set<cgltf_mesh*> removedMeshes;
    for (cgltf_size i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh* current = &dataCGLTF->meshes[i];
        for (cgltf_size j = 0; j < current->primitives_count; ++j) {
            cgltf_primitive& prim = current->primitives[j];
            if (prim.material == material) {
                prim.material = nullptr;
            }
        }
        if (!isValid(current)) {
            removedMeshes.insert(current);
        }
    }
    // Remove any found invalid materials
    for (auto& i : removedMeshes | views::reverse) {
        printWarning("Removed invalidated mesh: "s + getName(*i));
        removeMesh(i);
    }

    // Loop through all primitives and update material pointers to compensate for list change
    if (matPos >= dataCGLTF->materials_count) {
        return;
    }
    cgltf_material* current = &dataCGLTF->materials[matPos];
    for (cgltf_size i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh& mesh = dataCGLTF->meshes[i];
        for (cgltf_size k = 0; k < mesh.primitives_count; ++k) {
            cgltf_primitive& prim = mesh.primitives[k];
            if (prim.material > current) {
                prim.material = prim.material - 1;
            }
        }
    }
}

void Optimiser::removeMesh(cgltf_mesh* mesh) noexcept
{
    // Check for orphaned materials
    map<cgltf_material*, bool> orphanedMaterials;
    for (cgltf_size i = 0; i < mesh->primitives_count; ++i) {
        cgltf_primitive& current = mesh->primitives[i];
        if (current.material != nullptr) {
            orphanedMaterials.emplace(current.material, false);
        }
    }
    for (cgltf_size i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh* current = &dataCGLTF->meshes[i];
        for (cgltf_size j = 0; j < current->primitives_count; ++j) {
            cgltf_primitive& prim = current->primitives[j];
            if (auto pos = orphanedMaterials.find(prim.material); pos != orphanedMaterials.end()) {
                pos->second = true;
            }
        }
    }
    for (auto& i : orphanedMaterials) {
        if (!i.second) {
            removeMaterial(i.first);
        }
    }

    // Remove the mesh from the meshes list
    cgltf_size meshPos = 0;
    while (true) {
        cgltf_mesh* current = &dataCGLTF->meshes[meshPos];
        if (current == mesh) {
            cgltf_remove_mesh(dataCGLTF.get(), current);
            memmove(current, current + 1, (dataCGLTF->meshes_count - meshPos - 1) * sizeof(cgltf_mesh));
            --dataCGLTF->meshes_count;
            dataCGLTF->meshes[dataCGLTF->meshes_count] = {0};
            break;
        }
        if (++meshPos >= dataCGLTF->meshes_count) {
            break;
        }
    }

    // Loop through all nodes and set any matching pointers to null
    for (cgltf_size j = 0; j < dataCGLTF->nodes_count; ++j) {
        cgltf_node& current = dataCGLTF->nodes[j];
        if (current.mesh == mesh) {
            current.mesh = nullptr;
        }
    }

    // Loop through all nodes and update mesh pointers to compensate for list change
    if (meshPos >= dataCGLTF->meshes_count) {
        return;
    }
    cgltf_mesh* current = &dataCGLTF->meshes[meshPos];
    for (cgltf_size k = 0; k < dataCGLTF->nodes_count; ++k) {
        cgltf_node& node = dataCGLTF->nodes[k];
        if (node.mesh > current) {
            node.mesh = node.mesh - 1;
        }
    }
}
