#pragma once
#include <glm/glm.hpp>

#include "Engine/Render/Mesh.h"

struct Node
{
	glm::vec3 vMin;
	glm::vec3 vMax;
	glm::vec3 center;
	//uint32_t index;
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
public:
	BVH(const RT::Mesh& mesh);

	const std::vector<BoundingBox>& getHierarchy() const { return hierarchy; }
	
private:
	void split(const uint32_t parentIdx, glm::uvec2 bufferRegion, const uint8_t depth = maxDepth);

private:
	std::vector<Node> nodes = {};
	std::vector<BoundingBox> hierarchy = {};

	static constexpr uint8_t maxDepth = 0u;
};
