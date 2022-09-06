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
#include "TextureLoad.h"

#include <map>
#include <set>
#include <vector>

using namespace std;

bool Optimiser::passTextures() noexcept
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
        runOverMaterialTextures(material, [&](cgltf_texture*& p, bool, bool, bool = false) {
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

    // Check for duplicate images
    map<cgltf_image*, cgltf_image*> imageDuplicates;
    for (cgltf_size i = 0; i < dataCGLTF->images_count; ++i) {
        cgltf_image* image = &dataCGLTF->images[i];
        for (cgltf_size j = i + 1; j < dataCGLTF->images_count; ++j) {
            cgltf_image* image2 = &dataCGLTF->images[j];
            if (*image == *image2) {
                imageDuplicates[image2] = image;
            }
        }
    }
    // Update textures to remove duplicate images
    for (size_t i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture& texture = dataCGLTF->textures[i];
        if (auto pos = imageDuplicates.find(texture.image); pos != imageDuplicates.end()) {
            texture.image = pos->second;
        }
        if (auto pos = imageDuplicates.find(texture.basisu_image); pos != imageDuplicates.end()) {
            texture.image = pos->second;
        }
    }
    // Remove duplicate images
    for (size_t offset = 0; auto& i : imageDuplicates) {
        removeImage(i.first - offset, true);
        ++offset;
    }

    // Check for duplicate textures
    map<cgltf_texture*, cgltf_texture*> textureDuplicates;
    for (cgltf_size i = 0; i < dataCGLTF->textures_count; ++i) {
        cgltf_texture* texture = &dataCGLTF->textures[i];
        for (cgltf_size j = i + 1; j < dataCGLTF->textures_count; ++j) {
            cgltf_texture* texture2 = &dataCGLTF->textures[j];
            if (*texture == *texture2) {
                textureDuplicates[texture2] = texture;
            }
        }
    }
    // Update materials to remove duplicate textures
    for (size_t i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& material = dataCGLTF->materials[i];
        runOverMaterialTextures(material, [&](cgltf_texture*& p, bool, bool, bool = false) {
            if (auto pos = textureDuplicates.find(p); pos != textureDuplicates.end()) {
                p = pos->second;
            }
        });
    }
    // Remove duplicate textures
    for (size_t offset = 0; auto& i : textureDuplicates) {
        removeTexture(i.first - offset, true);
        ++offset;
    }

    // Convert all textures
    for (size_t i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& material = dataCGLTF->materials[i];
        runOverMaterialTextures(material, [&](cgltf_texture*& p, bool sRGB, bool normalMap, bool split = false) {
            convertTexture(p, sRGB, normalMap, split);
        });
    }
    return true;
}

bool Optimiser::convertTexture(cgltf_texture* texture, bool sRGB, bool normalMap, bool split) noexcept
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
    imageData.normalMap = normalMap;

    const size_t fileExt = imageFile.rfind('.');
    const string imageFileName = imageFile.substr(0, fileExt);

    // Convert
    if (split) {
        printInfo("Splitting texture: "s + imageFile);
        // Assumes we only want to split when metallicity/roughness
        uint32_t metalIndex = 2; // blue channel
        uint32_t roughIndex = 1; // green channel
        if (imageData.channelCount == 2) {
            metalIndex = 0;
            roughIndex = 1;
        } else if (imageData.channelCount != 3 && imageData.channelCount != 4) {
            printError("Unexpected channel count when splitting texture '" + imageFile + "'");
            return false;
        }

        // Split the files
        TextureLoad imageDataMetal(imageData, metalIndex);
        if (imageDataMetal.isUniqueTexture()) {
            const string metallicityFile = imageFileName + ".metallicity.ktx2";
            if (!imageDataMetal.writeKTX(metallicityFile)) {
                return false;
            }
        } else {
            printWarning("Skipping output of redundant split metallicity texture '" + imageFile + "'");
        }
        TextureLoad imageDataRough(imageData, roughIndex);
        if (imageDataRough.isUniqueTexture()) {
            const string roughnessFile = imageFileName + ".roughness.ktx2";
            if (!imageDataRough.writeKTX(roughnessFile)) {
                return false;
            }
        } else {
            printWarning("Skipping output of redundant split roughness texture '" + imageFile + "'");
        }
    }
    string fileName = imageFileName + ".ktx2";
    if (!imageData.writeKTX(fileName)) {
        return false;
    }

    // Set as converted
    images[image] = true;

    // Update texture with new image
    cgltf_image* newImage = nullptr;
    if (texture->basisu_image != nullptr) {
        // Reuse existing basisu image
        newImage = texture->basisu_image;
    } else if (options.keepOriginalTextures) {
        // Create new image
        auto newMemory =
            static_cast<cgltf_image*>(realloc(dataCGLTF->images, (dataCGLTF->images_count + 1) * sizeof(cgltf_image)));
        if (newMemory == nullptr) {
            printError("Out of memory"sv);
            return false;
        }
        // Check if pointers have moved
        if (newMemory != dataCGLTF->images) {
            // Fixup all image pointers
            for (size_t i = 0; i < dataCGLTF->textures_count; ++i) {
                cgltf_texture& current = dataCGLTF->textures[i];
                if (current.image != nullptr) {
                    current.image = (current.image - dataCGLTF->images) + newMemory;
                }
                if (current.basisu_image != nullptr) {
                    current.basisu_image = (current.basisu_image - dataCGLTF->images) + newMemory;
                }
            }
        }
        dataCGLTF->images = newMemory;
        newImage = &dataCGLTF->images[dataCGLTF->images_count];
        *newImage = {0};
        string_view basisu = "/basisu"sv;
        size_t nameLength = strlen(image->name);
        newImage->name = static_cast<char*>(malloc(nameLength + 1 + basisu.length()));
        if (newImage->name != nullptr) {
            memcpy(newImage->name, image->name, nameLength);
            memcpy(newImage->name + nameLength + 1, basisu.data(), basisu.length());
        }
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
