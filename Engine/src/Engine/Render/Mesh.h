#pragma once
#include "filesystem"

#include <glm/glm.hpp>

#include "Engine/Core/Base.h"

namespace RT
{

	#pragma pack(push, 1)
	struct Triangle
	{
		glm::vec3 A; float pad1;
		glm::vec3 B; float pad2;
		glm::vec3 C; float pad3;
		glm::vec2 uvA;
		glm::vec2 uvB;
		glm::vec2 uvC; float pad4[2];

		Triangle() = default;
		Triangle(
			const glm::vec3 A, const glm::vec3 B, const glm::vec3 C,
			const glm::vec2 uvA, const glm::vec2 uvB, const glm::vec2 uvC)
			: A{A}, pad1{}
			, B{B}, pad2{}
			, C{C}, pad3{}
			, uvA{uvA}
			, uvB{uvB}
			, uvC{uvC}, pad4{}
		{}
	};
	#pragma pack(pop)
	
	#pragma pack(push, 1)
	struct Box
	{
		glm::vec3 leftBottomFront; float pad1;
		glm::vec3 rightTopBack; float pad2;
	};
	#pragma pack(pop)
	
	class MeshInstance;

	class Mesh
	{
		friend MeshInstance;
	public:
		Mesh() = default;
		Mesh(const std::vector<Triangle>& buffer);

		void load(const std::filesystem::path& path);

		const std::vector<Triangle>& getModel() const { return model; }
		const Box& getVolume() const { return volume; }

	private:
		std::vector<Triangle> model;
		Box volume = {};
	};

	class MeshInstance
	{
	public:
		MeshInstance(const int32_t meshId);

		glm::mat4 getModelMatrix() const;
		glm::mat4 getInvModelMatrix() const;

	public:
		glm::vec3 position = {};
		glm::vec3 scale = glm::vec3{1.0f};
		glm::vec3 rotation = {};
		int32_t materialId = 0;
		int32_t meshId;

	private:
	};

}
