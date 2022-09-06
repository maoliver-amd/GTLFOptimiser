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

#include "SharedCGLTF.h"

#include <iostream>

using namespace std;

string_view getCGLTFError(const cgltf_result result, const shared_ptr<cgltf_data>& data) noexcept
{
    switch (result) {
        case cgltf_result_file_not_found:
            return data ? "Resource not found"sv : "File not found"sv;
        case cgltf_result_io_error:
            return "I/O error"sv;
        case cgltf_result_invalid_json:
            return "Invalid JSON"sv;
        case cgltf_result_invalid_gltf:
            return "Invalid GLTF"sv;
        case cgltf_result_out_of_memory:
            return "Out of memory"sv;
        case cgltf_result_legacy_gltf:
            return "Legacy GLTF"sv;
        case cgltf_result_data_too_short:
            return data ? "Buffer too short"sv : "Unknown file type (not a GLTF file)"sv;
        case cgltf_result_unknown_format:
            return data ? "Unknown resource format"sv : "Unknown file type (not a GLTF file)"sv;
        case cgltf_result_success:
            return "Success"sv;
        default:
            return "Unknown error";
    }
}

bool requiresGLTFExtension(const shared_ptr<cgltf_data>& data, const string_view& name) noexcept
{
    for (size_t i = 0; i < data->extensions_required_count; ++i) {
        if (strcmp(data->extensions_required[i], name.data()) == 0)
            return true;
    }
    return false;
}

bool operator==(const cgltf_image& a, const cgltf_image& b) noexcept
{
    if (a.uri == b.uri && a.uri != nullptr) {
        return true;
    } else if (a.buffer_view == b.buffer_view && a.buffer_view != nullptr) {
        return true;
    }
    return false;
}

bool operator==(const cgltf_texture& a, const cgltf_texture& b) noexcept
{
    if (((a.image == b.image && a.image != nullptr) || a.basisu_image == b.basisu_image && a.basisu_image != nullptr) &&
        a.sampler == b.sampler) {
        return true;
    }
    return false;
}

bool operator==(const cgltf_material& a, const cgltf_material& b) noexcept
{
    if (memcmp(&a.pbr_metallic_roughness, &b.pbr_metallic_roughness, sizeof(cgltf_pbr_metallic_roughness)) == 0 &&
        memcmp(&a.pbr_specular_glossiness, &b.pbr_specular_glossiness, sizeof(cgltf_pbr_specular_glossiness)) == 0 &&
        memcmp(&a.clearcoat, &b.clearcoat, sizeof(cgltf_clearcoat)) == 0 &&
        memcmp(&a.ior, &b.ior, sizeof(cgltf_ior)) == 0 &&
        memcmp(&a.specular, &b.specular, sizeof(cgltf_specular)) == 0 &&
        memcmp(&a.sheen, &b.sheen, sizeof(cgltf_sheen)) == 0 &&
        memcmp(&a.transmission, &b.transmission, sizeof(cgltf_transmission)) == 0 &&
        memcmp(&a.volume, &b.volume, sizeof(cgltf_volume)) == 0 &&
        memcmp(&a.normal_texture, &b.normal_texture, sizeof(cgltf_texture_view)) == 0 &&
        memcmp(&a.occlusion_texture, &b.occlusion_texture, sizeof(cgltf_texture_view)) == 0 &&
        memcmp(&a.emissive_texture, &b.emissive_texture, sizeof(cgltf_texture_view)) == 0 &&
        memcmp(&a.emissive_factor, &b.emissive_factor, sizeof(cgltf_float) * 3) == 0 &&
        memcmp(&a.alpha_mode, &b.alpha_mode, sizeof(cgltf_alpha_mode)) == 0 &&
        memcmp(&a.alpha_cutoff, &b.alpha_cutoff, sizeof(cgltf_float)) == 0 &&
        memcmp(&a.double_sided, &b.double_sided, sizeof(cgltf_bool)) == 0 &&
        memcmp(&a.unlit, &b.unlit, sizeof(cgltf_bool)) == 0) {
        return true;
    }
    return false;
}

void cgltf_remove_mesh(cgltf_data* data, cgltf_mesh* mesh) noexcept
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

void cgltf_remove_material(cgltf_data* data, cgltf_material* material) noexcept
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

void cgltf_remove_image(cgltf_data* data, cgltf_image* image) noexcept
{
    data->memory.free(data->memory.user_data, image->name);
    data->memory.free(data->memory.user_data, image->uri);
    data->memory.free(data->memory.user_data, image->mime_type);

    cgltf_free_extensions(data, image->extensions, image->extensions_count);
}

void cgltf_remove_texture(cgltf_data* data, cgltf_texture* texture) noexcept
{
    data->memory.free(data->memory.user_data, texture->name);
    cgltf_free_extensions(data, texture->extensions, texture->extensions_count);
}
