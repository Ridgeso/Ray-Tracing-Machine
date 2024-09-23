#pragma once
#include "filesystem"

#include <glm/glm.hpp>

#include "Engine/Core/Base.h"

namespace RT
{

	#pragma pack(push, 1)
	struct Triangle
	{
		glm::vec3 A;
		float padding_1;
		glm::vec3 B;
		float padding_2;
		glm::vec3 C;
		float padding_3;
		glm::vec2 uvA;
		glm::vec2 uvB;
		glm::vec2 uvC;
		float padding_4[2];
	};
	#pragma pack(pop)
	
	#pragma pack(push, 1)
	struct Box
	{
		glm::vec3 leftBottomFront;
		float padding_1;
		glm::vec3 rightTopBack;
		float padding_2;
	};
	#pragma pack(pop)
	
	class MeshInstance;

	class Mesh
	{
		friend MeshInstance;
	public:
		void load(const std::filesystem::path& path);

		const std::vector<Triangle>& getModel() const { return model; }
		const Box& getVolume() const { return volume; }

		MeshInstance createInstance();

	private:
		std::vector<Triangle> model;
		Box volume = {};
	};

	class MeshInstance
	{
	public:
		MeshInstance(const Ref<Mesh>& mesh);

		glm::mat4 getModelMatrix() const;
		glm::mat4 getInvModelMatrix() const;

	public:
		glm::vec3 position = {};
		glm::vec3 scale = glm::vec3{1.0f};
		glm::vec3 rotation = {};
		int32_t materialId = 0;
		Ref<Mesh> base;

	private:
	};

}
