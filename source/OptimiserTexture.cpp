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

#include <fstream>
#include <map>
#include <ranges>
#include <set>
#include <vector>

using namespace std;

bool Optimiser::passTextures() noexcept
{
    // Convert all textures
    set<cgltf_texture*> images;
    for (size_t i = 0; i < dataCGLTF->materials_count; ++i) {
        cgltf_material& material = dataCGLTF->materials[i];
        runOverMaterialTextures(material, [&](cgltf_texture*& p, bool sRGB, bool normalMap, bool split = false) {
            if (images.find(p) == images.end()) {
                convertTexture(p, sRGB, normalMap, split);
                images.insert(p);
            }
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

    // Get image file
    string imageFile = rootFolder + image->uri;
    const size_t fileExt = imageFile.rfind('.');
    const vector<pair<string_view, string_view>> substitutions = {{"%20", " "}, {"%21", "!"}, {"%23", "#"},
        {"%24", "$"}, {"%26", "&"}, {"%2B", "+"}, {"%2D", "-"}, {"%3D", "="}, {"%40", "@"}, {"%7E", "~"}};
    for (const auto& i : substitutions) {
        size_t pos = 0;
        string_view search = i.first;
        string_view replace = i.second;
        while ((pos = imageFile.find(search, pos)) != string::npos) {
            imageFile.replace(pos, search.length(), replace);
            pos += replace.length();
        }
    }
    const string imageFileName = imageFile.substr(0, fileExt);

    // Check for existing basisu texture
    if (texture->basisu_image != nullptr && !options.replaceCompressedTextures) {
        string fileName = imageFileName + ".ktx2";
        if (split && options.splitMetalRoughTextures) {
            // Check for split textures
            const string metallicityFile = imageFileName + ".metallicity.ktx2";
            const string roughnessFile = imageFileName + ".roughness.ktx2";
            if (ifstream(fileName).good() && ifstream(metallicityFile).good() && ifstream(roughnessFile).good()) {
                return true;
            }
        }
        if (ifstream(fileName).good()) {
            return true;
        }
    }

    // Load in existing texture
    if (image->uri == nullptr || strlen(image->uri) == 0) {
        return false;
    }

    // Run texture conversion in thread
    pool.push_task(
        [this](std::string imageFile, string imageFileName, bool sRGB, bool normalMap, bool split) {
            TextureLoad imageData(imageFile);
            if (imageData.data.get() == nullptr) {
                return false;
            }
            imageData.sRGB = sRGB;
            imageData.normalMap = normalMap;

            // Convert
            if (split && options.splitMetalRoughTextures) {
                const string metallicityFile = imageFileName + ".metallicity.ktx2";
                const string roughnessFile = imageFileName + ".roughness.ktx2";
                bool metalicityFound = !options.replaceCompressedTextures && ifstream(metallicityFile).good();
                bool roughnessFound = !options.replaceCompressedTextures && ifstream(roughnessFile).good();
                if (!metalicityFound || !roughnessFound) {
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
                        if (metalicityFound) {
                            printInfo("Using existing found metallicity texture '" + metallicityFile + "'");
                        } else if (!imageDataMetal.writeKTX(metallicityFile)) {
                            return false;
                        }
                    } else {
                        printWarning("Skipping output of redundant split metallicity texture '" + imageFile + "'");
                    }
                    TextureLoad imageDataRough(imageData, roughIndex);
                    if (imageDataRough.isUniqueTexture()) {
                        if (roughnessFound) {
                            printInfo("Using existing found roughness texture '" + roughnessFile + "'");
                        } else if (!imageDataRough.writeKTX(roughnessFile)) {
                            return false;
                        }
                    } else {
                        printWarning("Skipping output of redundant split roughness texture '" + imageFile + "'");
                    }
                } else {
                    printInfo("Using existing metallicity and roughness textures '" + metallicityFile + ", " +
                        roughnessFile + "'");
                }
            }
            string fileName = imageFileName + ".ktx2";
            if (!options.replaceCompressedTextures && ifstream(fileName).good()) {
                printInfo("Using existing found texture '" + fileName + "'");
            } else if (!imageData.writeKTX(fileName)) {
                return false;
            }
            return true;
        },
        imageFile, imageFileName, sRGB, normalMap, split);

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
            image = (image - dataCGLTF->images) + newMemory;
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
        const char* name = image->name != nullptr ? image->name : image->uri != nullptr ? image->uri : "";
        newImage->name = static_cast<char*>(malloc(strlen(name) + basisu.length() + 1));
        if (newImage->name != nullptr) {
            strcpy(newImage->name, name);
            strcat(newImage->name, basisu.data());
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
