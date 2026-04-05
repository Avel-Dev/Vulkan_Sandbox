#include <VulkanRenderer.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

/*
 * PBRMaterial:
  Pipeline* pipeline
  VkDescriptorSet descriptorSet

  PBRMaterialData data

  Texture* albedo
  Texture* normal
  Texture* metallic
  Texture* roughness*/

struct PBRMaterialData {
	glm::vec4 baseColor;
	float roughness;
};

class Texture {};

class Pipeline {};

class Material {
        public:
	void init();
	void Update();
	void Bind();

	void SetUniform(PBRMaterialData materialData);

        private:
	Pipeline* m_Pipeline;
	VkDescriptorSet m_DescriptorSet;
	PBRMaterialData m_MaterialData;

	Texture* m_Albedo;
	Texture* m_Normal;
	Texture* m_Metallic;
	Texture* m_Roughness;
};
