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
#include "MeshOptimiser.h"
#include "Shared.h"
#include "Version.h"

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <cgltf.h>
#include <cgltf_write.h>

using namespace std;

int main(int argc, char* argv[])
{
    // Pass command line arguments
    CLI::App app{"GLTF file optimiser"};
    app.set_version_flag("--version", std::string(SIG_VERSION_STR));
    string inputFile;
    app.add_option("-i,--input", inputFile, "The input GLTF file")->required();
    string outputFile;
    app.add_option("-o,--output", outputFile, "The output GLTF file")->default_str(inputFile);
    bool keepTextures = false;
    app.add_flag("--kt", keepTextures, "Keep original uncompressed textures");
    CLI11_PARSE(app, argc, argv);

    // Open the GLTF file
    printInfo("Opening input gltf file: "s + inputFile);
    cgltf_options options = {};
    cgltf_result result = cgltf_result_success;
    shared_ptr<cgltf_data> dataCGLTF(
        [&]() {
            cgltf_data* data;
            result = cgltf_parse_file(&options, inputFile.c_str(), &data);
            return data;
        }(),
        [](auto p) { cgltf_free(p); });
    if (result != cgltf_result_success) {
        printError(getCGLTFError(result, dataCGLTF));
        return 1;
    }
    result = cgltf_load_buffers(&options, dataCGLTF.get(), inputFile.c_str());
    if (result != cgltf_result_success) {
        printError(getCGLTFError(result, dataCGLTF));
        return 1;
    }
    result = cgltf_validate(dataCGLTF.get());
    if (result != cgltf_result_success) {
        printError(getCGLTFError(result, dataCGLTF));
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

    // Get asset file location
    size_t folderPos = inputFile.find_last_of("/\\");
    folderPos = inputFile.find_last_not_of("/\\", folderPos);
    std::string imageFolder = (folderPos != string::npos ? std::string(inputFile, 0, folderPos + 1) : inputFile);

    // TODO: optionally strip material names, mesh names, camera names etc.

    // Optimise meshes
    MeshOptimiser meshOpt(dataCGLTF, imageFolder);
    if (!meshOpt.passMeshes()) {
        return 1;
    }

    // Optimise images
    ImageOptimiser imageOpt(dataCGLTF, imageFolder, keepTextures);
    if (!imageOpt.passTextures()) {
        return 1;
    }

    // Write out updated gltf
    printInfo("Writing output gltf file: "s + outputFile);
    string_view generator = "GLTFOptimiser"sv;
    auto newMem = realloc(dataCGLTF->asset.generator, generator.size() + 1);
    if (newMem == nullptr) {
        printError("Out of memory"sv);
        return false;
    }
    dataCGLTF->asset.generator = static_cast<char*>(newMem);
    std::strcpy(dataCGLTF->asset.generator, generator.data());
    result = cgltf_write_file(&options, outputFile.c_str(), dataCGLTF.get());
    if (result != cgltf_result_success) {
        printError(getCGLTFError(result, dataCGLTF));
        return 1;
    }

    return 0;
}
