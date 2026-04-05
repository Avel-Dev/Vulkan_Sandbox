#include "Application.h"

#include <cmath>
#include <print>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Application::Application() = default;

Application::~Application() { cleanup(); }

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto Application::initialize() -> std::expected<void, AppError> {
  // Initialize GLFW
  if (auto result = initGLFW(); !result) {
    return result;
  }

  // Create window
  if (auto result = createWindow(); !result) {
    return result;
  }

  // Initialize Vulkan renderer
  if (auto result = renderer_.initialize(window_); !result) {
    return std::unexpected(
        AppError{"Vulkan initialization failed: " + result.error().message});
  }

  running_ = true;
  return {};
}

void Application::run() {
  if (!running_) {
    std::println(std::cerr, "Application not initialized!");
    return;
  }

  std::println("Running... Press ESC or close window to exit.");
  std::println("Rendering full-screen triangle with animated clear color.\n");

  lastTime_ = glfwGetTime();

  while (running_ && !glfwWindowShouldClose(window_)) {
    // Poll events
    glfwPollEvents();

    // Check for ESC key
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

    // Render frame
    if (auto result = renderer_.drawFrame(); !result) {
      std::println(std::cerr, "Render error: {}", result.error().message);
      break;
    }

    // Update FPS
    updateFPS();
  }

  // Ensure GPU is idle before cleanup
  return renderer_.onWindowResized(); // Triggers device wait
}

// ---------------------------------------------------------------------------
// GLFW Callbacks
// ---------------------------------------------------------------------------

void Application::framebufferResizeCallback(GLFWwindow *window, int, int) {
  // Get the application instance from user pointer
  auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
  if (app) {
    app->renderer_.setResized(true);
  }
}

// ---------------------------------------------------------------------------
// Private Methods
// ---------------------------------------------------------------------------

auto Application::initGLFW() -> std::expected<void, AppError> {
  if (!glfwInit()) {
    return std::unexpected(AppError{"Failed to initialize GLFW"});
  }

  // GLFW window hints for Vulkan
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);     // No OpenGL context
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);        // Allow resizing
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE); // HiDPI support

  return {};
}

auto Application::createWindow() -> std::expected<void, AppError> {
  window_ = glfwCreateWindow(static_cast<int>(DEFAULT_WIDTH),
                             static_cast<int>(DEFAULT_HEIGHT), TITLE.data(),
                             nullptr, // Monitor (nullptr for windowed)
                             nullptr  // Share (nullptr for no sharing)
  );

  if (!window_) {
    return std::unexpected(AppError{"Failed to create GLFW window"});
  }

  // Store this pointer for callbacks
  glfwSetWindowUserPointer(window_, this);

  // Set resize callback
  glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);

  // Print info
  int actualWidth, actualHeight;
  glfwGetFramebufferSize(window_, &actualWidth, &actualHeight);
  std::println("Window created: {}x{} (framebuffer: {}x{})", DEFAULT_WIDTH,
               DEFAULT_HEIGHT, actualWidth, actualHeight);

  return {};
}

void Application::updateFPS() {
  frameCount_++;

  double currentTime = glfwGetTime();
  double deltaTime = currentTime - lastTime_;

  if (deltaTime >= 1.0) {
    fps_ = static_cast<double>(frameCount_) / deltaTime;
    std::println("FPS: {:.1f}", fps_);

    frameCount_ = 0;
    lastTime_ = currentTime;
  }
}

void Application::cleanup() {
  // Renderer cleanup is automatic via RAII destructor

  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }

  glfwTerminate();
}
