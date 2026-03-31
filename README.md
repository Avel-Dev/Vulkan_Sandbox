# Vulkan Minimal Renderer

A modern, minimal Vulkan renderer written in C++23. Renders an animated full-screen triangle using the Vulkan graphics API and GLFW for cross-platform windowing.

![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red)
![License](https://img.shields.io/badge/license-MIT-green)

## Features

- **Modern C++23**: Uses `std::expected`, `std::print`, structured bindings, and other C++23 features
- **Vulkan 1.3**: Complete Vulkan pipeline implementation
- **Validation Layers**: Debug builds include Vulkan validation for catching errors early
- **Clean Architecture**: Two-class design with clear separation between application and renderer
- **RAII Resource Management**: Automatic cleanup via destructors, no manual resource tracking
- **Double Buffering**: Smooth rendering with `MAX_FRAMES_IN_FLIGHT = 2`
- **Dynamic Resize**: Proper handling of window resizing with swapchain recreation
- **Animated Background**: Subtle color animation using time-based shader uniforms

## Requirements

- **CMake** 3.20 or higher
- **Vulkan SDK** (with `glslc` shader compiler)
- **GLFW3**
- C++23 compatible compiler (GCC 13+, Clang 17+, MSVC 2022+)

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt install libglfw3-dev
# Install Vulkan SDK from https://vulkan.lunarg.com/
```

**macOS:**
```bash
brew install glfw
# Install Vulkan SDK from https://vulkan.lunarg.com/
```

**Windows:**
Install GLFW and Vulkan SDK from their respective websites.

## Building

```bash
# Configure (Debug by default)
cmake -B build

# Configure Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build
```

**Note:** Ensure `VULKAN_SDK` environment variable is set before building:
```bash
# Linux/macOS
export VULKAN_SDK=/path/to/vulkan-sdk

# Windows
set VULKAN_SDK=C:\VulkanSDK\1.x.x.x
```

## Running

```bash
./build/VulkanApp
```

- Press **ESC** or close the window to exit
- FPS is printed to the console every second

## Architecture

### Application Layer (`src/Application.cpp`)
- GLFW window management
- Event handling (keyboard, resize)
- FPS calculation and display
- Delegates rendering to `VulkanRenderer`

### Renderer Layer (`src/VulkanRenderer.cpp`)
- Complete Vulkan pipeline:
  1. Instance creation with validation layers
  2. Debug messenger setup
  3. Window surface creation
  4. Physical device selection (prefers discrete GPU)
  5. Logical device and queues
  6. Swapchain management
  7. Graphics pipeline with vertex/fragment shaders
  8. Command buffers and synchronization

### Shaders
- **Vertex shader** (`shaders/triangle.vert`): Generates a full-screen triangle with vertex positions computed in-shader (no vertex buffer)
- **Fragment shader** (`shaders/triangle.frag`): Outputs interpolated RGB colors with subtle animation

Shaders are compiled to SPIR-V automatically during the build process.

## Design Principles

1. **Error Handling with `std::expected`**: No exceptions for expected error paths; Vulkan errors include `VkResult` codes
2. **Non-Copyable/Movable Classes**: Vulkan resources are managed via RAII destructors
3. **Modern C++ I/O**: Uses `std::print` instead of iostream
4. **Platform Agnostic**: Supports Linux, macOS, and Windows

## Project Structure

```
.
├── CMakeLists.txt          # Build configuration
├── src/
│   ├── main.cpp            # Entry point
│   ├── Application.h/cpp   # Window and main loop
│   └── VulkanRenderer.h/cpp # Vulkan implementation
└── shaders/
    ├── triangle.vert       # Vertex shader (GLSL)
    └── triangle.frag       # Fragment shader (GLSL)
```

## License

MIT License - See LICENSE file for details
