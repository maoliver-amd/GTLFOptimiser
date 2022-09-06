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
#include <set>
#include <vector>

using namespace std;

bool Optimiser::passMeshes() noexcept
{
    // Loop through all nodes and check for unused meshes
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
    set<cgltf_material*> removedMaterials;
    for (cgltf_size i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material* material = &dataCGLTF->materials[i];
        if (!validMaterials.contains(material)) {
            removedMaterials.insert(material);
        }
    }

    // Remove any found invalid materials
    for (auto& i : removedMaterials) {
        removeMaterial(i);
    }

    // Check for duplicate materials
    map<cgltf_material*, cgltf_material*> materialDuplicates;
    for (cgltf_size i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material* material = &dataCGLTF->materials[i];
        for (cgltf_size j = i + 1; j < dataCGLTF->materials_count; ++j) {
            cgltf_material* material2 = &dataCGLTF->materials[j];
            if (*material == *material2) {
                materialDuplicates[material2] = material;
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
    for (size_t offset = 0; auto& i : materialDuplicates) {
        removeMaterial(i.first - offset, true);
        ++offset;
    }

    // TODO: optimise meshes
    return true;
}
