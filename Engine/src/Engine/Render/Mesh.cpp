#include "Mesh.h"

#include "External/Render/Common/MeshLoader.h"

#include <glm/gtc/matrix_transform.hpp>

namespace RT
{

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

	MeshInstance Mesh::createInstance()
	{
		return MeshInstance(Share<Mesh>{this});
	}

	MeshInstance::MeshInstance(const Share<Mesh>& mesh)
		: base{mesh}
	{
	}

	glm::mat4 MeshInstance::getModelMatrix() const
	{
		constexpr auto rotationAxises = glm::mat3{1.0f};
		auto modelMat = glm::mat4{1.0f};

		modelMat = glm::scale(modelMat, scale);
		
		modelMat = glm::rotate(modelMat, glm::radians(rotaion.x), rotationAxises[0]);
		modelMat = glm::rotate(modelMat, glm::radians(rotaion.y), rotationAxises[1]);
		modelMat = glm::rotate(modelMat, glm::radians(rotaion.z), rotationAxises[2]);

		modelMat = glm::translate(modelMat, pos);

		return modelMat;
	}

	glm::mat4 MeshInstance::getInvModelMatrix() const
	{
		return glm::inverse(getModelMatrix());
	}

}
