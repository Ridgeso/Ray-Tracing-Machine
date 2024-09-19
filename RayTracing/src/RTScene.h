#pragma once
#include "BVH.h"

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
	uint32_t bvhRoot;
	uint32_t modelRoot;
	int32_t materialId;
	float padding_[1];
};
#pragma pack(pop)

struct RTScene
{
	std::vector<RT::Triangle> triangles;
	std::vector<Sphere> spheres;
	std::vector<BoundingBox> boundingBoxes;
	std::vector<MeshWrapper> meshWrappers;

	void addMesh(const RT::Mesh& mesh)
	{
		meshWrappers.emplace_back(MeshWrapper{ (uint32_t)boundingBoxes.size(), (uint32_t)triangles.size(), mesh.materialId });
		triangles.insert(triangles.end(), mesh.getModel().begin(), mesh.getModel().end());
		auto bvh = BVH(mesh);
		boundingBoxes.insert(boundingBoxes.end(), bvh.getHierarchy().begin(), bvh.getHierarchy().end());
	}
};
