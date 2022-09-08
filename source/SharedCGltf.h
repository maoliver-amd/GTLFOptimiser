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
#pragma once

#include <cgltf.h>
#include <memory>
#include <string_view>

std::string_view getCGLTFError(const cgltf_result result, const std::shared_ptr<cgltf_data>& data) noexcept;

bool requiresGLTFExtension(const std::shared_ptr<cgltf_data>& data, const std::string_view& name) noexcept;

bool operator==(const cgltf_image& a, const cgltf_image& b) noexcept;

bool operator==(const cgltf_texture& a, const cgltf_texture& b) noexcept;

bool operator==(const cgltf_material& a, const cgltf_material& b) noexcept;

template<typename Func>
void runOverMaterialTextures(cgltf_material& material, Func function) noexcept
{
    if (material.has_pbr_metallic_roughness) {
        cgltf_pbr_metallic_roughness& materialPBR = material.pbr_metallic_roughness;
        function(materialPBR.base_color_texture.texture, true, false, false);
        function(materialPBR.metallic_roughness_texture.texture, false, false, true);
    }
    function(material.emissive_texture.texture, true, false, false);
    function(material.normal_texture.texture, false, true, false);
    function(material.occlusion_texture.texture, false, false, false);
    if (material.has_specular) {
        function(material.specular.specular_color_texture.texture, true, false, false);
        function(material.specular.specular_texture.texture, false, false, false);
    }
    if (material.has_clearcoat) {
        function(material.clearcoat.clearcoat_normal_texture.texture, false, true, false);
        function(material.clearcoat.clearcoat_roughness_texture.texture, false, false, false);
    }
    if (material.has_sheen) {
        function(material.sheen.sheen_color_texture.texture, true, false, false);
        function(material.sheen.sheen_roughness_texture.texture, false, false, false);
    }
    if (material.has_transmission) {
        function(material.transmission.transmission_texture.texture, false, false, false);
    }
    if (material.has_pbr_specular_glossiness) {
        function(material.pbr_specular_glossiness.diffuse_texture.texture, false, false, false);
        function(material.pbr_specular_glossiness.specular_glossiness_texture.texture, false, false, false);
    }
    if (material.has_volume) {
        function(material.volume.thickness_texture.texture, false, false, false);
    }
    if (material.has_iridescence) {
        function(material.iridescence.iridescence_texture.texture, false, false, false);
        function(material.iridescence.iridescence_thickness_texture.texture, false, false, false);
    }
}

extern void cgltf_free_extensions(cgltf_data* data, cgltf_extension* extensions, cgltf_size extensions_count);

void cgltf_remove_mesh(cgltf_data* data, cgltf_mesh* mesh) noexcept;

void cgltf_remove_material(cgltf_data* data, cgltf_material* material) noexcept;

void cgltf_remove_image(cgltf_data* data, cgltf_image* image) noexcept;

void cgltf_remove_texture(cgltf_data* data, cgltf_texture* texture) noexcept;
