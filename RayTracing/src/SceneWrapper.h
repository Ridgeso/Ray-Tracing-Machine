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

#pragma pack(push, 1)
struct MeshInstanceWrapper
{
	glm::mat4 worldToLocalMatrix;
	int32_t meshId;
	int32_t materialId;
	float padding_2[2];
};
#pragma pack(pop)

class SceneWrapper
{
public:
	SceneWrapper() = default;
	SceneWrapper(const RT::Scene& scene)
		: SceneWrapper{}
	{

	}
	~SceneWrapper() = default;

	void addMesh(const RT::Mesh& mesh);
	void addMeshInstance(const RT::MeshInstance& object);

	int32_t getInstanceWrapperId(const RT::MeshInstance& object) const;

public:
	std::vector<Sphere> spheres;
	std::vector<BoundingBox> boundingBoxes;
	std::vector<RT::Triangle> triangles;
	std::vector<MeshWrapper> meshWrappers;
	std::vector<MeshInstanceWrapper> meshInstanceWrappers;

private:
	std::unordered_map<const RT::Mesh*, int32_t> meshLookup = {};
	std::unordered_map<const RT::MeshInstance*, int32_t> meshInstanceLookup = {};
};
