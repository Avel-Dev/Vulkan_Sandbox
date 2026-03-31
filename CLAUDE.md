# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A minimal Vulkan renderer written in C++23. Renders a full-screen triangle with animated background using GLFW for windowing and the Vulkan graphics API.

## Build System

Uses CMake (minimum version 3.20). Debug build is configured by default with Vulkan validation layers enabled.

**Dependencies:**
- Vulkan SDK (with `glslc` shader compiler)
- GLFW3

**Build commands:**
```bash
# Configure (Debug by default)
cmake -B build

# Configure Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run
./build/VulkanApp
```

**Controls:** ESC or close window to exit. FPS is printed to console.

## Architecture

**Two-class design with clear separation of concerns:**

1. **`Application`** (`src/Application.h`, `src/Application.cpp`)
   - Owns the GLFW window and main loop
   - Handles window events (resize, key input)
   - Tracks and prints FPS
   - Delegates all rendering to `VulkanRenderer`

2. **`VulkanRenderer`** (`src/VulkanRenderer.h`, `src/VulkanRenderer.cpp`)
   - Encapsulates all Vulkan API usage
   - Manages the full Vulkan pipeline: instance → device → swapchain → pipeline → rendering
   - Uses double-buffering (`MAX_FRAMES_IN_FLIGHT = 2`)
   - Handles window resize via swapchain recreation

**Key Design Patterns:**
- **RAII**: Resource cleanup in destructors (non-movable/non-copyable classes)
- **`std::expected`**: Error handling instead of exceptions (C++23)
- **Structured bindings**: Used extensively with `std::expected` returns
- **`std::print`**: Modern C++23 output (not iostream)

**Error Types:**
- `AppError` for application-level failures
- `VulkanError` for Vulkan API failures (includes `VkResult`)

## Shader Compilation

Shaders are GLSL (`.vert`, `.frag`) compiled to SPIR-V (`.spv`) automatically during build:
- `shaders/triangle.vert` → Full-screen triangle (vertex positions in shader, no vertex buffer)
- `shaders/triangle.frag` → Outputs interpolated RGB colors

CMake finds `glslc` from `VULKAN_SDK` environment variable. Shader compilation is a build dependency of the main executable.

## Debugging

Debug builds enable Vulkan validation layers (`ENABLE_VALIDATION_LAYERS` macro defined). Validation errors are printed to stderr with `[VULKAN ERROR/WARNING/INFO]` prefixes.

If validation layers fail to load, ensure the Vulkan SDK is installed and `VULKAN_SDK` environment variable is set.
