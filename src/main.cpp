#include "Application.h"

#include <cstdlib>
#include <iostream>
#include <print>

/**
 * @brief Entry point for the Vulkan application
 *
 * This minimal renderer demonstrates:
 * - C++23 features (std::expected, std::optional, structured bindings)
 * - Vulkan validation layers in Debug builds
 * - RAII resource management
 * - Clean separation of concerns
 */

int main() {
	std::println("==============================");
	std::println("Vulkan Minimal Renderer");
	std::println("C++23 | Validation Layers | Clean Architecture");
	std::println("==============================\n");

	try {
		Application app;

		if (auto result = app.initialize(); !result) {
			std::println(std::cerr, "Initialization failed: {}",
				   result.error().message);
			return EXIT_FAILURE;
		}

		app.run();

	} catch (const std::exception& e) {
		std::println(std::cerr, "Fatal error: {}", e.what());
		return EXIT_FAILURE;
	} catch (...) {
		std::println(std::cerr, "Unknown fatal error occurred");
		return EXIT_FAILURE;
	}

	std::println("\nGoodbye!");
	return EXIT_SUCCESS;
}
