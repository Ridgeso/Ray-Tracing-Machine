#pragma once
#include "Engine/Render/Scene.h"

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
struct MeshWrapper
{
	RT::Box volume;
	glm::uvec2 bufferRegion;
	int32_t materialId;
	float padding_1;
};
#pragma pack(pop)

struct RTScene
{
	std::vector<RT::Triangle> triangles;
	std::vector<Sphere> spheres;
	std::vector<MeshWrapper> meshWrappers;

	void addMesh(const RT::Mesh& mesh)
	{
		uint32_t lastTrianglesSize = triangles.size();
		triangles.insert(triangles.end(), mesh.getModel().begin(), mesh.getModel().end());
		meshWrappers.emplace_back(MeshWrapper{ mesh.getVolume(), { lastTrianglesSize , triangles.size() - 1 }, 0 });
	}
};
