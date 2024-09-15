#include "Mesh.h"

#include "External/Render/Common/MeshLoader.h"

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
	}

}
