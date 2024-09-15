#pragma once
#include "filesystem"
#include "functional"
#include "unordered_map"
#include "variant"

#include "Engine/Core/Utils.h"
#include "Engine/Render/Scene.h"

#include "tiny_gltf.h"

namespace RT
{

	struct UnknownLoader
	{
	public:
		bool load(const std::filesystem::path& path);
		std::vector<Triangle> buildModel() const;
	};

	struct GltfLoader
	{
	public:
		tinygltf::Model model = {};
		
		bool load(const std::filesystem::path& path);
		std::vector<Triangle> buildModel() const;

	private:
		static bool isBinGltf(const std::filesystem::path& path);
		static constexpr uint32_t primitiveComponentTypeToSize(const uint32_t componentType);
		static constexpr uint32_t primitiveTypeToSize(const uint32_t type);
		static constexpr uint32_t maskPrimitiveType(const uint32_t type);
	};

	class MeshLoader
	{
		using Loader = std::variant<UnknownLoader, GltfLoader>;

	public:
		bool load(const std::filesystem::path& path);

		std::vector<Triangle> buildModel() const;

	private:
		Loader loader = {};

		static const std::unordered_map<std::string, std::function<void(Loader&)>> loaderSelector;
	};

}
