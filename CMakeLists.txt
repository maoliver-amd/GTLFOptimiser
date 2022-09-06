cmake_minimum_required(VERSION 3.21)

# Detect vcpkg ports
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" ENV_VCPKG_ROOT)
    set(CMAKE_TOOLCHAIN_FILE "${ENV_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "")
endif()

project(GLTFOptimiser
    LANGUAGES CXX
    VERSION 1.0.0
    DESCRIPTION "A GLTF file optimiser"
)

# Default to a release build if desired configuration is not specified
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
        message(STATUS "Setting build type to 'Release' as none was specified.")
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "Release")
    endif()
endif()

# Set the install location to the source location if not currently specified
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "")
    message("Installing into source folder")
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install CACHE PATH "..." FORCE)
endif()

# Use folder structure for arranging files within IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(Ktx CONFIG REQUIRED)
find_path(CGLTF_INCLUDE_DIRS "cgltf.h")
#find_package(meshoptimizer CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h")
find_path(VULKAN_HEADERS_INCLUDE_DIRS "vulkan/vulkan_core.h")

add_executable(GLTFOptimiser)

configure_file(source/Version.h.in Version.h)

# Add in the executable code
target_sources(GLTFOptimiser PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/cgltf.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/stb.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Shared.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Shared.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/SharedCGLTF.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/SharedCGLTF.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/TextureLoad.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/TextureLoad.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Optimiser.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Optimiser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/OptimiserRemove.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/OptimiserTexture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/OptimiserMesh.cpp"
)

target_compile_features(GLTFOptimiser
    PRIVATE cxx_std_20
)

target_include_directories(GLTFOptimiser
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source/"
    "${PROJECT_BINARY_DIR}"
	"${CGLTF_INCLUDE_DIRS}"
    "${STB_INCLUDE_DIRS}"
    "${VULKAN_HEADERS_INCLUDE_DIRS}"
)

if(MSVC)
    target_compile_definitions(GLTFOptimiser PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(GLTFOptimiser PRIVATE /W4)
    if(CMAKE_BUILD_TYPE EQUAL "DEBUG")
        target_compile_options(GLTFOptimiser PRIVATE /fsanitize=address)
    endif()
endif()

target_link_libraries(GLTFOptimiser PRIVATE
    KTX::ktx
	#meshoptimizer::meshoptimizer
    CLI11::CLI11
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" REGULAR_EXPRESSION "*.h")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" REGULAR_EXPRESSION "*.cpp")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" REGULAR_EXPRESSION "*.qml")

include(InstallRequiredSystemLibraries)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
include(GNUInstallDirs)
install(TARGETS GLTFOptimiser
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
include(CPack)
