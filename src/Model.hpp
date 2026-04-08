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
	const std::vector<Vertex> vertices = {
	  // position              // color
	  {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 0
	  {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},  // 1
	  {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},   // 2
	  {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},  // 3
	  {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}},  // 4
	  {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}},   // 5
	  {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},    // 6
	  {{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}},   // 7
	};
	const std::vector<uint16_t> indices = {
	  0, 1, 2, 2, 3, 0, // back face
	  4, 5, 6, 6, 7, 4, // front face
	  0, 4, 7, 7, 3, 0, // left face
	  1, 5, 6, 6, 2, 1, // right face
	  3, 7, 6, 6, 2, 3, // top face
	  0, 4, 5, 5, 1, 0	// bottom face
	};
	VkBuffer m_VertexBuffer;
	VkDeviceMemory m_VertexBufferMemory;

	VkBuffer m_IndexBuffer;
	VkDeviceMemory m_IndexBufferMemory;
};
