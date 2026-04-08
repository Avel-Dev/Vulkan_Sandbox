#include "Model.hpp"

#include "VulkanRenderer.h"

#include <cstring>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

void Model::Init() {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(vertices[0]) * vertices.size();
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vkCreateBuffer(VulkanRenderer::device_, &bufferInfo, nullptr, &m_VertexBuffer);

	VkBufferCreateInfo indexBufferInfo{};
	indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexBufferInfo.size = sizeof(indices[0]) * indices.size();
	indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vkCreateBuffer(VulkanRenderer::device_, &indexBufferInfo, nullptr, &m_IndexBuffer);

	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(VulkanRenderer::device_, m_VertexBuffer, &memReqs);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex =
	  findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkAllocateMemory(VulkanRenderer::device_, &allocInfo, nullptr, &m_VertexBufferMemory);
	vkBindBufferMemory(VulkanRenderer::device_, m_VertexBuffer, m_VertexBufferMemory, 0);

	VkMemoryRequirements i_memReqs;
	vkGetBufferMemoryRequirements(VulkanRenderer::device_, m_VertexBuffer, &i_memReqs);

	VkMemoryAllocateInfo i_allocInfo{};
	i_allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	i_allocInfo.allocationSize = i_memReqs.size;
	i_allocInfo.memoryTypeIndex =
	  findMemoryType(i_memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkAllocateMemory(VulkanRenderer::device_, &i_allocInfo, nullptr, &m_IndexBufferMemory);
	vkBindBufferMemory(VulkanRenderer::device_, m_IndexBuffer, m_IndexBufferMemory, 0);

	void* data;
	vkMapMemory(VulkanRenderer::device_, m_VertexBufferMemory, 0, bufferInfo.size, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferInfo.size);
	vkUnmapMemory(VulkanRenderer::device_, m_VertexBufferMemory);

	vkMapMemory(VulkanRenderer::device_, m_IndexBufferMemory, 0, indexBufferInfo.size, 0,
		  &data);
	memcpy(data, indices.data(), (size_t)indexBufferInfo.size);
	vkUnmapMemory(VulkanRenderer::device_, m_IndexBufferMemory);
}

void Model::Bind() {
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(VulkanRenderer::commandbuffer_, 0, 1, &m_VertexBuffer, offsets);

	vkCmdBindIndexBuffer(VulkanRenderer::commandbuffer_, m_IndexBuffer, 0,
			 VK_INDEX_TYPE_UINT16);
}

void Model::Draw() {
	vkCmdDrawIndexed(VulkanRenderer::commandbuffer_,
		       static_cast<uint32_t>(indices.size()), // indexCount
		       1,				      // instanceCount
		       0,				      // firstIndex
		       0,				      // vertexOffset
		       0				      // firstInstance
	);
}
