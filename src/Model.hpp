#include "glm/ext/vector_float3.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
};

class Model {
        public:
	void Init();
	void Bind();
	void Draw();

        private:
	std::vector<Vertex> vertices = {
	  // Front face
	  {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
	  {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
	  {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	  {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}},

	  // Back face
	  {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
	  {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
	  {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
	  {{-0.5f, 0.5f, -0.5f}, {0.2f, 0.2f, 0.2f}},
	};

	std::vector<uint16_t> indices = {
	  // Front
	  0, 1, 2, 2, 3, 0,

	  // Right
	  1, 5, 6, 6, 2, 1,

	  // Back
	  5, 4, 7, 7, 6, 5,

	  // Left
	  4, 0, 3, 3, 7, 4,

	  // Top
	  3, 2, 6, 6, 7, 3,

	  // Bottom
	  4, 5, 1, 1, 0, 4};

	VkBuffer m_VertexBuffer;
	VkDeviceMemory m_VertexBufferMemory;

	VkBuffer m_IndexBuffer;
	VkDeviceMemory m_IndexBufferMemory;
};
