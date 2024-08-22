#pragma once
#include <vector>
#include <glm/glm.hpp>

namespace RT
{

	#pragma pack(push, 1)
	struct Material
	{
		glm::vec3 albedo;
		float padding1[1];
		glm::vec3 emissionColor;
		float roughness;
		float metalic;
		float specularProbability;
		float emissionPower;
		float refractionRatio;
	};
	#pragma pack(pop)

	#pragma pack(push, 1)
	struct Sphere
	{
		glm::vec3 position;
		float radius;
		int32_t materialId;
		float padding[3];
	};
	#pragma pack(pop)

	struct Scene
	{
		std::vector<Material> materials;
		std::vector<Sphere> spheres;
	};

}
