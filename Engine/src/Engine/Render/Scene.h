#pragma once
#include <vector>
#include <glm/glm.hpp>

namespace RT
{

	#pragma pack(push, 1)
	struct Material
	{
		glm::vec3 albedo;
		float padding_1;
		glm::vec3 emissionColor;
		float roughness;
		float metalic;
		float emissionPower;
		float refractionRatio;
		int32_t textureId;
	};
	#pragma pack(pop)

	#pragma pack(push, 1)
	struct Sphere
	{
		glm::vec3 position;
		float radius;
		int32_t materialId;
		float padding_[3];
	};
	#pragma pack(pop)

	#pragma pack(push, 1)
	struct Triangle
	{
		glm::vec3 A;
		float padding_1;
		glm::vec3 B;
		float padding_2;
		glm::vec3 C;
		float padding_3;
		glm::vec2 uvA;
		glm::vec2 uvB;
		glm::vec2 uvC;
		int32_t materialId;
		float padding_4;
	};
	#pragma pack(pop)

	struct Scene
	{
		std::vector<Material> materials;
		std::vector<Sphere> spheres;
		std::vector<Triangle> triangles;
	};

}
