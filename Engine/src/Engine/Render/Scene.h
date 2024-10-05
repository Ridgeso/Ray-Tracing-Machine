#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "Mesh.h"

namespace RT
{

	#pragma pack(push, 1)
	struct Material
	{
		glm::vec3 albedo; float pad1;
		glm::vec3 emissionColor;
		float roughness;
		float metalic;
		float emissionPower;
		float refractionRatio;
		int32_t textureId;
	};
	#pragma pack(pop)

	struct Scene
	{
		std::vector<Material> materials;
		std::vector<Mesh> meshes;
		std::vector<MeshInstance> objects;
	};

}
