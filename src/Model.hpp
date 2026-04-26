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
	void createbuffer(VkDeviceSize size, VkBuffer& buffer, VkDeviceMemory& deviceMemory,
		        VkBufferUsageFlags usage);

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
	  // back (-Z)
	  0, 2, 1, 0, 3, 2,

	  // front (+Z)
	  4, 5, 6, 4, 6, 7,

	  // left (-X)
	  0, 7, 3, 0, 4, 7,

	  // right (+X)
	  1, 2, 6, 1, 6, 5,

	  // top (+Y)
	  3, 7, 6, 3, 6, 2,

	  // bottom (-Y)
	  0, 1, 5, 0, 5, 4};
	VkBuffer m_VertexBuffer;
	VkDeviceMemory m_VertexBufferMemory;

	VkBuffer m_IndexBuffer;
	VkDeviceMemory m_IndexBufferMemory;
};
