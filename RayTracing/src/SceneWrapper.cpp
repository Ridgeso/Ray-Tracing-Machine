#include "SceneWrapper.h"

#include "Engine/Core/Log.h"

void SceneWrapper::addMesh(const RT::Mesh& mesh)
{
	if (meshLookup.find(&mesh) != meshLookup.end())
	{
		LOG_DEBUG("Mesh found, skipping mesh wrapping");
	}

	const auto bvh = BVH(mesh);

	const int32_t meshId = meshWrappers.size();
	const auto trianglesOffset = triangles.size();
	const auto boxesOffset = boundingBoxes.size();
	const auto& bvhHierarchy = bvh.getHierarchy();
	const auto bvhModel = bvh.buildTriangles();

	boundingBoxes.insert(boundingBoxes.end(), bvhHierarchy.begin(), bvhHierarchy.end());
	triangles.insert(triangles.end(), bvhModel.begin(), bvhModel.end());
	meshWrappers.emplace_back(MeshWrapper{ (uint32_t)boxesOffset, (uint32_t)trianglesOffset });
	meshLookup[&mesh] = meshId;
}

void SceneWrapper::addMeshInstance(const RT::MeshInstance& object)
{
	auto meshId = meshLookup.find(&object.base.get());
	if (meshLookup.end() == meshId)
	{
		LOG_WARN("Mesh not found in SceneWrapper, constructing and adding it to the SceneWrapper");
		addMesh(object.base.get());
		meshId = meshLookup.find(&object.base.get());
	}
	const int32_t objectId = meshInstanceWrappers.size();
	meshInstanceWrappers.emplace_back(MeshInstanceWrapper{ object.getInvModelMatrix(), meshId->second, object.materialId });
	meshInstanceLookup[&object] = objectId;
}

int32_t SceneWrapper::getInstanceWrapperId(const RT::MeshInstance& object) const
{
	auto wrapper = meshInstanceLookup.find(&object);
	if (meshInstanceLookup.end() == wrapper)
	{
		return -1;
	}
	return wrapper->second;
}
