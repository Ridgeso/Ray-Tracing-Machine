#include "BVH.h"

#include "Engine/Core/Time.h"
#include "Engine/Core/Log.h"

namespace
{

	void growToInclude(const Node& node, BoundingBox& box)
	{
		box.vMin = glm::min(box.vMin, node.vMin);
		box.vMax = glm::max(box.vMax, node.vMax);
	}

	float area(const BoundingBox& box)
	{
		
		if (glm::any(glm::greaterThan(box.vMin, box.vMax)))
		{
			return 0.0f;
		}
		auto size = box.vMin - box.vMax;
		auto halfArea = size.x * size.y + size.y * size.z + size.x * size.z;
		return halfArea * 2.0f;
	}

	float calculateCost(const BoundingBox& box, const float primitiveCost)
	{
		return area(box) * primitiveCost;
	}

}

BVH::BVH(const RT::Mesh& mesh)
	: mesh{mesh}
{
	auto buildTimer = RT::Timer{};
	
	buildNodes();
	construct();

	stats.buildTime = buildTimer.Ellapsed();
}

std::vector<RT::Triangle> BVH::buildTriangles() const
{
	const auto& triangles = mesh.getModel();
	auto alignedTriangles = std::vector<RT::Triangle>();
	alignedTriangles.reserve(nodes.size());

	for (const auto& index : indices)
	{
		alignedTriangles.push_back(triangles[index]);
	}

	return alignedTriangles;
}

void BVH::buildNodes()
{
	const auto& triangles = mesh.getModel();
	indices.reserve(triangles.size());
	nodes.reserve(triangles.size());
	uint32_t index = 0u;
	for (const auto& triangle : triangles)
	{
		indices.push_back(index++);
	
		auto& node = nodes.emplace_back();
		node.vMin = glm::min(glm::min(triangle.A, triangle.B), triangle.C);
		node.vMax = glm::max(glm::max(triangle.A, triangle.B), triangle.C);
		node.center = (triangle.A + triangle.B + triangle.C) / 3.0f;
	}

	stats.triCnt = triangles.size();
}

void BVH::construct()
{
	const auto& meshVolume = mesh.getVolume();
	
	auto& rootBoundingBox = hierarchy.emplace_back();
	rootBoundingBox.vMin = meshVolume.leftBottomFront;
	rootBoundingBox.vMax = meshVolume.rightTopBack;
	rootBoundingBox.bufferRegion = glm::uvec3{ 0u };

	split(0, { 0, nodes.size() });

	stats.nodeCnt = hierarchy.size();
}

void BVH::split(const uint32_t parentIdx, const glm::uvec2 bufferRegion, const uint8_t depth)
{
	const uint32_t triangleCount = bufferRegion.y - bufferRegion.x;
	
	auto splitInfo = splitAxis(hierarchy[parentIdx], bufferRegion);
	auto paretnCost = calculateCost(hierarchy[parentIdx], triangleCount);

	if (maxDepth == depth or splitInfo.cost >= paretnCost)
	{
		stats.leafCnt++;
		stats.leafDepth.x = depth < stats.leafDepth.x ? depth : stats.leafDepth.x;
		stats.leafDepth.y = depth >= stats.leafDepth.y ? depth : stats.leafDepth.y;
		stats.leafDepthSum += depth;
		stats.leafTris.x = triangleCount < stats.leafTris.x ? triangleCount : stats.leafTris.x;
		stats.leafTris.y = triangleCount >= stats.leafTris.y ? triangleCount : stats.leafTris.y;
		stats.leafTrisSum += triangleCount;

		hierarchy[parentIdx].bufferRegion = bufferRegion;
		return;
	}

	auto leftChild = emptyBox;
	auto rightChild = emptyBox;

	uint32_t bufferCenter = bufferRegion.x;
	for (uint32_t i = bufferRegion.x; i < bufferRegion.y; i++)
	{
		const auto& node = nodes[indices[i]];
		if (node.center[splitInfo.axis] < splitInfo.position)
		{
			growToInclude(node, leftChild);

			std::swap(indices[bufferCenter], indices[i]);
			bufferCenter++;
		}
		else
		{
			growToInclude(node, rightChild);
		}
	}

	hierarchy[parentIdx].bufferRegion.x = hierarchy.size();
	hierarchy[parentIdx].bufferRegion.y = 0u;
	hierarchy.push_back(leftChild);
	hierarchy.push_back(rightChild);
	
	split(hierarchy[parentIdx].bufferRegion.x, { bufferRegion.x, bufferCenter }, depth + 1u);
	split(hierarchy[parentIdx].bufferRegion.x + 1u, { bufferCenter, bufferRegion.y }, depth + 1u);
}

BVH::Split BVH::splitAxis(const BoundingBox& box, const glm::uvec2 bufferRegion) const
{
	auto jump = (bufferRegion.y - bufferRegion.x) / 5u + 1u;

	auto split = Split{ std::numeric_limits<float>::max(), 0.0f, 0 };
	for (uint8_t axis = 0u; axis < 3u; axis++)
	{
		for (uint32_t i = bufferRegion.x; i < bufferRegion.y; i += jump)
		{
			float position = nodes[indices[i]].center[axis];
			float cost = evaluateCost(axis, position, bufferRegion);
			if (cost < split.cost)
			{
				split = Split{ cost, position, axis };
			}
		}
	}

	return split;
}

float BVH::evaluateCost(const uint8_t axis, const float position, const glm::uvec2 bufferRegion) const
{
	auto left = emptyBox;
	auto right = emptyBox;
	float leftCnt = 0.0f;
	float rightCnt = 0.0f;

	for (uint32_t i = bufferRegion.x; i < bufferRegion.y; i++)
	{
		auto node = nodes[indices[i]];
		if (node.center[axis] < position)
		{
			growToInclude(node, left);
			leftCnt++;
		}
		else
		{
			growToInclude(node, right);
			rightCnt++;
		}
	}

	return calculateCost(left, leftCnt) + calculateCost(right, rightCnt);
}

void BVH::Stats::print() const
{
	LOG_DEBUG("BVH buildTime: {} ms", buildTime);
	LOG_DEBUG("BVH triangles: {}", triCnt);
	LOG_DEBUG("BVH nodes: {}", nodeCnt);
	LOG_DEBUG("BVH leaf: {}", leafCnt);
	LOG_DEBUG("BVH leaf Depth: Min: {} Max: {} Mean: {}", leafDepth.x, leafDepth.y, meanDepth());
	LOG_DEBUG("BVH leaf Tris : Min: {} Max: {} Mean: {}", leafTris.x, leafTris.y, meanTris());
}
