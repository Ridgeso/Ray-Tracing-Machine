#pragma once
#include <glm/glm.hpp>

#include "Engine/Render/Mesh.h"

struct Node
{
	glm::vec3 vMin;
	glm::vec3 vMax;
	glm::vec3 center;
};

#pragma pack(push, 1)
struct BoundingBox
{
	glm::vec3 vMin;
	float padding_1;
	glm::vec3 vMax;
	float padding_2;
	glm::uvec2 bufferRegion;
	float padding_3[2];
};
#pragma pack(pop)

class BVH
{
private:
	struct Split
	{
		float cost;
		float position;
		uint8_t axis;
	};

public:
	struct Stats
	{
		float buildTime;
		uint32_t triCnt;
		uint32_t nodeCnt;
		uint32_t leafCnt;
		glm::uvec2 leafDepth;
		float leafDepthSum;
		glm::uvec2 leafTris;
		float leafTrisSum;
		float SAH;

		float meanDepth() const { return leafDepthSum / leafCnt; }
		float meanTris() const { return leafTrisSum / leafCnt; }
		void measure(const uint8_t depth, const uint32_t triangleCount, const float cost);
		void print() const;
	} stats = { 0, 0, 0, 0, { 100, 0 }, 0, { 1000000, 0 }, 0, 0 };

public:
	BVH(const RT::Mesh& mesh);

	std::vector<RT::Triangle> buildTriangles() const;
	const std::vector<BoundingBox>& getHierarchy() const { return hierarchy; }
	
private:
	void buildNodes();
	void construct();
	void split(const uint32_t parentIdx, const glm::uvec2 bufferRegion, const uint8_t depth = 0u);
	Split splitBox(const BoundingBox& box, const glm::uvec2 bufferRegion) const;
	Split splitAxis(const uint8_t axis, const glm::uvec2 bufferRegion, const glm::vec2 bounds) const;
	glm::vec2 findCenterBounds(const uint8_t axis, const glm::uvec2 bufferRegion) const;

private:
	const RT::Mesh& mesh;
	std::vector<uint32_t> indices = {};
	std::vector<Node> nodes = {};
	std::vector<BoundingBox> hierarchy = {};

	static constexpr uint8_t maxDepth = 32u;
	static constexpr uint32_t nrOfSubplanes = 6u;
	static constexpr BoundingBox emptyBox = {
		glm::vec3{std::numeric_limits<float>::max()}, 0.0f,
		glm::vec3{std::numeric_limits<float>::lowest()}, 0.0f,
		glm::uvec2{0u}
	};
};
