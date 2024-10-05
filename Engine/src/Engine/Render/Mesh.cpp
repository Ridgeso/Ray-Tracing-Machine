#include "Mesh.h"

#include "External/Render/Common/MeshLoader.h"

#include <glm/gtc/matrix_transform.hpp>

namespace RT
{

	Mesh::Mesh(const std::vector<Triangle>& buffer)
		: model{buffer}
		, volume{
			glm::vec3{ std::numeric_limits<float>::max() }, 0,
			glm::vec3{ std::numeric_limits<float>::lowest() }, 0}
	{
		for (const auto& tri : model)
		{
			volume.leftBottomFront = glm::min(volume.leftBottomFront, tri.A);
			volume.leftBottomFront = glm::min(volume.leftBottomFront, tri.B);
			volume.leftBottomFront = glm::min(volume.leftBottomFront, tri.C);
			volume.rightTopBack = glm::max(volume.rightTopBack, tri.A);
			volume.rightTopBack = glm::max(volume.rightTopBack, tri.B);
			volume.rightTopBack = glm::max(volume.rightTopBack, tri.C);
		}
	}
	
	void Mesh::load(const std::filesystem::path& path)
	{
		auto loader = MeshLoader{};
		if (not loader.load(path))
		{
			return;
		}

		model = loader.buildModel();
		volume = loader.buildVolume();
	}

	MeshInstance::MeshInstance(const int32_t meshId)
		: meshId{meshId}
	{
	}

	glm::mat4 MeshInstance::getModelMatrix() const
	{
		constexpr auto rotationAxises = glm::mat3{1.0f};
		auto modelMat = glm::mat4{1.0f};

		modelMat = glm::translate(modelMat, position);

		modelMat = glm::rotate(modelMat, glm::radians(rotation.x), rotationAxises[0]);
		modelMat = glm::rotate(modelMat, glm::radians(rotation.y), rotationAxises[1]);
		modelMat = glm::rotate(modelMat, glm::radians(rotation.z), rotationAxises[2]);
		
		modelMat = glm::scale(modelMat, scale);

		return modelMat;
	}

	glm::mat4 MeshInstance::getInvModelMatrix() const
	{
		return glm::inverse(getModelMatrix());
	}

}
