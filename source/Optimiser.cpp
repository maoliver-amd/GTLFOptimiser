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
#include "Version.h"

#include <cgltf.h>
#include <cgltf_write.h>
#include <map>
#include <set>
#include <vector>

using namespace std;

Optimiser::Optimiser(const Options& opts) noexcept
    : options(opts)
{}

bool Optimiser::pass(const std::string& inputFile, const std::string& outputFile) noexcept
{
    // Get asset file location
    size_t folderPos = inputFile.find_last_of("/\\");
    folderPos = inputFile.find_last_not_of("/\\", folderPos);
    rootFolder = (folderPos != string::npos ? std::string(inputFile, 0, folderPos + 1) : inputFile);

    if (!rootFolder.empty() && rootFolder.back() != '/' && rootFolder.back() != '\\') {
        rootFolder += '/';
    }
    // Open the GLTF file
    printInfo("Opening input gltf file: "s + inputFile);
    cgltf_options optionsCGLTF = {};
    cgltf_result result = cgltf_result_success;
    dataCGLTF = shared_ptr<cgltf_data>(
        [&]() {
            cgltf_data* data;
            result = cgltf_parse_file(&optionsCGLTF, inputFile.c_str(), &data);
            return data;
        }(),
        [](auto p) { cgltf_free(p); });
    if (result != cgltf_result_success) {
        printError("Failed to parse input file: "s + getCGLTFError(result, dataCGLTF));
        return 1;
    }
    result = cgltf_load_buffers(&optionsCGLTF, dataCGLTF.get(), inputFile.c_str());
    if (result != cgltf_result_success) {
        printError("Failed to load input file buffers: "s + getCGLTFError(result, dataCGLTF));
        return 1;
    }
    result = cgltf_validate(dataCGLTF.get());
    if (result != cgltf_result_success) {
        printError("Invalid input file detected: "s + getCGLTFError(result, dataCGLTF));
        return 1;
    }

    // Check for unusable extensions
    if (requiresGLTFExtension(dataCGLTF, "KHR_draco_mesh_compression")) {
        printError("Draco mesh compressions is not supported (input file uses KHR_draco_mesh_compression)"sv);
        return 1;
    }
    if (requiresGLTFExtension(dataCGLTF, "EXT_mesh_gpu_instancing")) {
        printError("Mesh instancing is not supported (input file uses EXT_mesh_gpu_instancing)"sv);
        return 1;
    }

    // TODO: optionally strip material names, mesh names, camera names etc.

    // Remove invalid objects
    passInvalid();

    // Check for unused objects
    passUnused();

    // Check for duplicate objects
    passDuplicate();

    // Optimise meshes
    auto checkMeshes = pool.submit(&Optimiser::passMeshes, this);

    // Wait for thread pool to complete all jobs before continuing
    pool.wait_for_tasks();

    if (!checkMeshes.get()) {
        return false;
    }

    // Optimise images
    auto checkTextures = pool.submit(&Optimiser::passTextures, this);

    // Wait for thread pool to complete all jobs
    pool.wait_for_tasks();

    if (!checkTextures.get()) {
        return false;
    }

    // Write out updated gltf
    printInfo("Writing output gltf file: "s + outputFile);
    string_view generator = "GLTFOptimiser (" SIG_VERSION_STR ")";
    auto newMem = realloc(dataCGLTF->asset.generator, generator.size() + 1);
    if (newMem == nullptr) {
        printError("Out of memory"sv);
        return false;
    }
    dataCGLTF->asset.generator = static_cast<char*>(newMem);
    std::strcpy(dataCGLTF->asset.generator, generator.data());
    // Validate output file
    result = cgltf_validate(dataCGLTF.get());
    if (result != cgltf_result_success) {
        printError("Invalid output file detected: "s + getCGLTFError(result, dataCGLTF));
        return 1;
    }
    result = cgltf_write_file(&optionsCGLTF, outputFile.c_str(), dataCGLTF.get());
    if (result != cgltf_result_success) {
        printError("Failed writing output file: "s + getCGLTFError(result, dataCGLTF));
        return false;
    }

    return true;
}
