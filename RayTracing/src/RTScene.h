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
	std::vector<Sphere> spheres;
	std::vector<BoundingBox> boundingBoxes;
	std::vector<RT::Triangle> triangles;
	std::vector<MeshWrapper> meshWrappers;

	void addMesh(const RT::Mesh& mesh)
	{
		auto bvh = BVH(mesh);

		auto trianglesOffset = triangles.size();
		auto boxesOffset = boundingBoxes.size();
		const auto& bvhHierarchy = bvh.getHierarchy();
		auto bvhModel = bvh.buildTriangles();

		boundingBoxes.insert(boundingBoxes.end(), bvhHierarchy.begin(), bvhHierarchy.end());
		triangles.insert(triangles.end(), bvhModel.begin(), bvhModel.end());
		meshWrappers.emplace_back(MeshWrapper{ (uint32_t)boxesOffset, (uint32_t)trianglesOffset, mesh.materialId });

		LOG_DEBUG("Mesh BVH build:");
		bvh.stats.print();
	}
};
