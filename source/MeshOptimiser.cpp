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

#include "MeshOptimiser.h"

#include "Shared.h"

#include <map>
#include <set>

using namespace std;

extern void cgltf_free_extensions(cgltf_data* data, cgltf_extension* extensions, cgltf_size extensions_count);

static void cgltf_remove_mesh(cgltf_data* data, cgltf_mesh* mesh)
{
    data->memory.free(data->memory.user_data, mesh->name);

    for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
        for (cgltf_size k = 0; k < mesh->primitives[j].attributes_count; ++k) {
            data->memory.free(data->memory.user_data, mesh->primitives[j].attributes[k].name);
        }

        data->memory.free(data->memory.user_data, mesh->primitives[j].attributes);

        for (cgltf_size k = 0; k < mesh->primitives[j].targets_count; ++k) {
            for (cgltf_size m = 0; m < mesh->primitives[j].targets[k].attributes_count; ++m) {
                data->memory.free(data->memory.user_data, mesh->primitives[j].targets[k].attributes[m].name);
            }

            data->memory.free(data->memory.user_data, mesh->primitives[j].targets[k].attributes);
        }

        data->memory.free(data->memory.user_data, mesh->primitives[j].targets);

        if (mesh->primitives[j].has_draco_mesh_compression) {
            for (cgltf_size k = 0; k < mesh->primitives[j].draco_mesh_compression.attributes_count; ++k) {
                data->memory.free(
                    data->memory.user_data, mesh->primitives[j].draco_mesh_compression.attributes[k].name);
            }

            data->memory.free(data->memory.user_data, mesh->primitives[j].draco_mesh_compression.attributes);
        }

        data->memory.free(data->memory.user_data, mesh->primitives[j].mappings);

        cgltf_free_extensions(data, mesh->primitives[j].extensions, mesh->primitives[j].extensions_count);
    }

    data->memory.free(data->memory.user_data, mesh->primitives);
    data->memory.free(data->memory.user_data, mesh->weights);

    for (cgltf_size j = 0; j < mesh->target_names_count; ++j) {
        data->memory.free(data->memory.user_data, mesh->target_names[j]);
    }

    cgltf_free_extensions(data, mesh->extensions, mesh->extensions_count);

    data->memory.free(data->memory.user_data, mesh->target_names);
}

static void cgltf_remove_material(cgltf_data* data, cgltf_material* material)
{
    data->memory.free(data->memory.user_data, material->name);

    if (material->has_pbr_metallic_roughness) {
        cgltf_free_extensions(data, material->pbr_metallic_roughness.metallic_roughness_texture.extensions,
            material->pbr_metallic_roughness.metallic_roughness_texture.extensions_count);
        cgltf_free_extensions(data, material->pbr_metallic_roughness.base_color_texture.extensions,
            material->pbr_metallic_roughness.base_color_texture.extensions_count);
    }
    if (material->has_pbr_specular_glossiness) {
        cgltf_free_extensions(data, material->pbr_specular_glossiness.diffuse_texture.extensions,
            material->pbr_specular_glossiness.diffuse_texture.extensions_count);
        cgltf_free_extensions(data, material->pbr_specular_glossiness.specular_glossiness_texture.extensions,
            material->pbr_specular_glossiness.specular_glossiness_texture.extensions_count);
    }
    if (material->has_clearcoat) {
        cgltf_free_extensions(data, material->clearcoat.clearcoat_texture.extensions,
            material->clearcoat.clearcoat_texture.extensions_count);
        cgltf_free_extensions(data, material->clearcoat.clearcoat_roughness_texture.extensions,
            material->clearcoat.clearcoat_roughness_texture.extensions_count);
        cgltf_free_extensions(data, material->clearcoat.clearcoat_normal_texture.extensions,
            material->clearcoat.clearcoat_normal_texture.extensions_count);
    }
    if (material->has_specular) {
        cgltf_free_extensions(
            data, material->specular.specular_texture.extensions, material->specular.specular_texture.extensions_count);
        cgltf_free_extensions(data, material->specular.specular_color_texture.extensions,
            material->specular.specular_color_texture.extensions_count);
    }
    if (material->has_transmission) {
        cgltf_free_extensions(data, material->transmission.transmission_texture.extensions,
            material->transmission.transmission_texture.extensions_count);
    }
    if (material->has_volume) {
        cgltf_free_extensions(
            data, material->volume.thickness_texture.extensions, material->volume.thickness_texture.extensions_count);
    }
    if (material->has_sheen) {
        cgltf_free_extensions(
            data, material->sheen.sheen_color_texture.extensions, material->sheen.sheen_color_texture.extensions_count);
        cgltf_free_extensions(data, material->sheen.sheen_roughness_texture.extensions,
            material->sheen.sheen_roughness_texture.extensions_count);
    }

    cgltf_free_extensions(data, material->normal_texture.extensions, material->normal_texture.extensions_count);
    cgltf_free_extensions(data, material->occlusion_texture.extensions, material->occlusion_texture.extensions_count);
    cgltf_free_extensions(data, material->emissive_texture.extensions, material->emissive_texture.extensions_count);

    cgltf_free_extensions(data, material->extensions, material->extensions_count);
}

MeshOptimiser::MeshOptimiser(shared_ptr<cgltf_data>& data, const std::string& folder) noexcept
    : dataCGLTF(data)
    , rootFolder(folder)
{
    if (!rootFolder.empty() && rootFolder.back() != '/' && rootFolder.back() != '\\') {
        rootFolder += '/';
    }
}

bool MeshOptimiser::passMeshes() noexcept
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
            cgltf_primitive& prim = mesh.primitives[i];
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

    // TODO: optimise meshes
    return true;
}

void MeshOptimiser::removeMesh(cgltf_mesh* mesh) noexcept
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
        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive& current = mesh->primitives[j];
            if (auto pos = orphanedMaterials.find(current.material); pos != orphanedMaterials.end()) {
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
    for (cgltf_size i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh* current = &dataCGLTF->meshes[i];
        if (current == mesh) {
            printWarning("Removed unused mesh: "s + ((current->name != nullptr) ? current->name : "unknown"));
            cgltf_remove_mesh(dataCGLTF.get(), current);
            memmove(current, current + 1, (dataCGLTF->meshes_count - i - 1) * sizeof(cgltf_image));
            --dataCGLTF->meshes_count;
            break;
        }
    }
}

void MeshOptimiser::removeMaterial(cgltf_material* material) noexcept
{
    // Remove the material from the materials list
    for (cgltf_size i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material* current = &dataCGLTF->materials[i];
        if (current == material) {
            printWarning("Removed unused material: "s + ((current->name != nullptr) ? current->name : "unknown"));
            cgltf_remove_material(dataCGLTF.get(), current);
            memmove(current, current + 1, (dataCGLTF->materials_count - i - 1) * sizeof(cgltf_texture));
            --dataCGLTF->materials_count;
            break;
        }
    }

    // Loop through all primitives and set any matching pointers to null
    for (cgltf_size i = 0; i < dataCGLTF->meshes_count; ++i) {
        cgltf_mesh& mesh = dataCGLTF->meshes[i];
        for (cgltf_size j = 0; j < mesh.primitives_count; ++j) {
            cgltf_primitive& prim = mesh.primitives[j];
            if (prim.material == material) {
                prim.material = nullptr;
            }
        }
    }
}
