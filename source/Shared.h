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
#include <string_view>

void printError(const std::string_view& message) noexcept;

void printError(const std::string& message) noexcept;

void printWarning(const std::string_view& message) noexcept;

void printWarning(const std::string& message) noexcept;

void printInfo(const std::string_view& message) noexcept;

void printInfo(const std::string& message) noexcept;
