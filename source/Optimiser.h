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
#include <map>
#include <memory>
#include <string>

class Optimiser
{
public:
    struct Options
    {
        bool keepOriginalTextures = false;
    };

    Optimiser(const Options& opts) noexcept;

    [[nodiscard]] bool pass(const std::string& inputFile, const std::string& outputFile) noexcept;

private:
    [[nodiscard]] bool passTextures() noexcept;

    [[nodiscard]] bool passMeshes() noexcept;

    void removeImage(cgltf_image* image, bool print = true) noexcept;

    void removeTexture(cgltf_texture* texture, bool print = true) noexcept;

    void removeMesh(cgltf_mesh* mesh, bool print = true) noexcept;

    void removeMaterial(cgltf_material* material, bool print = true) noexcept;

    bool convertTexture(cgltf_texture* texture, bool sRGB, bool normalMap, bool split = false) noexcept;

    std::string rootFolder;
    std::shared_ptr<cgltf_data> dataCGLTF = nullptr;
    std::map<cgltf_image*, bool> images;
    Options options;
};
