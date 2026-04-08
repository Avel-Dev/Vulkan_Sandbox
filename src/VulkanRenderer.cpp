#include "VulkanRenderer.h"

#include "Model.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>		// core types (vec3, mat4, etc.)
#include <glm/gtc/matrix_transform.hpp> // rotate, lookAt, perspective
#include <print>
#include <set>
#include <vulkan/vulkan_core.h>

// Platform-specific surface creation
#if defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

VkDevice VulkanRenderer::device_ = {VK_NULL_HANDLE};
VkPhysicalDevice VulkanRenderer::physicalDevice_ = {VK_NULL_HANDLE};
VkCommandBuffer VulkanRenderer::commandbuffer_ = {VK_NULL_HANDLE};

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
	cleanup();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto VulkanRenderer::initialize(GLFWwindow* window) -> std::expected<void, VulkanError> {
	window_ = window;

	// Step 1: Create Vulkan instance
	if (auto result = createInstance(); !result) {
		return std::unexpected(result.error());
	}

	// Step 2: Set up debug messenger (only in debug builds)
#ifdef ENABLE_VALIDATION_LAYERS
	if (auto result = setupDebugMessenger(); !result) {
		return std::unexpected(result.error());
	}
#endif

	// Step 3: Create window surface
	if (auto result = createSurface(); !result) {
		return std::unexpected(result.error());
	}

	// Step 4: Pick physical device
	if (auto result = pickPhysicalDevice(); !result) {
		return std::unexpected(result.error());
	}

	// Step 5: Create logical device
	if (auto result = createLogicalDevice(); !result) {
		return std::unexpected(result.error());
	}

	// Step 6-7: Create swap chain and image views
	if (auto result = createSwapChain(); !result) {
		return std::unexpected(result.error());
	}
	if (auto result = createImageViews(); !result) {
		return std::unexpected(result.error());
	}

	// Step 8: Create render pass
	if (auto result = createRenderPass(); !result) {
		return std::unexpected(result.error());
	}

	// Step 9: Create graphics pipeline
	if (auto result = createGraphicsPipeline(); !result) {
		return std::unexpected(result.error());
	}

	// Step 10: Create framebuffers
	if (auto result = createFramebuffers(); !result) {
		return std::unexpected(result.error());
	}

	// Step 11: Create command pool and buffers
	if (auto result = createCommandPool(); !result) {
		return std::unexpected(result.error());
	}
	if (auto result = createCommandBuffers(); !result) {
		return std::unexpected(result.error());
	}

	// Step 12: Create synchronization objects
	if (auto result = createSyncObjects(); !result) {
		return std::unexpected(result.error());
	}

	// Step 13: Create Uniform Buffers
	if (auto result = createUniformBuffers(); !result) {
		return std::unexpected(result.error());
	}

	auto descriptorSetLayout = createDescriptorSetLayout();
	if (!descriptorSetLayout) {
		return std::unexpected(descriptorSetLayout.error());
	}

	descriptorSetLayout_ = descriptorSetLayout.value();
	auto descriptorSet = createDiscriptorSets();
	if (!descriptorSet) {
		return std::unexpected(descriptorSet.error());
	}
	MVP_.resize(MAX_FRAMES_IN_FLIGHT);

	m_model = new Model();
	m_model->Init();

	return {};
}

auto VulkanRenderer::drawFrame() -> std::expected<void, VulkanError> {
	// Get synchronization objects for this frame
	auto [imageAvailable, inFlight] = syncObjects_[currentFrame_];

	// Wait for this frame's previous work to complete
	vkWaitForFences(device_, 1, &inFlight, VK_TRUE, UINT64_MAX);

	auto status = update();
	if (!status) {
		return status;
	}
	std::uint32_t imageIndex{};
	// Use this frame's imageAvailable semaphore to acquire a swapchain image
	VkResult result = vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX, imageAvailable,
					VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		return onWindowResized();
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		return std::unexpected(
		  VulkanError{"Failed to acquire swap chain image", result});
	}

	// Reset fence only if submitting work
	vkResetFences(device_, 1, &inFlight);

	// Record command buffer
	vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
	VulkanRenderer::commandbuffer_ = commandBuffers_[currentFrame_];

	if (auto r = recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex); !r) {
		return r;
	}

	// Use the per-image renderFinished semaphore to avoid semaphore reuse issues
	VkSemaphore renderFinished = renderFinishedPerImage_[imageIndex];

	// Submit command buffer
	VkSemaphore waitSemaphores[] = {imageAvailable};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore signalSemaphores[] = {renderFinished};

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlight) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to submit draw command buffer"});
	}

	// Present the frame
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain_;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(presentQueue_, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
	    framebufferResized_) {
		framebufferResized_ = false;
		if (auto r = recreateSwapChain(); !r) {
			return r;
		}
	} else if (result != VK_SUCCESS) {
		return std::unexpected(
		  VulkanError{"Failed to present swap chain image", result});
	}

	// Advance frame index
	currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;

	return {};
}

auto VulkanRenderer::update() -> std::expected<void, VulkanError> {
	return updateUniformBuffers();
}

auto VulkanRenderer::updateUniformBuffers() -> std::expected<void, VulkanError> {
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float>(currentTime - startTime).count();

	MVP& ubo = MVP_[currentFrame_];

	// rotate cube over time around Y axis
	ubo.model =
	  glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	// camera positioned at (2,2,2) looking at origin
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
			   glm::vec3(0.0f, 1.0f, 0.0f));

	// perspective projection
	ubo.proj =
	  glm::perspective(glm::radians(45.0f),
		         swapChainExtent_.width / (float)swapChainExtent_.height, 0.1f, 10.0f);

	// Vulkan Y-axis is flipped vs OpenGL — fix it
	ubo.proj[1][1] *= -1;

	memcpy(MVP_UniformBuffer[currentFrame_].bufferMapped, &ubo, sizeof(ubo));

	return {};
}

auto VulkanRenderer::onWindowResized() -> std::expected<void, VulkanError> {
	framebufferResized_ = true;
	int width = 0, height = 0;
	glfwGetFramebufferSize(window_, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window_, &width, &height);
		glfwWaitEvents();
	}

	return recreateSwapChain();
}

// ---------------------------------------------------------------------------
// Instance Creation
// ---------------------------------------------------------------------------

auto VulkanRenderer::createInstance() -> std::expected<void, VulkanError> {
#ifdef ENABLE_VALIDATION_LAYERS
	if (!checkValidationLayerSupport()) {
		return std::unexpected(
		  VulkanError{"Validation layers requested but not available!\n"
			    "Install the Vulkan SDK or disable ENABLE_VALIDATION_LAYERS."});
	}
#endif

	// Application info
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Minimal Renderer";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	// Create info
	auto extensions = getRequiredExtensions();

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef ENABLE_VALIDATION_LAYERS
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	populateDebugMessengerInfo(debugCreateInfo);

	createInfo.enabledLayerCount = static_cast<std::uint32_t>(VALIDATION_LAYERS.size());
	createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	createInfo.pNext = &debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
#endif

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
	if (result != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create Vulkan instance", result});
	}

	return {};
}

auto VulkanRenderer::checkValidationLayerSupport() const -> bool {
	std::uint32_t layerCount{};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : VALIDATION_LAYERS) {
		bool found = false;
		for (const auto& layer : availableLayers) {
			if (std::strcmp(layerName, layer.layerName) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}
	return true;
}

auto VulkanRenderer::getRequiredExtensions() const -> std::vector<const char*> {
	std::uint32_t glfwExtensionCount{};
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef ENABLE_VALIDATION_LAYERS
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	return extensions;
}

// ---------------------------------------------------------------------------
// Debug Messenger
// ---------------------------------------------------------------------------

auto VulkanRenderer::setupDebugMessenger() -> std::expected<void, VulkanError> {
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerInfo(createInfo);

	// Load extension function
	auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
	  vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));

	if (!func) {
		return std::unexpected(
		  VulkanError{"Failed to load vkCreateDebugUtilsMessengerEXT"});
	}

	if (func(instance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to set up debug messenger"});
	}

	return {};
}

void VulkanRenderer::populateDebugMessengerInfo(VkDebugUtilsMessengerCreateInfoEXT& info) const {
	info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info.messageSeverity = DEBUG_SEVERITY;
	info.messageType = DEBUG_TYPE;
	info.pfnUserCallback = debugCallback;
	info.pUserData = nullptr;
}

auto VulkanRenderer::destroyDebugMessenger() -> void {
	if (debugMessenger_ != VK_NULL_HANDLE) {
		auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
		  vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
		if (func) {
			func(instance_, debugMessenger_, nullptr);
		}
	}
}

// Debug callback - static method
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
	// Print to stderr with appropriate prefix
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		std::println(std::cerr, "[VULKAN ERROR] {}", pCallbackData->pMessage);
	} else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::println(std::cerr, "[VULKAN WARNING] {}", pCallbackData->pMessage);
	} else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		std::println(std::cerr, "[VULKAN INFO] {}", pCallbackData->pMessage);
	} else {
		std::println(std::cerr, "[VULKAN VERBOSE] {}", pCallbackData->pMessage);
	}

	return VK_FALSE; // Don't abort on validation errors
}

// ---------------------------------------------------------------------------
// Surface & Device
// ---------------------------------------------------------------------------

auto VulkanRenderer::createSurface() -> std::expected<void, VulkanError> {
	if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create window surface"});
	}
	return {};
}

auto VulkanRenderer::pickPhysicalDevice() -> std::expected<void, VulkanError> {
	std::uint32_t deviceCount{};
	vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

	if (deviceCount == 0) {
		return std::unexpected(VulkanError{"No Vulkan-compatible GPUs found"});
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

	// Prefer discrete GPU, fallback to first suitable
	VkPhysicalDevice bestDevice{VK_NULL_HANDLE};
	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			// Prefer discrete GPU
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);

			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				physicalDevice_ = device;
				break;
			}
			if (bestDevice == VK_NULL_HANDLE) {
				bestDevice = device;
			}
		}
	}

	if (physicalDevice_ == VK_NULL_HANDLE) {
		physicalDevice_ = bestDevice;
	}

	if (physicalDevice_ == VK_NULL_HANDLE) {
		return std::unexpected(VulkanError{"No suitable GPU found"});
	}

	// Print selected GPU
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(physicalDevice_, &props);
	std::println("Selected GPU: {}", props.deviceName);
	std::println("Push Constants Size: {}", props.limits.maxPushConstantsSize);

	return {};
}

auto VulkanRenderer::isDeviceSuitable(VkPhysicalDevice device) const -> bool {
	auto indices = findQueueFamilies(device);
	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionsSupported) {
		auto swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate =
		  !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

auto VulkanRenderer::findQueueFamilies(VkPhysicalDevice device) const -> QueueFamilyIndices {
	QueueFamilyIndices indices;

	std::uint32_t queueFamilyCount{};
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	for (std::uint32_t i = 0; i < queueFamilyCount; ++i) {
		// Graphics family
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		// Present family
		VkBool32 presentSupport{VK_FALSE};
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
		if (presentSupport) {
			indices.presentFamily = i;
		}
	}

	return indices;
}

auto VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) const -> bool {
	std::uint32_t extensionCount{};
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
				       availableExtensions.data());

	std::set<std::string> requiredExtensions(DEVICE_EXTENSIONS.begin(),
					 DEVICE_EXTENSIONS.end());

	for (const auto& ext : availableExtensions) {
		requiredExtensions.erase(ext.extensionName);
	}

	return requiredExtensions.empty();
}

auto VulkanRenderer::querySwapChainSupport(VkPhysicalDevice device) const
  -> SwapChainSupportDetails {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

	std::uint32_t formatCount{};
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount,
					       details.formats.data());
	}

	std::uint32_t presentModeCount{};
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount,
						  details.presentModes.data());
	}

	return details;
}

auto VulkanRenderer::createLogicalDevice() -> std::expected<void, VulkanError> {
	auto indices = findQueueFamilies(physicalDevice_);

	// Unique queue families
	std::set<std::uint32_t> uniqueQueueFamilies{*indices.graphicsFamily,
					    *indices.presentFamily};

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	float queuePriority = 1.0f;

	for (std::uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Device features (empty = use defaults)
	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(DEVICE_EXTENSIONS.size());
	createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
	createInfo.pEnabledFeatures = &deviceFeatures;

#ifdef ENABLE_VALIDATION_LAYERS
	createInfo.enabledLayerCount = static_cast<std::uint32_t>(VALIDATION_LAYERS.size());
	createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
#else
	createInfo.enabledLayerCount = 0;
#endif

	if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create logical device"});
	}

	// Get queue handles
	vkGetDeviceQueue(device_, *indices.graphicsFamily, 0, &graphicsQueue_);
	vkGetDeviceQueue(device_, *indices.presentFamily, 0, &presentQueue_);

	return {};
}

// ---------------------------------------------------------------------------
// Swap Chain
// ---------------------------------------------------------------------------

auto VulkanRenderer::createSwapChain() -> std::expected<void, VulkanError> {
	auto [capabilities, formats, presentModes] = querySwapChainSupport(physicalDevice_);

	auto surfaceFormat = chooseSwapSurfaceFormat(formats);
	auto presentMode = chooseSwapPresentMode(presentModes);
	auto extent = chooseSwapExtent(capabilities);

	std::uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface_;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto indices = findQueueFamilies(physicalDevice_);
	std::uint32_t queueFamilyIndices[] = {*indices.graphicsFamily, *indices.presentFamily};

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create swap chain"});
	}

	// Get swap chain images
	vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
	swapChainImages_.resize(imageCount);
	vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages_.data());

	swapChainImageFormat_ = surfaceFormat.format;
	swapChainExtent_ = extent;

	return {};
}

auto VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
  -> VkSurfaceFormatKHR {
	// Prefer SRGB with non-linear color space
	for (const auto& format : formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
		    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}
	return formats[0]; // Fallback to first available
}

auto VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const
  -> VkPresentModeKHR {
	// Prefer mailbox (low latency, no tearing)
	for (const auto& mode : modes) {
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return mode;
		}
	}
	// Fallback to FIFO (V-Sync)
	return VK_PRESENT_MODE_FIFO_KHR;
}

auto VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
  -> VkExtent2D {
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}

	int width, height;
	glfwGetFramebufferSize(window_, &width, &height);

	VkExtent2D actualExtent{.width = static_cast<std::uint32_t>(width),
			    .height = static_cast<std::uint32_t>(height)};

	actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
				  capabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
				   capabilities.maxImageExtent.height);

	return actualExtent;
}

auto VulkanRenderer::createImageViews() -> std::expected<void, VulkanError> {
	swapChainImageViews_.resize(swapChainImages_.size());

	for (size_t i = 0; i < swapChainImages_.size(); ++i) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages_[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat_;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device_, &createInfo, nullptr,
				  &swapChainImageViews_[i]) != VK_SUCCESS) {
			return std::unexpected(VulkanError{"Failed to create image view"});
		}
	}

	return {};
}

// ---------------------------------------------------------------------------
// Graphics Pipeline
// ---------------------------------------------------------------------------

auto VulkanRenderer::createRenderPass() -> std::expected<void, VulkanError> {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapChainImageFormat_;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create render pass"});
	}

	return {};
}

auto VulkanRenderer::createGraphicsPipeline() -> std::expected<void, VulkanError> {
	// Load shaders
	auto vertCode = readFile(SHADER_DIR "/model/model.vert.spv");
	if (!vertCode) {
		return std::unexpected(vertCode.error());
	}

	auto fragCode = readFile(SHADER_DIR "/model/model.frag.spv");
	if (!fragCode) {
		return std::unexpected(fragCode.error());
	}

	auto vertModule = createShaderModule(*vertCode);
	if (!vertModule) {
		return std::unexpected(vertModule.error());
	}

	auto fragModule = createShaderModule(*fragCode);
	if (!fragModule) {
		return std::unexpected(fragModule.error());
	}

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = *vertModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = *fragModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
						fragShaderStageInfo};

	std::array<VkVertexInputAttributeDescription, 2> attrDescs{};

	// position → location 0 in shader
	attrDescs[0].binding = 0;
	attrDescs[0].location = 0;
	attrDescs[0].format = VK_FORMAT_R32G32_SFLOAT;
	attrDescs[0].offset = offsetof(Vertex, position);

	// color → location 1 in shader
	attrDescs[1].binding = 0;
	attrDescs[1].location = 1;
	attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrDescs[1].offset = offsetof(Vertex, color);

	VkVertexInputBindingDescription bindingDesc{};
	bindingDesc.binding = 0;
	bindingDesc.stride = sizeof(Vertex);
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// Vertex input (no vertices - triangle is generated in vertex shader)
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexAttributeDescriptionCount = 2;
	vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;

	// Input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.lineWidth = 1.0f;

	// Multisampling
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;

	// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
				        VK_COLOR_COMPONENT_G_BIT |
				        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	auto layoutResult = createDescriptorSetLayout();

	if (!layoutResult) {
		return std::unexpected(layoutResult.error());
	}

	VkDescriptorSetLayout layout = layoutResult.value();
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &layout;

	if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) !=
	    VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create pipeline layout"});
	}

	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
					     VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent_.width;
	viewport.height = (float)swapChainExtent_.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = swapChainExtent_;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// Create pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = pipelineLayout_;
	pipelineInfo.renderPass = renderPass_;
	pipelineInfo.subpass = 0;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pViewportState = &viewportState;

	if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
				&graphicsPipeline_) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create graphics pipeline"});
	}

	// Cleanup shader modules
	vkDestroyShaderModule(device_, *vertModule, nullptr);
	vkDestroyShaderModule(device_, *fragModule, nullptr);

	return {};
}

auto VulkanRenderer::createShaderModule(const std::vector<char>& code) const
  -> std::expected<VkShaderModule, VulkanError> {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

	VkShaderModule shaderModule{};
	if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create shader module"});
	}

	return shaderModule;
}

auto VulkanRenderer::createFramebuffers() -> std::expected<void, VulkanError> {
	framebuffers_.resize(swapChainImageViews_.size());

	for (size_t i = 0; i < swapChainImageViews_.size(); ++i) {
		VkImageView attachments[] = {swapChainImageViews_[i]};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass_;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapChainExtent_.width;
		framebufferInfo.height = swapChainExtent_.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr,
				    &framebuffers_[i]) != VK_SUCCESS) {
			return std::unexpected(VulkanError{"Failed to create framebuffer"});
		}
	}

	return {};
}

// ---------------------------------------------------------------------------
// Commands & Sync
// ---------------------------------------------------------------------------

auto VulkanRenderer::createCommandPool() -> std::expected<void, VulkanError> {
	auto indices = findQueueFamilies(physicalDevice_);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = *indices.graphicsFamily;

	if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create command pool"});
	}

	return {};
}

auto VulkanRenderer::createCommandBuffers() -> std::expected<void, VulkanError> {
	commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool_;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());

	if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to allocate command buffers"});
	}

	return {};
}

auto VulkanRenderer::recordCommandBuffer(VkCommandBuffer buffer, std::uint32_t imageIndex)
  -> std::expected<void, VulkanError> {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(buffer, &beginInfo) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to begin recording command buffer"});
	}

	// Dynamic clear color (subtle animation based on time)
	auto time = std::chrono::steady_clock::now().time_since_epoch().count();
	float r = 0.2f + 0.1f * std::sin(time / 1e9f);
	float g = 0.3f + 0.1f * std::sin(time / 1e9f + 2.0f);
	float b = 0.5f + 0.1f * std::sin(time / 1e9f + 4.0f);

	VkClearValue clearColor{};
	clearColor.color = {{r, g, b, 1.0f}};

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass_;
	renderPassInfo.framebuffer = framebuffers_[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = swapChainExtent_;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);
	// Viewport and scissor
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent_.width);
	viewport.height = static_cast<float>(swapChainExtent_.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vkCmdSetViewport(buffer, 0, 1, &viewport);
	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = swapChainExtent_;
	vkCmdSetScissor(buffer, 0, 1, &scissor);
	vkCmdBindDescriptorSets(commandBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS,
			    pipelineLayout_, 0, 1, &descriptorSets_[currentFrame_], 0,
			    nullptr);
	m_model->Bind();
	m_model->Draw();
	// vkCmdDraw(buffer, 3, 1, 0, 0); // Draw 3 vertices (full-screen triangle)

	vkCmdEndRenderPass(buffer);

	if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to record command buffer"});
	}

	return {};
}

auto VulkanRenderer::createSyncObjects() -> std::expected<void, VulkanError> {
	syncObjects_.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedPerImage_.resize(swapChainImages_.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	// Create per-frame sync objects (imageAvailable semaphores + fences)
	for (auto& sync : syncObjects_) {
		if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &sync.imageAvailable) !=
		      VK_SUCCESS ||
		    vkCreateFence(device_, &fenceInfo, nullptr, &sync.inFlight) != VK_SUCCESS) {
			return std::unexpected(
			  VulkanError{"Failed to create synchronization objects"});
		}
	}

	// Create per-swapchain-image renderFinished semaphores
	for (auto& semaphore : renderFinishedPerImage_) {
		if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore) !=
		    VK_SUCCESS) {
			return std::unexpected(
			  VulkanError{"Failed to create renderFinished semaphore"});
		}
	}

	return {};
}

// ---------------------------------------------------------------------------
// Resize Handling
// ---------------------------------------------------------------------------

auto VulkanRenderer::recreateSwapChain() -> std::expected<void, VulkanError> {
	// Wait for device idle
	vkDeviceWaitIdle(device_);

	// Cleanup old swap chain
	cleanupSwapChain();

	// Recreate
	if (auto result = createSwapChain(); !result) {
		return result;
	}
	if (auto result = createImageViews(); !result) {
		return result;
	}
	if (auto result = createFramebuffers(); !result) {
		return result;
	}

	// Recreate per-image renderFinished semaphores
	for (auto semaphore : renderFinishedPerImage_) {
		if (semaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(device_, semaphore, nullptr);
		}
	}
	renderFinishedPerImage_.resize(swapChainImages_.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (auto& semaphore : renderFinishedPerImage_) {
		if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore) !=
		    VK_SUCCESS) {
			return std::unexpected(VulkanError{
			  "Failed to create renderFinished semaphore during resize"});
		}
	}

	return {};
}

void VulkanRenderer::cleanupSwapChain() {
	for (auto framebuffer : framebuffers_) {
		vkDestroyFramebuffer(device_, framebuffer, nullptr);
	}
	framebuffers_.clear();

	for (auto imageView : swapChainImageViews_) {
		vkDestroyImageView(device_, imageView, nullptr);
	}
	swapChainImageViews_.clear();

	if (swapChain_ != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device_, swapChain_, nullptr);
		swapChain_ = VK_NULL_HANDLE;
	}
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

void VulkanRenderer::cleanup() {
	if (device_ == VK_NULL_HANDLE) return;

	vkDeviceWaitIdle(device_);

	// Per-image renderFinished semaphores
	for (auto semaphore : renderFinishedPerImage_) {
		if (semaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(device_, semaphore, nullptr);
		}
	}
	renderFinishedPerImage_.clear();

	// Per-frame sync objects (imageAvailable semaphores + fences)
	for (auto& sync : syncObjects_) {
		if (sync.inFlight != VK_NULL_HANDLE) {
			vkDestroyFence(device_, sync.inFlight, nullptr);
		}
		if (sync.imageAvailable != VK_NULL_HANDLE) {
			vkDestroySemaphore(device_, sync.imageAvailable, nullptr);
		}
	}

	// Command pool
	if (commandPool_ != VK_NULL_HANDLE) {
		vkDestroyCommandPool(device_, commandPool_, nullptr);
	}

	// Pipeline
	if (graphicsPipeline_ != VK_NULL_HANDLE) {
		vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
	}
	if (pipelineLayout_ != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
	}
	if (renderPass_ != VK_NULL_HANDLE) {
		vkDestroyRenderPass(device_, renderPass_, nullptr);
	}

	// Swap chain
	cleanupSwapChain();

	// Device
	vkDestroyDevice(device_, nullptr);

	// Surface
	if (surface_ != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance_, surface_, nullptr);
	}

	// Debug messenger
	destroyDebugMessenger();

	// Instance
	if (instance_ != VK_NULL_HANDLE) {
		vkDestroyInstance(instance_, nullptr);
	}
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

auto VulkanRenderer::readFile(const std::string& filename)
  -> std::expected<std::vector<char>, VulkanError> {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return std::unexpected(
		  VulkanError{"Failed to open file: " + filename +
			    "\n"
			    "Ensure the shaders were compiled and SHADER_DIR is correct."});
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
	file.close();

	return buffer;
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(VulkanRenderer::physicalDevice_, &memProps);

	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
		    (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	throw std::runtime_error("Failed to find suitable memory type!");
}

auto VulkanRenderer::createDiscriptorPool() -> std::expected<VkDescriptorPool, VulkanError> {
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPool descriptorPool;
	if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create descriptor pools"});
	}

	return descriptorPool;
}

auto VulkanRenderer::createDiscriptorSets() -> std::expected<void, VulkanError> {
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	auto pool = createDiscriptorPool();
	if (!pool) {
		return std::unexpected(pool.error());
	}
	allocInfo.descriptorPool = pool.value();
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
	vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data());

	// write the uniform buffer into each descriptor set
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = MVP_UniformBuffer[currentFrame_].uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(MVP);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets_[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
	}
	return {};
}

auto VulkanRenderer::createDescriptorSetLayout()
  -> std::expected<VkDescriptorSetLayout, VulkanError> {
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	VkDescriptorSetLayout descriptorSetLayout;
	if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout) !=
	    VK_SUCCESS) {
		return std::unexpected(VulkanError{"Failed to create Descriptro Sets"});
	}
	return descriptorSetLayout;
}

auto VulkanRenderer::createUniformBuffers() -> std::expected<void, VulkanError> {
	VkDeviceSize bufferSize = sizeof(MVP);

	MVP_UniformBuffer.resize(MAX_FRAMES_IN_FLIGHT);

	for (auto& bufferdata : MVP_UniformBuffer) {
		auto& [uniformBuffer, uniformBufferMemory, bufferMapped] = bufferdata;

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(VulkanRenderer::device_, &bufferInfo, nullptr,
			         &uniformBuffer) != VK_SUCCESS) {
			return std::unexpected(
			  VulkanError{"Failed to create Buffer for uniform buffer"});
		}
		VkMemoryRequirements memReqs;
		vkGetBufferMemoryRequirements(VulkanRenderer::device_, uniformBuffer, &memReqs);
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = findMemoryType(
		  memReqs.memoryTypeBits,
		  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkAllocateMemory(VulkanRenderer::device_, &allocInfo, nullptr,
			       &uniformBufferMemory);
		vkBindBufferMemory(VulkanRenderer::device_, uniformBuffer, uniformBufferMemory,
			         0);
		vkMapMemory(device_, uniformBufferMemory, 0, bufferInfo.size, 0, &bufferMapped);
	}

	return {};
}
