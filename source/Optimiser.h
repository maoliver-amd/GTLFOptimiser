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

#include "BS_thread_pool.hpp"

#include <cgltf.h>
#include <memory>
#include <string>

class Optimiser
{
public:
    struct Options
    {
        bool keepOriginalTextures = false;
        bool replaceCompressedTextures = false;
        bool searchCompressedTextures = false;
        bool splitMetalRoughTextures = false;
    };

    Optimiser(const Options& opts) noexcept;

    [[nodiscard]] bool pass(const std::string& inputFile, const std::string& outputFile) noexcept;

private:
    void checkInvalidImages() noexcept;

    void checkInvalidTextures() noexcept;

    void checkInvalidMaterials() noexcept;

    void checkInvalidMeshes() noexcept;

    void passInvalid() noexcept;

    void checkUnusedImages() noexcept;

    void checkUnusedTextures() noexcept;

    void checkUnusedMaterials() noexcept;

    void checkUnusedMeshes() noexcept;

    void passUnused() noexcept;

    void checkDuplicateImages() noexcept;

    void checkDuplicateTextures() noexcept;

    void checkDuplicateMaterials() noexcept;

    void checkDuplicateMeshes() noexcept;

    void passDuplicate() noexcept;

    [[nodiscard]] bool passTextures() noexcept;

    [[nodiscard]] bool passMeshes() noexcept;

    void removeImage(cgltf_image* image) noexcept;

    void removeTexture(cgltf_texture* texture) noexcept;

    void removeMaterial(cgltf_material* material) noexcept;

    void removeMesh(cgltf_mesh* mesh) noexcept;

    bool convertTexture(cgltf_texture* texture, bool sRGB, bool normalMap, bool split = false) noexcept;

    std::string rootFolder;
    std::shared_ptr<cgltf_data> dataCGLTF = nullptr;
    Options options;
    BS::thread_pool pool;
};
