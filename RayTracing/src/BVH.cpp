#include "BVH.h"

#include <array>

#include "Engine/Core/Time.h"
#include "Engine/Core/Log.h"

namespace
{

	struct Bucket
	{
		BoundingBox bounds;
		uint32_t cnt;
	};

	struct BucketStat
	{
		float area;
		uint32_t sumCnt;
	};

	void extendBox(BoundingBox& box, const Node& node)
	{
		box.vMin = glm::min(box.vMin, node.vMin);
		box.vMax = glm::max(box.vMax, node.vMax);
	}

	void growBox(BoundingBox& box, const BoundingBox& extent)
	{
		box.vMin = glm::min(box.vMin, extent.vMin);
		box.vMax = glm::max(box.vMax, extent.vMax);
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
	
	auto splitInfo = splitBox(hierarchy[parentIdx], bufferRegion);
	auto parentCost = area(hierarchy[parentIdx]) * triangleCount;

	if (maxDepth == depth or splitInfo.cost >= parentCost)
	{
		stats.measure(depth, triangleCount, parentCost);

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
			extendBox(leftChild, node);

			std::swap(indices[bufferCenter], indices[i]);
			bufferCenter++;
		}
		else
		{
			extendBox(rightChild, node);
		}
	}

	hierarchy[parentIdx].bufferRegion.x = hierarchy.size();
	hierarchy[parentIdx].bufferRegion.y = 0u;
	hierarchy.push_back(leftChild);
	hierarchy.push_back(rightChild);
	
	split(hierarchy[parentIdx].bufferRegion.x, { bufferRegion.x, bufferCenter }, depth + 1u);
	split(hierarchy[parentIdx].bufferRegion.x + 1u, { bufferCenter, bufferRegion.y }, depth + 1u);
}	

BVH::Split BVH::splitBox(const BoundingBox& box, const glm::uvec2 bufferRegion) const
{
	auto bestSplit = Split{ std::numeric_limits<float>::max(), 0.0f, 0 };
	
	if (0u == bufferRegion.y - bufferRegion.x)
	{
		return bestSplit;
	}

	for (uint8_t axis = 0u; axis < 3u; axis++)
	{
		const auto centerBounds = findCenterBounds(axis, bufferRegion);
		if (centerBounds.x == centerBounds.y)
		{
			continue;
		}

		auto split = splitAxis(axis, bufferRegion, centerBounds);

		if (split.cost < bestSplit.cost)
		{
			bestSplit = split;
		}
	}
	return bestSplit;
}

BVH::Split BVH::splitAxis(const uint8_t axis, const glm::uvec2 bufferRegion, const glm::vec2 bounds) const
{
	const float subplaneInterval = (float)nrOfSubplanes / (bounds.y - bounds.x);
	auto buckets = std::array<Bucket, nrOfSubplanes>{};
	buckets.fill({ emptyBox, 0u });

	for (uint32_t i = bufferRegion.x; i < bufferRegion.y; i++)
	{
		const auto& node = nodes[indices[i]];
		const uint32_t idx = glm::min(nrOfSubplanes - 1, (uint32_t)((node.center[axis] - bounds.x) * subplaneInterval));
		extendBox(buckets[idx].bounds, node);
		buckets[idx].cnt++;
	}

	auto leftStat = std::array<BucketStat, nrOfSubplanes - 1u>{};
	auto rightStat = std::array<BucketStat, nrOfSubplanes - 1u>{};
	auto leftBox = Bucket{ emptyBox, 0u };
	auto rightBox = Bucket{ emptyBox, 0u };
	for (uint32_t li = 0u; li < nrOfSubplanes - 1u; li++)
	{
		leftBox.cnt += buckets[li].cnt;
		leftStat[li].sumCnt = leftBox.cnt;
		growBox(leftBox.bounds, buckets[li].bounds);
		leftStat[li].area = area(leftBox.bounds);

		auto ri = nrOfSubplanes - 1u - li;
		rightBox.cnt += buckets[ri].cnt;
		rightStat[ri - 1u].sumCnt = rightBox.cnt;
		growBox(rightBox.bounds, buckets[li].bounds);
		rightStat[ri - 1u].area = area(rightBox.bounds);
	}

	const float subplaneSize = (bounds.y - bounds.x) / (float)nrOfSubplanes;
	auto bestSplit = Split{ std::numeric_limits<float>::max(), 0.0f, 0 };
	for (uint32_t i = 0u; i < nrOfSubplanes - 1u; i++)
	{
		float planeCost = leftStat[i].sumCnt * leftStat[i].area + rightStat[i].sumCnt * rightStat[i].area;
		if (planeCost < bestSplit.cost)
		{
			bestSplit = Split{ planeCost, bounds.x + subplaneSize * (i + 1.0f), axis};
		}
	}

	return bestSplit;
}

glm::vec2 BVH::findCenterBounds(const uint8_t axis, const glm::uvec2 bufferRegion) const
{
	float leftCenter = std::numeric_limits<float>::max();
	float rightCenter = std::numeric_limits<float>::lowest();
	for (uint32_t i = bufferRegion.x; i < bufferRegion.y; i++)
	{
		const auto& node = nodes[indices[i]];
		leftCenter = glm::min(leftCenter, node.center[axis]);
		rightCenter = glm::max(rightCenter, node.center[axis]);
	}
	return { leftCenter, rightCenter };
}

void BVH::Stats::measure(const uint8_t depth, const uint32_t triangleCount, const float cost)
{
	leafCnt++;
	leafDepth.x = depth < leafDepth.x ? depth : leafDepth.x;
	leafDepth.y = depth >= leafDepth.y ? depth : leafDepth.y;
	leafDepthSum += depth;
	leafTris.x = triangleCount < leafTris.x ? triangleCount : leafTris.x;
	leafTris.y = triangleCount >= leafTris.y ? triangleCount : leafTris.y;
	leafTrisSum += triangleCount;
	SAH += cost;
}

void BVH::Stats::print() const
{
	LOG_DEBUG("BVH buildTime: {} ms", buildTime);
	LOG_DEBUG("BVH triangles = {} nodes = {} leafs = {}", triCnt, nodeCnt, leafCnt);
	LOG_DEBUG("BVH leaf Depth: Min = {} Max = {} Mean = {}", leafDepth.x, leafDepth.y, meanDepth());
	LOG_DEBUG("BVH leaf Tris:  Min = {} Max = {} Mean = {} SAH on leafs = {}", leafTris.x, leafTris.y, meanTris(), SAH);
}
