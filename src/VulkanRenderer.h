#pragma once

#include "glm/ext/matrix_float4x4.hpp"

#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <array>
#include <expected>
#include <optional>
#include <string>
#include <vector>
// Validation layer constants
constexpr std::array VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};

constexpr std::array DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// Debug callback configuration
constexpr VkDebugUtilsMessageSeverityFlagsEXT DEBUG_SEVERITY =
  VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

constexpr VkDebugUtilsMessageTypeFlagsEXT DEBUG_TYPE =
  VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

/**
 * @brief Error type for Vulkan operations
 */
struct VulkanError {
	std::string message;
	VkResult result{VK_SUCCESS};
};

// MVP
struct MVP {
	glm::mat4 model;
};

struct Camera {
	glm::mat4 view;
	glm::mat4 proj;
};

struct UniformBuffer {
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;
	void* bufferMapped;
};

/**
 * @brief Queue family indices for device selection
 */
struct QueueFamilyIndices {
	std::optional<std::uint32_t> graphicsFamily;
	std::optional<std::uint32_t> presentFamily;

	[[nodiscard]] constexpr bool isComplete() const noexcept {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

/**
 * @brief Swap chain support details
 */
struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

/**
 * @brief Frame synchronization objects
 * Note: renderFinished semaphore is per-swapchain-image, not per-frame,
 * to avoid semaphore reuse validation errors.
 */
struct FrameSync {
	VkSemaphore imageAvailable{VK_NULL_HANDLE};
	VkFence inFlight{VK_NULL_HANDLE};
};

/**
 * @brief Minimal Vulkan renderer with validation layers
 */

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

class Model;

class VulkanRenderer {
        public:
	VulkanRenderer();
	~VulkanRenderer();

	// Non-copyable, non-movable
	VulkanRenderer(const VulkanRenderer&) = delete;
	VulkanRenderer& operator=(const VulkanRenderer&) = delete;
	VulkanRenderer(VulkanRenderer&&) = delete;
	VulkanRenderer& operator=(VulkanRenderer&&) = delete;

	/**
	 * @brief Initialize the Vulkan renderer
	 * @param window GLFW window handle
	 * @return Expected void or VulkanError
	 */
	[[nodiscard]] auto initialize(GLFWwindow* window) -> std::expected<void, VulkanError>;

	/**
	 * @brief Render a single frame
	 */
	auto drawFrame() -> std::expected<void, VulkanError>;
	auto update() -> std::expected<void, VulkanError>;
	auto updateUniformBuffers() -> std::expected<void, VulkanError>;

	/**
	 * @brief Handle window resize
	 */
	[[nodiscard]] auto onWindowResized() -> std::expected<void, VulkanError>;

	/**
	 * @brief Check if framebuffer was resized
	 */
	[[nodiscard]] auto wasResized() const noexcept -> bool {
		return framebufferResized_;
	}

	/**
	 * @brief Set framebuffer resize flag
	 */
	void setResized(bool resized) noexcept {
		framebufferResized_ = resized;
	}

        public:
	static VkDevice device_;
	static VkPhysicalDevice physicalDevice_;
	static VkCommandBuffer commandbuffer_;

        private:
	// Vulkan handles
	VkInstance instance_{VK_NULL_HANDLE};
	VkDebugUtilsMessengerEXT debugMessenger_{VK_NULL_HANDLE};
	VkSurfaceKHR surface_{VK_NULL_HANDLE};
	VkQueue graphicsQueue_{VK_NULL_HANDLE};
	VkQueue presentQueue_{VK_NULL_HANDLE};
	VkSwapchainKHR swapChain_{VK_NULL_HANDLE};
	VkFormat swapChainImageFormat_{VK_FORMAT_UNDEFINED};
	VkExtent2D swapChainExtent_{};

	std::vector<VkImage> swapChainImages_;
	std::vector<VkImageView> swapChainImageViews_;

	VkRenderPass renderPass_{VK_NULL_HANDLE};
	VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
	VkPipeline graphicsPipeline_{VK_NULL_HANDLE};

	std::vector<VkFramebuffer> framebuffers_;

	VkCommandPool commandPool_{VK_NULL_HANDLE};
	VkDescriptorSetLayout descriptorSetLayout_;
	std::vector<VkDescriptorSet> descriptorSets_;
	std::vector<VkCommandBuffer> commandBuffers_;
	std::vector<FrameSync> syncObjects_;
	std::vector<VkSemaphore> renderFinishedPerImage_;

	VkImage depthImage_;
	VkDeviceMemory depthImageMemory_;
	VkImageView depthImageView_;
	std::vector<MVP> MVP_;
	std::vector<UniformBuffer> MVP_UniformBuffer;
	// State
	GLFWwindow* window_{nullptr};

	bool framebufferResized_{false};
	std::uint32_t currentFrame_{0};
	static constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	// --- Instance creation ---
	[[nodiscard]] auto createInstance() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto checkValidationLayerSupport() const -> bool;
	[[nodiscard]] auto getRequiredExtensions() const -> std::vector<const char*>;

	// --- Debug messenger ---
	[[nodiscard]] auto setupDebugMessenger() -> std::expected<void, VulkanError>;
	void populateDebugMessengerInfo(VkDebugUtilsMessengerCreateInfoEXT& info) const;

	// --- Surface & Device ---
	[[nodiscard]] auto createSurface() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto pickPhysicalDevice() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto isDeviceSuitable(VkPhysicalDevice device) const -> bool;
	[[nodiscard]] auto findQueueFamilies(VkPhysicalDevice device) const -> QueueFamilyIndices;
	[[nodiscard]] auto checkDeviceExtensionSupport(VkPhysicalDevice device) const -> bool;
	[[nodiscard]] auto querySwapChainSupport(VkPhysicalDevice device) const
	  -> SwapChainSupportDetails;
	[[nodiscard]] auto createLogicalDevice() -> std::expected<void, VulkanError>;

	// --- Swap chain ---
	[[nodiscard]] auto createSwapChain() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto
	chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
	  -> VkSurfaceFormatKHR;
	[[nodiscard]] auto chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const
	  -> VkPresentModeKHR;
	[[nodiscard]] auto chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
	  -> VkExtent2D;
	[[nodiscard]] auto createImageViews() -> std::expected<void, VulkanError>;

	// --- Graphics pipeline ---
	[[nodiscard]] auto createRenderPass() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto createGraphicsPipeline() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto createShaderModule(const std::vector<char>& code) const
	  -> std::expected<VkShaderModule, VulkanError>;
	[[nodiscard]] auto createFramebuffers() -> std::expected<void, VulkanError>;

	// --- Commands & Sync ---
	[[nodiscard]] auto createCommandPool() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto createCommandBuffers() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto recordCommandBuffer(VkCommandBuffer buffer, std::uint32_t imageIndex)
	  -> std::expected<void, VulkanError>;
	[[nodiscard]] auto createSyncObjects() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto createDescriptorSetLayout()
	  -> std::expected<VkDescriptorSetLayout, VulkanError>;
	[[nodiscard]] auto createDiscriptorPool() -> std::expected<VkDescriptorPool, VulkanError>;
	[[nodiscard]] auto createDiscriptorSets() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto createUniformBuffers() -> std::expected<void, VulkanError>;
	[[nodiscard]] auto createDepthResources() -> std::expected<void, VulkanError>;

	// --- Resize handling ---
	[[nodiscard]] auto recreateSwapChain() -> std::expected<void, VulkanError>;
	void cleanupSwapChain();

	// --- Resource cleanup ---
	void destroyDebugMessenger();
	void cleanup();

	// --- Utilities ---
	[[nodiscard]] static auto readFile(const std::string& filename)
	  -> std::expected<std::vector<char>, VulkanError>;

	// --- Static callback for debug messenger ---
	static VKAPI_ATTR VkBool32 VKAPI_CALL
	debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		    VkDebugUtilsMessageTypeFlagsEXT messageType,
		    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	Model* m_model;
	Camera m_camera;
};
