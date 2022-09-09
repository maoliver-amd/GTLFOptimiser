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
#include "Version.h"

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>

using namespace std;

int main(int argc, char* argv[])
{
    // Pass command line arguments
    CLI::App app{"GLTF file optimiser"};
    app.set_version_flag("--version", std::string(SIG_VERSION_STR));
    string inputFile;
    app.add_option("-i,--input", inputFile, "The input GLTF file")->required();
    string outputFile;
    app.add_option("-o,--output", outputFile, "The output GLTF file (defaults to input file)")->default_str(inputFile);
    bool keepTextures = false;
    app.add_flag("-k,--keep-uncompressed-textures", keepTextures, "Keep original uncompressed textures")->default_val(false);
    bool regenCompressed = false;
    app.add_flag(
           "-r,--replace-compressed-textures", keepTextures, "Recreate and replace any existing compressed textures")
        ->default_val(false);
    CLI11_PARSE(app, argc, argv);
    if (outputFile.empty()) {
        outputFile = inputFile;
    }

    // Optimise meshes
    Optimiser::Options opts;
    opts.keepOriginalTextures = keepTextures;
    opts.replaceCompressedTextures = regenCompressed;
    Optimiser opt(opts);

    if (!opt.pass(inputFile, outputFile)) {
        return 1;
    }

    return 0;
}
