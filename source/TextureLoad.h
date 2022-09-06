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

#include <memory>
#include <string>

class TextureLoad
{
public:
    TextureLoad(const std::string& fileName) noexcept;

    TextureLoad(const TextureLoad& other, uint32_t channel) noexcept;

    TextureLoad() = delete;

    ~TextureLoad() noexcept = default;

    bool writeKTX(const std::string& fileName) noexcept;

    bool isUniqueTexture() noexcept;

    void convertTo8bit() noexcept;

    std::shared_ptr<uint8_t> data = nullptr;
    uint32_t imageWidth = 0;
    uint32_t imageHeight = 0;
    uint32_t channelCount = 0;
    uint32_t bytesPerChannel = 0;
    bool sRGB = false;
    bool normalMap = false;
};
