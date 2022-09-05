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

#include "ImageOptimiser.h"

#include "Shared.h"

#include <ktx.h>
#include <map>
#include <set>
#include <stb_image.h>
#include <thread>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace std;

extern void cgltf_free_extensions(cgltf_data* data, cgltf_extension* extensions, cgltf_size extensions_count);

static void cgltf_remove_image(cgltf_data* data, cgltf_image* image)
{
    data->memory.free(data->memory.user_data, image->name);
    data->memory.free(data->memory.user_data, image->uri);
    data->memory.free(data->memory.user_data, image->mime_type);

    cgltf_free_extensions(data, image->extensions, image->extensions_count);
}

static void cgltf_remove_texture(cgltf_data* data, cgltf_texture* texture)
{
    data->memory.free(data->memory.user_data, texture->name);
    cgltf_free_extensions(data, texture->extensions, texture->extensions_count);
}

class TextureLoad
{
public:
    TextureLoad(const string& fileName) noexcept;

    TextureLoad(const TextureLoad& other, uint32_t channel) noexcept;

    TextureLoad() = delete;

    ~TextureLoad() noexcept = default;

    bool writeKTX(const string& fileName) noexcept;

    shared_ptr<uint8_t> data = nullptr;
    uint32_t imageWidth = 0;
    uint32_t imageHeight = 0;
    uint32_t channelCount = 0;
    uint32_t bytesPerChannel = 0;
    bool sRGB = false;
};

template<typename Func>
void runOverMaterialTextures(cgltf_material& material, Func function) noexcept
{
    if (material.has_pbr_metallic_roughness) {
        cgltf_pbr_metallic_roughness& materialPBR = material.pbr_metallic_roughness;
        function(materialPBR.base_color_texture.texture, true);
        function(materialPBR.metallic_roughness_texture.texture, false, true);
    }
    function(material.emissive_texture.texture, true);
    function(material.normal_texture.texture, false);
    function(material.occlusion_texture.texture, false);
    function(material.specular.specular_color_texture.texture, true);
    function(material.specular.specular_texture.texture, false);
    function(material.clearcoat.clearcoat_normal_texture.texture, false);
    function(material.clearcoat.clearcoat_roughness_texture.texture, false);
    function(material.sheen.sheen_color_texture.texture, true);
    function(material.sheen.sheen_roughness_texture.texture, false);
    function(material.transmission.transmission_texture.texture, false);
}

ImageOptimiser::ImageOptimiser(
    shared_ptr<cgltf_data>& data, const std::string& folder, bool keepOriginalTextures) noexcept
    : dataCGLTF(data)
    , rootFolder(folder)
    , keepTextures(keepOriginalTextures)
{
    if (!rootFolder.empty() && rootFolder.back() != '/' && rootFolder.back() != '\\') {
        rootFolder += '/';
    }
}

bool ImageOptimiser::passTextures() noexcept
{
    // Loop through and collect list of all valid images
    set<cgltf_image*> removedImages;
    set<cgltf_texture*> removedTextures;
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture& texture = dataCGLTF->textures[i];
        if (texture.image == nullptr && texture.basisu_image == nullptr) {
            removedTextures.insert(&texture);
            continue;
        }
        if (texture.image == nullptr) {
            // Doesn't contain a texture we can convert so just pass through the current one
            images[texture.basisu_image] = true;
            continue;
        }
        cgltf_image* image = texture.image;
        if (image->uri == nullptr) {
            removedImages.insert(image);
            removedTextures.insert(&texture);
            continue;
        }
        images[image] = false;
    }

    // Loop through all materials and check for unused textures
    set<cgltf_texture*> validTextures;
    for (size_t i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& material = dataCGLTF->materials[i];
        runOverMaterialTextures(material, [&](cgltf_texture*& p, bool, bool = false) {
            if (p != nullptr) {
                validTextures.insert(p);
            }
        });
    }
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture* texture = &dataCGLTF->textures[i];
        if (!validTextures.contains(texture)) {
            removedTextures.insert(texture);
        }
    }

    // Loop through all textures and check for unused images
    set<cgltf_image*> validImages;
    for (size_t i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture& texture = dataCGLTF->textures[i];
        if (texture.basisu_image != nullptr) {
            validImages.insert(texture.basisu_image);
        }
        if (texture.image != nullptr) {
            validImages.insert(texture.image);
        }
    }
    for (cgltf_size i = 0; i < dataCGLTF->images_count; ++i) {
        cgltf_image* image = &dataCGLTF->images[i];
        if (!validImages.contains(image)) {
            removedImages.insert(image);
        }
    }

    // Remove any found invalid images/textures
    for (auto& i : removedImages) {
        removeImage(i);
    }
    for (auto& i : removedTextures) {
        removeTexture(i);
    }

    // Convert all textures
    for (size_t i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& material = dataCGLTF->materials[i];
        runOverMaterialTextures(
            material, [&](cgltf_texture*& p, bool sRGB, bool split = false) { convertTexture(p, sRGB, split); });
    }
    return true;
}

void ImageOptimiser::removeImage(cgltf_image* image) noexcept
{
    // Remove the image from the images list
    cgltf_size i = 0;
    while (true) {
        cgltf_image* current = &dataCGLTF->images[i];
        if (current == image) {
            printWarning("Removed unused image: "s + ((current->name != nullptr) ? current->name : "unknown"));
            if (current->uri != nullptr) {
                // Delete file from disk
                string imageFile = rootFolder + image->uri;
                remove(imageFile.c_str());
            }
            // Remove image from gltf
            cgltf_remove_image(dataCGLTF.get(), current);
            memmove(current, current + 1, (dataCGLTF->images_count - i - 1) * sizeof(cgltf_image));
            --dataCGLTF->images_count;
            break;
        }
        if (++i >= dataCGLTF->images_count) {
            break;
        }
    }

    // Loop through all textures and set any matching pointers to null
    i = 0;
    while (true) {
        cgltf_texture* current = &dataCGLTF->textures[i];
        if (current->image == image) {
            current->image = nullptr;
        } else if (current->basisu_image == image) {
            current->basisu_image = nullptr;
        }
        // If texture has no valid images then remove it
        if (current->image == nullptr && current->basisu_image == nullptr) {
            removeTexture(current);
            --i;
        }
        if (++i >= dataCGLTF->textures_count) {
            break;
        }
    }
}

void ImageOptimiser::removeTexture(cgltf_texture* texture) noexcept
{
    // Remove the texture from the textures list
    cgltf_size i = 0;
    while (true) {
        cgltf_texture* current = &dataCGLTF->textures[i];
        if (current == texture) {
            printWarning("Removed unused texture: "s + ((current->name != nullptr) ? current->name : "unknown"));
            cgltf_remove_texture(dataCGLTF.get(), current);
            memmove(current, current + 1, (dataCGLTF->textures_count - i - 1) * sizeof(cgltf_texture));
            --dataCGLTF->textures_count;
            break;
        }
        if (++i >= dataCGLTF->textures_count) {
            break;
        }
    }

    // Loop through all materials and set any matching pointers to null
    for (i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& current = dataCGLTF->materials[i];
        runOverMaterialTextures(current, [&](cgltf_texture*& p, bool, bool = false) {
            if (p == texture) {
                p = nullptr;
            }
        });
    }
}

bool ImageOptimiser::convertTexture(cgltf_texture* texture, bool sRGB, bool split) noexcept
{
    // Check if has a valid image to convert
    if (texture == nullptr || (texture->image == nullptr && texture->basisu_image != nullptr)) {
        return true;
    }

    cgltf_image* image = texture->image;
    if (image == nullptr) {
        return false;
    }
    // Check if already converted
    if (images[image]) {
        return true;
    }

    // Load in existing texture
    if (image->uri == nullptr || strlen(image->uri) == 0) {
        return false;
    }
    string imageFile = rootFolder + image->uri;
    TextureLoad imageData(imageFile);
    if (imageData.data.get() == nullptr) {
        return false;
    }
    imageData.sRGB = sRGB;

    const size_t fileExt = imageFile.rfind('.');
    const string imageFileName = imageFile.substr(0, fileExt);

    // Convert
    if (split) {
        printInfo("Splitting texture: "s + imageFile);
        // Assumes we only want to split when metallicity/roughness
        if (imageData.channelCount != 2) {
            printError("Unexpected channel count when splitting texture '" + imageFile + "'");
        }

        // Split the files
        TextureLoad imageDataMetal(imageData, 0);
        const string metallicityFile = imageFileName + ".metallicity.ktx2";
        if (!imageDataMetal.writeKTX(metallicityFile)) {
            return false;
        }
        TextureLoad imageDataRough(imageData, 1);
        const string roughnessFile = imageFileName + ".roughness.ktx2";
        if (!imageDataRough.writeKTX(roughnessFile)) {
            return false;
        }
    } else {
        string fileName = imageFileName + ".ktx2";
        if (!imageData.writeKTX(fileName)) {
            return false;
        }
    }

    // Set as converted
    images[image] = true;

    // Update texture with new image
    cgltf_image* newImage = nullptr;
    if (keepTextures) {
        // Create new image
        auto newMemory = realloc(dataCGLTF->images, (dataCGLTF->images_count + 1) * sizeof(cgltf_image));
        if (newMemory == nullptr) {
            printError("Out of memory"sv);
            return false;
        }
        dataCGLTF->images = static_cast<cgltf_image*>(newMemory);
        newImage = &dataCGLTF->images[dataCGLTF->images_count];
        *newImage = {0};
        ++dataCGLTF->images_count;
    } else {
        // Reuse existing image allocation
        newImage = image;

        // Remove old texture file
        printInfo("Removing old texture: "s + imageFile);
        remove(imageFile.c_str());
        texture->image = nullptr;
    }
    string newFile = imageFileName.substr(rootFolder.length()) + ".ktx2";
    auto newMemory = realloc(newImage->uri, newFile.length() + 1);
    if (newMemory == nullptr) {
        printError("Out of memory"sv);
        return false;
    }
    newImage->uri = static_cast<char*>(newMemory);
    std::strcpy(newImage->uri, newFile.data());
    string_view mimeType = "image/ktx2"sv;
    newMemory = realloc(newImage->mime_type, mimeType.length() + 1);
    if (newMemory == nullptr) {
        printError("Out of memory"sv);
        return false;
    }
    newImage->mime_type = static_cast<char*>(newMemory);
    std::strcpy(newImage->mime_type, mimeType.data());
    texture->basisu_image = newImage;
    texture->has_basisu = true;

    return true;
}

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
