#include "Model.hpp"

#include "VulkanRenderer.h"

#include <cstring>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

void Model::createbuffer(VkDeviceSize size, VkBuffer& buffer, VkDeviceMemory& deviceMemory,
		     VkBufferUsageFlags usage) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vkCreateBuffer(VulkanRenderer::s_device, &bufferInfo, nullptr, &buffer);

	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(VulkanRenderer::s_device, buffer, &memReqs);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex =
	  findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkAllocateMemory(VulkanRenderer::s_device, &allocInfo, nullptr, &deviceMemory);
	vkBindBufferMemory(VulkanRenderer::s_device, buffer, deviceMemory, 0);
}

void Model::Init() {
	auto vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	auto indexBufferSize = sizeof(indices[0]) * indices.size();

	createbuffer(vertexBufferSize, m_VertexBuffer, m_VertexBufferMemory,
		   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	createbuffer(indexBufferSize, m_IndexBuffer, m_IndexBufferMemory,
		   VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	void* data;
	vkMapMemory(VulkanRenderer::s_device, m_VertexBufferMemory, 0, vertexBufferSize, 0, &data);
	memcpy(data, vertices.data(), vertexBufferSize);
	vkUnmapMemory(VulkanRenderer::s_device, m_VertexBufferMemory);

	vkMapMemory(VulkanRenderer::s_device, m_IndexBufferMemory, 0, indexBufferSize, 0, &data);
	memcpy(data, indices.data(), indexBufferSize);
	vkUnmapMemory(VulkanRenderer::s_device, m_IndexBufferMemory);
}

void Model::Bind() {
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(VulkanRenderer::s_commandBuffer, 0, 1, &m_VertexBuffer, offsets);

	vkCmdBindIndexBuffer(VulkanRenderer::s_commandBuffer, m_IndexBuffer, 0,
			 VK_INDEX_TYPE_UINT16);
}

void Model::Draw() {
	vkCmdDrawIndexed(VulkanRenderer::s_commandBuffer,
		       static_cast<uint32_t>(indices.size()), // indexCount
		       1,				      // instanceCount
		       0,				      // firstIndex
		       0,				      // vertexOffset
		       0				      // firstInstance
	);
}
