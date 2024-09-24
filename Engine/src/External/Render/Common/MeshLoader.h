#pragma once
#include <fstream>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <variant>

#include "Engine/Core/Utils.h"
#include "Engine/Render/Scene.h"

#include "tiny_gltf.h"

namespace RT
{

	#define LOADER_IMPL								  \
		bool load(const std::filesystem::path& path); \
		std::vector<Triangle> buildModel() const;	  \
		Box buildVolume() const;

	class UnknownLoader
	{
	public:
		LOADER_IMPL
	};

	class GltfLoader
	{
	public:
		LOADER_IMPL

	private:
		static bool isBinGltf(const std::filesystem::path& path);
		static constexpr uint32_t primitiveComponentTypeToSize(const uint32_t componentType);
		static constexpr uint32_t primitiveTypeToSize(const uint32_t type);
		static constexpr uint32_t maskPrimitiveType(const uint32_t type);

	private:
		tinygltf::Model model = {};
	};

	class ObjLoader
	{
	public:
		LOADER_IMPL

	private:
		mutable Box volume = {};
		mutable std::ifstream model = {};
	};

	class MeshLoader
	{
		using Loader = std::variant<UnknownLoader, GltfLoader, ObjLoader>;

	public:
		bool load(const std::filesystem::path& path);

		std::vector<Triangle> buildModel() const;
		Box buildVolume() const;

	private:
		Loader loader = {};

		static const std::unordered_map<std::string, std::function<void(Loader&)>> loaderSelector;
	};

}
