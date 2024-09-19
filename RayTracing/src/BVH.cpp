#include "BVH.h"

#include "Engine/Core/Log.h"

namespace
{

	void growToInclude(const Node& node, BoundingBox& box)
	{
		box.vMin = glm::min(box.vMin, node.vMin);
		box.vMax = glm::max(box.vMax, node.vMax);
	}

	int8_t splitAxis(const BoundingBox& box)
	{
		auto size = box.vMax - box.vMin;
		int8_t axis = 0;
		if (size.x < size.y)
		{
			axis = 1;
		}
		if (size[axis] < size.z)
		{
			axis = 2;
		}
		return axis;
	}

}

BVH::BVH(const RT::Mesh& mesh)
{
	const auto& triangles = mesh.getModel();
	const auto& meshVolume = mesh.getVolume();

	nodes.reserve(triangles.size());
	for (const auto& triangle : triangles)
	{
		auto node = Node{};
		node.vMin = glm::min(glm::min(triangle.A, triangle.B), triangle.C);
		node.vMax = glm::max(glm::max(triangle.A, triangle.B), triangle.C);
		node.center = (triangle.A + triangle.B + triangle.C) / 3.0f;
		//node.index = nodes.size();
		nodes.push_back(node);
	}

	auto& rootBoundingBox = hierarchy.emplace_back();
	rootBoundingBox.vMin = meshVolume.leftBottomFront;
	rootBoundingBox.vMax = meshVolume.rightTopBack;
	rootBoundingBox.bufferRegion = glm::uvec3{0u};

	split(0, { 0, nodes.size() - 1 });
}

void BVH::split(const uint32_t parentIdx, glm::uvec2 bufferRegion, const uint8_t depth)
{
	if (depth == 0)
	{
		hierarchy[parentIdx].bufferRegion = bufferRegion;
		return;
	}
	
	uint32_t leftBoxSize = 0;
	auto axis = splitAxis(hierarchy[parentIdx]);
	auto boxAxisCenter = (hierarchy[parentIdx].vMax[axis] + hierarchy[parentIdx].vMin[axis]) / 2.0f;

	auto leftChild = BoundingBox{};
	auto rightChild = BoundingBox{};

	for (uint32_t i = bufferRegion.x; i <= bufferRegion.y; i++)
	{
		if (nodes[i].center[axis] < boxAxisCenter)
		{
			growToInclude(nodes[i], leftChild);

			std::swap(nodes[i], nodes[bufferRegion.x + leftBoxSize]);
			leftBoxSize++;
		}
		else
		{
			growToInclude(nodes[i], rightChild);
		}
	}

	hierarchy[parentIdx].bufferRegion.x = hierarchy.size();
	hierarchy[parentIdx].bufferRegion.y = 0u;
	hierarchy.push_back(leftChild);
	hierarchy.push_back(rightChild);
		
	split(hierarchy[parentIdx].bufferRegion.x, { bufferRegion.x, bufferRegion.x + leftBoxSize }, depth - 1);
	split(hierarchy[parentIdx].bufferRegion.x + 1, { bufferRegion.x + leftBoxSize + 1u, bufferRegion.y }, depth - 1);
}
