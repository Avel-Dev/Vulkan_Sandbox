#pragma once

#include "VulkanRenderer.h"

#include <GLFW/glfw3.h>

#include <string>
#include <expected>
#include <cstdint>

/**
 * @brief Application error type
 */
struct AppError {
    std::string message;
};

/**
 * @brief Main application class that manages the window and renderer
 */
class Application {
public:
    // Window settings
    static constexpr std::uint32_t DEFAULT_WIDTH  = 1280;
    static constexpr std::uint32_t DEFAULT_HEIGHT = 720;
    static constexpr std::string_view TITLE = "Vulkan Minimal Renderer";

    Application();
    ~Application();

    // Non-copyable, non-movable
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    /**
     * @brief Initialize the application
     * @return Expected void or AppError
     */
    [[nodiscard]] auto initialize() -> std::expected<void, AppError>;

    /**
     * @brief Run the main loop
     */
    void run();

    /**
     * @brief Get the GLFW window
     */
    [[nodiscard]] auto window() const noexcept -> GLFWwindow* { return window_; }

    /**
     * @brief Static callback for framebuffer resize
     */
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

private:
    GLFWwindow*    window_{nullptr};
    VulkanRenderer renderer_;

    // State
    bool running_{false};

    // FPS tracking
    double lastTime_{0.0};
    int    frameCount_{0};
    double fps_{0.0};

    /**
     * @brief Initialize GLFW
     */
    [[nodiscard]] auto initGLFW() -> std::expected<void, AppError>;

    /**
     * @brief Create the application window
     */
    [[nodiscard]] auto createWindow() -> std::expected<void, AppError>;

    /**
     * @brief Update FPS counter
     */
    void updateFPS();

    /**
     * @brief Cleanup resources
     */
    void cleanup();
};
