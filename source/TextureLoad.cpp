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

#include "TextureLoad.h"

#include "Shared.h"

#include <ktx.h>
#include <stb_image.h>
#include <thread>
#include <vulkan/vulkan_core.h>

using namespace std;

TextureLoad::TextureLoad(const string& fileName) noexcept
{
    // Load in data from texture file
    uint8_t* imageData = nullptr;
    bytesPerChannel = 2;
    int32_t width = 0;
    int32_t height = 0;
    int32_t channels = 0;
    if (stbi_is_16_bit(fileName.data()))
        imageData = reinterpret_cast<uint8_t*>(stbi_load_16(fileName.data(), &width, &height, &channels, 0));
    if (imageData == nullptr) {
        imageData = stbi_load(fileName.data(), &width, &height, &channels, 0);
        bytesPerChannel = 1;
    }
    if (imageData == nullptr) {
        printError("Unable to load image '"s + fileName + "': " + stbi_failure_reason());
        return;
    }
    imageWidth = static_cast<uint32_t>(width);
    imageHeight = static_cast<uint32_t>(height);
    channelCount = static_cast<uint32_t>(channels);
    data = shared_ptr<uint8_t>(imageData, [](auto p) { stbi_image_free(p); });
}

TextureLoad::TextureLoad(const TextureLoad& other, uint32_t channel) noexcept
{
    // Create a new object by splitting out the texture channels
    const size_t imageSize = (size_t)other.imageWidth * other.imageHeight * other.bytesPerChannel;
    data = shared_ptr<uint8_t>(static_cast<stbi_uc*>(malloc(imageSize)), [](auto p) { stbi_image_free(p); });
    imageWidth = other.imageWidth;
    imageHeight = other.imageHeight;
    channelCount = other.channelCount;
    bytesPerChannel = other.bytesPerChannel;
    if (other.bytesPerChannel == 1) {
        uint8_t* sourceData = other.data.get();
        for (size_t y = 0; y < imageHeight; ++y) {
            for (size_t x = 0; x < imageWidth; ++x) {
                for (size_t k = 0; k < channelCount; ++k) {
                    if (k == channel) {
                        const size_t destIndex = (x + y * imageWidth);
                        const size_t sourceIndex = channelCount * (x + y * imageWidth) + k;
                        const uint8_t source = sourceData[sourceIndex];
                        data.get()[destIndex] = source;
                    }
                }
            }
        }
    } else {
        uint16_t* sourceData = (uint16_t*)other.data.get();
        for (size_t y = 0; y < imageHeight; ++y) {
            for (size_t x = 0; x < imageWidth; ++x) {
                for (size_t k = 0; k < channelCount; ++k) {
                    if (k == channel) {
                        const size_t destIndex = (x + y * imageWidth);
                        const size_t sourceIndex = other.channelCount * (x + y * imageWidth) + k;
                        const uint16_t source = sourceData[sourceIndex];
                        reinterpret_cast<uint16_t*>(data.get())[destIndex] = source;
                    }
                }
            }
        }
    }
}

bool TextureLoad::writeKTX(const string& fileName) noexcept
{
    printInfo("Writing compressed texture: "s + fileName);

    // Initialise data describing the new texture
    ktxTextureCreateInfo createInfo = {0};
    createInfo.glInternalformat = 0;
    if (channelCount == 1) {
        createInfo.vkFormat = (bytesPerChannel == 1) ? VK_FORMAT_R8_UNORM : VK_FORMAT_R16_UNORM;
    } else if (channelCount == 2) {
        createInfo.vkFormat = (bytesPerChannel == 1) ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R16G16_UNORM;
    } else if (channelCount == 3) {
        if (sRGB) {
            createInfo.vkFormat = (bytesPerChannel == 1) ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R16G16B16A16_UNORM;
        } else {
            createInfo.vkFormat = (bytesPerChannel == 1) ? VK_FORMAT_R8G8B8_UNORM : VK_FORMAT_R16G16B16_UNORM;
        }
    } else if (channelCount == 4) {
        if (sRGB) {
            createInfo.vkFormat = (bytesPerChannel == 1) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R16G16B16A16_UNORM;
        } else {
            createInfo.vkFormat = (bytesPerChannel == 1) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R16G16B16A16_UNORM;
        }
    }
    createInfo.baseWidth = imageWidth;
    createInfo.baseHeight = imageHeight;
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = 1;
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_TRUE;

    KTX_error_code result = KTX_SUCCESS;
    shared_ptr<ktxTexture2> texture(
        [&]() {
            ktxTexture2* data;
            result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &data);
            return data;
        }(),
        [](auto p) { ktxTexture_Destroy(ktxTexture(p)); });
    ;
    if (result != KTX_SUCCESS) {
        printError("Failed creating ktx texture '"s + fileName + "'" + ": " + ktxErrorString(result));
        return false;
    }

    // Copy across existing image data into ktx texture
    size_t const imageSize = (size_t)imageWidth * imageHeight * channelCount * bytesPerChannel;
    result = ktxTexture_SetImageFromMemory((ktxTexture*)texture.get(), 0, 0, 0, data.get(), imageSize);
    if (result != KTX_SUCCESS) {
        printError("Failed initialising ktx texture '"s + fileName + "'" + ": " + ktxErrorString(result));
        return false;
    }

    // Apply basisu compression on the texture
    ktxBasisParams params = {0};
    params.structSize = sizeof(params);
    params.uastc = KTX_TRUE;
    params.uastcFlags = KTX_PACK_UASTC_MAX_LEVEL;
    params.uastcRDO = true;
    params.uastcRDOQualityScalar = 1.0f;
    params.threadCount = thread::hardware_concurrency();
    result = ktxTexture2_CompressBasisEx(texture.get(), &params);
    if (result != KTX_SUCCESS) {
        printError("Failed encoding ktx texture '"s + fileName + "'" + ": " + ktxErrorString(result));
        return false;
    }

    // Apply zstd supercompression
    result = ktxTexture2_DeflateZstd(texture.get(), 22);
    if (result != KTX_SUCCESS) {
        printError("Failed compressing ktx texture '"s + fileName + "'" + ": " + ktxErrorString(result));
        return false;
    }

    // Write out to disk
    result = ktxTexture_WriteToNamedFile((ktxTexture*)texture.get(), fileName.c_str());
    if (result != KTX_SUCCESS) {
        printError("Failed writing ktx texture '"s + fileName + "'" + ": " + ktxErrorString(result));
        return false;
    }
    return true;
}
