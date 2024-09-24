#include "SceneWrapper.h"

#include "Engine/Core/Log.h"

SceneWrapper::SceneWrapper(RT::Scene& scene)
	: baseScene{ scene }
{
	for (const auto& mesh : baseScene.meshes)
	{
		addMesh(mesh);
	}

	for (const auto& object : baseScene.objects)
	{
		addMeshInstance(object);
	}
}

void SceneWrapper::addMesh(const RT::Mesh& mesh)
{
	const auto bvh = BVH(mesh);

	const int32_t meshId = meshWrappers.size();
	const auto trianglesOffset = triangles.size();
	const auto boxesOffset = boundingBoxes.size();
	const auto& bvhHierarchy = bvh.getHierarchy();
	const auto bvhModel = bvh.buildTriangles();

	boundingBoxes.insert(boundingBoxes.end(), bvhHierarchy.begin(), bvhHierarchy.end());
	triangles.insert(triangles.end(), bvhModel.begin(), bvhModel.end());
	meshWrappers.emplace_back(MeshWrapper{ (uint32_t)boxesOffset, (uint32_t)trianglesOffset });
}

void SceneWrapper::addMeshInstance(const RT::MeshInstance& object)
{
	meshInstanceWrappers.emplace_back(MeshInstanceWrapper{ object.getInvModelMatrix(), object.meshId, object.materialId });
}

void SceneWrapper::removeInstanceWrapper(const uint32_t objectId)
{
	meshInstanceWrappers.erase(meshInstanceWrappers.begin() + objectId);
}
