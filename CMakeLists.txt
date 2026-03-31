cmake_minimum_required(VERSION 3.20)
project(VulkanApp VERSION 1.0.0 LANGUAGES CXX)

# C++23 Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Debug build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Find packages
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

# Define source files
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/VulkanRenderer.cpp
)

set(HEADERS
    src/Application.h
    src/VulkanRenderer.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
)

# Debug configuration: enable validation layers
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:ENABLE_VALIDATION_LAYERS>
    $<$<CONFIG:Debug>:DEBUG>
)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3>
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2>
    )
endif()

# Shader compilation
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SPIRV_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SPIRV_DIR})

# Find glslc
find_program(GLSLC_EXECUTABLE glslc PATHS
    $ENV{VULKAN_SDK}/bin
    $ENV{VULKAN_SDK}/Bin
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Please install Vulkan SDK.")
endif()

set(SHADER_FILES
    ${SHADER_DIR}/triangle.vert
    ${SHADER_DIR}/triangle.frag
)

set(SPIRV_FILES)

foreach(SHADER ${SHADER_FILES})
    get_filename_component(FILENAME ${SHADER} NAME)
    set(SPIRV_FILE ${SPIRV_DIR}/${FILENAME}.spv)
    list(APPEND SPIRV_FILES ${SPIRV_FILE})

    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${FILENAME}"
    )
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPIRV_FILES})
add_dependencies(${PROJECT_NAME} shaders)

# Copy shaders to build directory for runtime
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADER_DIR="${SPIRV_DIR}"
)

# Output build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Vulkan SDK: ${Vulkan_VERSION}")
message(STATUS "GLSLC: ${GLSLC_EXECUTABLE}")
