#pragma once
#include "BVH.h"

#include "Engine/Render/Scene.h"

#pragma pack(push, 1)
struct Sphere
{
	glm::vec3 position;
	float radius;
	int32_t materialId; float pad[3];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MeshWrapper
{
	uint32_t bvhRoot;
	uint32_t modelRoot;
	int32_t materialId; float pad[1];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MeshInstanceWrapper
{
	glm::mat4 worldToLocalMatrix;
	int32_t meshId;
	int32_t materialId; float pad[2];
};
#pragma pack(pop)

class SceneWrapper
{
public:
	SceneWrapper(RT::Scene& scene);
	~SceneWrapper() = default;

	void build();
	void addMesh(const RT::Mesh& mesh);
	void addMeshInstance(const RT::MeshInstance& object);
	void removeInstanceWrapper(const uint32_t objectId);

public:
	std::vector<Sphere> spheres;
	std::vector<BoundingBox> boundingBoxes;
	std::vector<RT::Triangle> triangles;
	std::vector<MeshWrapper> meshWrappers;
	std::vector<MeshInstanceWrapper> meshInstanceWrappers;

private:
	RT::Scene& baseScene;
};
