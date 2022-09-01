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

#include "Shared.h"

#include <iostream>

using namespace std;

string_view getCGLTFError(const cgltf_result result, const shared_ptr<cgltf_data>& data) noexcept
{
    switch (result) {
        case cgltf_result_file_not_found:
            return data ? "Resource not found"sv : "File not found"sv;
        case cgltf_result_io_error:
            return "I/O error"sv;
        case cgltf_result_invalid_json:
            return "Invalid JSON"sv;
        case cgltf_result_invalid_gltf:
            return "Invalid GLTF"sv;
        case cgltf_result_out_of_memory:
            return "Out of memory"sv;
        case cgltf_result_legacy_gltf:
            return "Legacy GLTF"sv;
        case cgltf_result_data_too_short:
            return data ? "Buffer too short"sv : "Unknown file type (not a GLTF file)"sv;
        case cgltf_result_unknown_format:
            return data ? "Unknown resource format"sv : "Unknown file type (not a GLTF file)"sv;
        default:
            return "Unknown error";
    }
}

bool requiresGLTFExtension(const shared_ptr<cgltf_data>& data, const string_view& name) noexcept
{
    for (size_t i = 0; i < data->extensions_required_count; ++i) {
        if (strcmp(data->extensions_required[i], name.data()) == 0)
            return true;
    }
    return false;
}

void printError(const std::string_view& message) noexcept
{
    cout << "Error: " << message << endl;
}

void printError(const std::string& message) noexcept
{
    cout << "Error: " << message << endl;
}

void printWarning(const std::string_view& message) noexcept
{
    cout << "Warning: " << message << endl;
}

void printWarning(const std::string& message) noexcept
{
    cout << "Warning: " << message << endl;
}

void printInfo(const std::string_view& message) noexcept
{
    cout << "Info: " << message << endl;
}

void printInfo(const std::string& message) noexcept
{
    cout << "Info: " << message << endl;
}
