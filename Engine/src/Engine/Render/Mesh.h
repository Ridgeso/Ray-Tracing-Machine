#pragma once
#include "filesystem"

#include "Scene.h"

namespace RT
{
	
	class Mesh
	{
	public:
		void load(const std::filesystem::path& path);

		const std::vector<Triangle>& getModel() const { return model; }

	private:
		std::vector<Triangle> model;
	};

}
