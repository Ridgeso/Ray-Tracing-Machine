#include "MeshLoader.h"

#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Log.h"

namespace RT
{

    /*
        GltfLoader impl
    */
    bool UnknownLoader::load(const std::filesystem::path& path)
    {
        RT_LOG_ERROR("Only basic gltf format suppoerted but found {}", path.extension());
        return false;
    }

    std::vector<Triangle> UnknownLoader::buildModel() const
    {
        RT_LOG_ERROR("Cannot build model from unknown format");
        return {};
    }

    /*
        GltfLoader impl
    */
    bool GltfLoader::load(const std::filesystem::path& path)
    {
        using LoaderFuncPtr = bool (tinygltf::TinyGLTF::*)(tinygltf::Model* model, std::string* err, std::string* warn, const std::string& filename, uint32_t checkSections);

        auto loader = LoaderFuncPtr{};

        if (isBinGltf(path))
        {
            RT_LOG_INFO("Reading binary glTF: {}", path);
            loader = &tinygltf::TinyGLTF::LoadBinaryFromFile;
        }
        else
        {
            RT_LOG_INFO("Reading ASCII glTF: {}", path);
            loader = &tinygltf::TinyGLTF::LoadASCIIFromFile;
        }

        auto err = std::string{};
        auto warn = std::string{};
        auto gltfCtx = tinygltf::TinyGLTF{};

        bool ret = (gltfCtx.*loader)(&model, &err, &warn, path.string(), 1u);

        if (not warn.empty()) RT_LOG_WARN("While loading glTF: {}", warn);
        if (not err.empty())  RT_LOG_ERROR("While Loading glTf: {}", err);
        if (not ret)          RT_LOG_CRITICAL("Failed to parse glTF");
        return ret;
    }

    std::vector<Triangle> GltfLoader::buildModel() const
    {
        auto rtModel = std::vector<Triangle>();

        for (const auto& mesh : model.meshes)
        {
            RT_LOG_INFO("Trying to add mesh: {}", mesh.name);
            const auto primitive = std::find_if(
                mesh.primitives.begin(),
                mesh.primitives.end(),
                [](const auto& primitive) { return TINYGLTF_MODE_TRIANGLES == primitive.mode; });

            if (mesh.primitives.end() == primitive)
            {
                RT_LOG_WARN("Could not load mesh: {}. Only Triangle mode supported", mesh.name);
                continue;
            }

            if (-1 == primitive->indices)
            {
                RT_LOG_WARN("Could not load mesh: {}. No indices found", mesh.name);
                continue;
            }

            auto primAccessorIdx = primitive->attributes.find("POSITION");
            if (primitive->attributes.end() == primAccessorIdx)
            {
                RT_LOG_WARN("Could not load mesh: {}. No POSITION found", mesh.name);
                continue;
            }

            const auto& primAccessor = model.accessors[primAccessorIdx->second];
            if (TINYGLTF_COMPONENT_TYPE_FLOAT != primAccessor.componentType)
            {
                RT_LOG_WARN("Could not load mesh: {}. Compoenty type must be FLOAT({})", mesh.name, TINYGLTF_COMPONENT_TYPE_FLOAT);
                continue;
            }
            if (TINYGLTF_TYPE_VEC3 != primAccessor.type)
            {
                RT_LOG_WARN("Could not load mesh: {}. Type must be VEC3({})", mesh.name, TINYGLTF_TYPE_VEC3);
                continue;
            }

            const auto& primBufferView = model.bufferViews[primAccessor.bufferView];
            const auto& primRawBuffer = model.buffers[primBufferView.buffer].data.data() + primBufferView.byteOffset;

            const auto& primIndicesAccessor = model.accessors[primitive->indices];
            const auto& primIndicesBufferView = model.bufferViews[primIndicesAccessor.bufferView];
            const auto& primIndicesBuffer = model.buffers[primIndicesBufferView.buffer];

            uint32_t primIndicesPtr = primIndicesBufferView.byteOffset + primIndicesAccessor.byteOffset;
            uint32_t primIndicesIncrement = primIndicesBufferView.byteStride
                ? primIndicesBufferView.byteStride
                : primitiveComponentTypeToSize(primIndicesAccessor.componentType) * primitiveTypeToSize(primIndicesAccessor.type);
            uint32_t primStride = primBufferView.byteStride
                ? primBufferView.byteStride
                : primitiveComponentTypeToSize(primAccessor.componentType) * primitiveTypeToSize(primAccessor.type);

            constexpr size_t nrOfTrianglesInFace = 3;
            uint32_t lastTriangleSize = rtModel.size();
            rtModel.resize(lastTriangleSize + primIndicesAccessor.count / nrOfTrianglesInFace);
            for (int32_t lt = lastTriangleSize; lt < rtModel.size(); lt++)
            {
                uint32_t primPtr = *(uint32_t*)(primIndicesBuffer.data.data() + primIndicesPtr) & maskPrimitiveType(primIndicesAccessor.componentType);
                std::memcpy(glm::value_ptr(rtModel[lt].A), primRawBuffer + primPtr * primStride, sizeof(glm::vec3));
                primIndicesPtr += primIndicesIncrement;

                primPtr = *(uint32_t*)(primIndicesBuffer.data.data() + primIndicesPtr) & maskPrimitiveType(primIndicesAccessor.componentType);
                std::memcpy(glm::value_ptr(rtModel[lt].B), primRawBuffer + primPtr * primStride, sizeof(glm::vec3));
                primIndicesPtr += primIndicesIncrement;

                primPtr = *(uint32_t*)(primIndicesBuffer.data.data() + primIndicesPtr) & maskPrimitiveType(primIndicesAccessor.componentType);
                std::memcpy(glm::value_ptr(rtModel[lt].C), primRawBuffer + primPtr * primStride, sizeof(glm::vec3));
                primIndicesPtr += primIndicesIncrement;

                rtModel[lt].uvA = { 0, 0 };
                rtModel[lt].uvB = { 0, 0 };
                rtModel[lt].uvC = { 0, 0 };
            }
        }

        return rtModel;
    }

    bool GltfLoader::isBinGltf(const std::filesystem::path& path)
    {
        return ".glb" == path.extension();
    }

    constexpr uint32_t GltfLoader::primitiveComponentTypeToSize(const uint32_t componentType)
    {
        switch (componentType)
        {
            case TINYGLTF_COMPONENT_TYPE_BYTE:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return 1;

            case TINYGLTF_COMPONENT_TYPE_SHORT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                return 2;

            case TINYGLTF_COMPONENT_TYPE_INT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return 4;

            case TINYGLTF_COMPONENT_TYPE_DOUBLE:
                return 8;
            }
        RT_LOG_ERROR("Unknown componentType: {}", componentType);
        return 0;
    }

    constexpr uint32_t GltfLoader::primitiveTypeToSize(const uint32_t type)
    {
        switch (type)
        {
            case TINYGLTF_TYPE_SCALAR: return 1;
            case TINYGLTF_TYPE_VEC2:   return 2;
            case TINYGLTF_TYPE_VEC3:   return 3;
            case TINYGLTF_TYPE_VEC4:   return 4;
        }
        RT_LOG_ERROR("Unknown type: {}", type);

        return 0;
    }

    constexpr uint32_t GltfLoader::maskPrimitiveType(const uint32_t type)
    {
        constexpr uint8_t bitsInByte = 8u;
        constexpr uint8_t uintBitSize = sizeof(uint32_t) * bitsInByte;
        constexpr uint32_t mask = ~0u;
        return (mask >> (uintBitSize - bitsInByte * primitiveComponentTypeToSize(type)));
    }

    /*
        MeshLoader impl
    */
    #define SELECTOR(STRING_TYPE, LOADER) { std::string{STRING_TYPE}, [](MeshLoader::Loader& loader) { loader = LOADER{}; } }
    const std::unordered_map<std::string, std::function<void(MeshLoader::Loader&)>> MeshLoader::loaderSelector =
    {
        SELECTOR(".gltf", GltfLoader),
        SELECTOR(".glb",  GltfLoader)
    };

    bool MeshLoader::load(const std::filesystem::path& path)
    {
        const auto& selector = loaderSelector.find(path.extension().string());
        if (loaderSelector.end() == selector)
        {
            loader = UnknownLoader{};
        }
        else
        {
            selector->second(loader);
        }

        return std::visit([&path](auto& loader) { return loader.load(path); }, loader);
    }

    std::vector<Triangle> MeshLoader::buildModel() const
    {
        return std::visit([](const auto& loader) { return loader.buildModel(); }, loader);
    }

}
