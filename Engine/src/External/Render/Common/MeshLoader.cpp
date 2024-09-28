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

    Box UnknownLoader::buildVolume() const
    {
        RT_LOG_ERROR("Cannot build Volume from unknown format");
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
            for (const auto& primitive : mesh.primitives)
            {
                if (TINYGLTF_MODE_TRIANGLES != primitive.mode)
                {
                    continue;
                }

                if (-1 == primitive.indices)
                {
                    RT_LOG_WARN("Could not load mesh: {}. No indices found", mesh.name);
                    continue;
                }

                auto primAccessorIdx = primitive.attributes.find("POSITION");
                if (primitive.attributes.end() == primAccessorIdx)
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
                const auto* primRawBuffer = model.buffers[primBufferView.buffer].data.data()
                    + primBufferView.byteOffset
                    + primAccessor.byteOffset;

                const auto& primIndicesAccessor = model.accessors[primitive.indices];
                const auto& primIndicesBufferView = model.bufferViews[primIndicesAccessor.bufferView];
                const auto* primIndicesBuffer = model.buffers[primIndicesBufferView.buffer].data.data()
                    + primIndicesBufferView.byteOffset
                    + primIndicesAccessor.byteOffset;

                uint32_t primIndicesPtr = 0u;
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
                    uint32_t primPtr = *(uint32_t*)(primIndicesBuffer + primIndicesPtr) & maskPrimitiveType(primIndicesAccessor.componentType);
                    std::memcpy(glm::value_ptr(rtModel[lt].A), primRawBuffer + primPtr * primStride, sizeof(glm::vec3));
                    primIndicesPtr += primIndicesIncrement;

                    primPtr = *(uint32_t*)(primIndicesBuffer + primIndicesPtr) & maskPrimitiveType(primIndicesAccessor.componentType);
                    std::memcpy(glm::value_ptr(rtModel[lt].B), primRawBuffer + primPtr * primStride, sizeof(glm::vec3));
                    primIndicesPtr += primIndicesIncrement;

                    primPtr = *(uint32_t*)(primIndicesBuffer + primIndicesPtr) & maskPrimitiveType(primIndicesAccessor.componentType);
                    std::memcpy(glm::value_ptr(rtModel[lt].C), primRawBuffer + primPtr * primStride, sizeof(glm::vec3));
                    primIndicesPtr += primIndicesIncrement;

                    rtModel[lt].uvA = { 0, 0 };
                    rtModel[lt].uvB = { 0, 0 };
                    rtModel[lt].uvC = { 0, 0 };
                }
                //break;
            }
        }

        if (rtModel.empty())
        {
            LOG_WARN("Colnd not find any Triangle primitive. No data in mesh");
        }

        return rtModel;
    }

    Box GltfLoader::buildVolume() const
    {
        auto volume = Box{
            glm::vec3{ std::numeric_limits<float>::max() }, 0,
            glm::vec3{ std::numeric_limits<float>::lowest() }, 0
        };

        for (const auto& mesh : model.meshes)
        {
            for (const auto& primitive : mesh.primitives)
            {
                if (TINYGLTF_MODE_TRIANGLES != primitive.mode)
                {
                    continue;
                }

                auto primAccessorIdx = primitive.attributes.find("POSITION");
                if (primitive.attributes.end() == primAccessorIdx)
                {
                    RT_LOG_WARN("Could not load mesh: {}. No POSITION found", mesh.name);
                    continue;
                }

                const auto& primAccessor = model.accessors[primAccessorIdx->second];

                auto vMin = glm::make_vec3(primAccessor.minValues.data());
                auto vMax = glm::make_vec3(primAccessor.maxValues.data());

                volume.leftBottomFront = glm::min(volume.leftBottomFront, glm::vec3{ vMin });
                volume.rightTopBack = glm::max(volume.rightTopBack, glm::vec3{ vMax });
            }
        }

        return volume;
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
        GltfLoader impl
    */
    bool ObjLoader::load(const std::filesystem::path& path)
    {
        model.open(path);
        return model.is_open();
    }

    std::vector<Triangle> ObjLoader::buildModel() const
    {
        volume.leftBottomFront = glm::vec3{ std::numeric_limits<float>::max() };
        volume.rightTopBack = glm::vec3{ std::numeric_limits<float>::lowest() };

        auto vertices = std::vector<glm::vec3>{};
        vertices.reserve(1000);
        auto texCoords = std::vector<glm::vec2>{};
        texCoords.reserve(1000);
        auto triangles = std::vector<RT::Triangle>{};
        triangles.reserve(1000);

        auto line = std::string{};
        while (std::getline(model, line))
        {
            auto ss = std::stringstream{ line };
            auto prefix = std::string{ "" };
            ss >> prefix;

            if (line.empty() or prefix == "#")
            {
                continue;
            }

            if (prefix == "v")
            {
                auto& v = vertices.emplace_back();
                ss >> v.x >> v.y >> v.z;
            }
            else if (prefix == "vt")
            {
                auto& vt = texCoords.emplace_back();
                ss >> vt.x >> vt.y;
            }
            else if (prefix == "vn")
            {
                // not supported yet
                // for future, normal may not be normalized
            }
            else if (prefix == "l")
            {
                // not supported yet
            }
            else if (prefix == "f")
            {
                const auto setVertex = [&](auto vertexData, auto& v, auto& vt /*, auto& vn*/) -> void
                {
                    int32_t vIdx = 0;
                    int32_t vtIdx = 0;
                    //int32_t vnIdx = 0;

                    size_t delim = vertexData.find("//");
                    if (std::string::npos != delim)
                    {
                        vertexData.replace(delim, 2u, " ");
                        auto vA = std::stringstream{ vertexData };

                        vA >> vIdx;
                        //vA >> vnIdx;
                        v = vertices[vIdx - 1];
                        //vn = vertices[vnIdx - 1];
                        return;
                    }

                    delim = vertexData.find("/");
                    if (std::string::npos != delim)
                    {
                        std::replace(vertexData.begin(), vertexData.end(), '/', ' ');
                        auto vA = std::stringstream{ vertexData };

                        vA >> vIdx;
                        vA >> vtIdx;
                        v = vertices[vIdx - 1];
                        vt = texCoords[vtIdx - 1];
                        //if (' ' == vA.peek())
                        //{
                        //    v >> vnIdx;
                        //    vn = vertices[vnIdx - 1];
                        //}
                    }
                    else
                    {
                        auto vA = std::stringstream{ vertexData };
                        vA >> vIdx;
                        v = vertices[vIdx - 1];
                    }
                };

                auto vertexDataA = std::string{ "" };
                auto vertexDataB = std::string{ "" };
                auto vertexDataC = std::string{ "" };
                ss >> vertexDataA;
                ss >> vertexDataB;
                ss >> vertexDataC;

                auto& face = triangles.emplace_back();
                setVertex(vertexDataA, face.A, face.uvA);
                setVertex(vertexDataB, face.B, face.uvB);
                setVertex(vertexDataC, face.C, face.uvC);

                volume.leftBottomFront = glm::min(volume.leftBottomFront, face.A);
                volume.leftBottomFront = glm::min(volume.leftBottomFront, face.B);
                volume.leftBottomFront = glm::min(volume.leftBottomFront, face.C);
                volume.rightTopBack = glm::max(volume.rightTopBack, face.A);
                volume.rightTopBack = glm::max(volume.rightTopBack, face.B);
                volume.rightTopBack = glm::max(volume.rightTopBack, face.C);

                if (' ' == ss.peek())
                {
                    auto vertexDataD = std::string{ "" };
                    ss >> vertexDataD;
                    auto& additionalFace = triangles.emplace_back();
                    setVertex(vertexDataA, additionalFace.A, additionalFace.uvA);
                    setVertex(vertexDataC, additionalFace.B, additionalFace.uvB);
                    setVertex(vertexDataD, additionalFace.C, additionalFace.uvC);

                    volume.leftBottomFront = glm::min(volume.leftBottomFront, additionalFace.A);
                    volume.leftBottomFront = glm::min(volume.leftBottomFront, additionalFace.B);
                    volume.leftBottomFront = glm::min(volume.leftBottomFront, additionalFace.C);
                    volume.rightTopBack = glm::max(volume.rightTopBack, additionalFace.A);
                    volume.rightTopBack = glm::max(volume.rightTopBack, additionalFace.B);
                    volume.rightTopBack = glm::max(volume.rightTopBack, additionalFace.C);
                }
            }
        }

        return triangles;
    }

    Box ObjLoader::buildVolume() const
    {
        return volume;
    }

    /*
        MeshLoader impl
    */
    #define SELECTOR(STRING_TYPE, LOADER) { std::string{STRING_TYPE}, [](MeshLoader::Loader& loader) { loader = LOADER{}; } }
    const std::unordered_map<std::string, std::function<void(MeshLoader::Loader&)>> MeshLoader::loaderSelector =
    {
        SELECTOR(".gltf", GltfLoader),
        SELECTOR(".glb",  GltfLoader),
        SELECTOR(".obj",  ObjLoader)
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
    
    Box MeshLoader::buildVolume() const
    {
        return std::visit([](const auto& loader) { return loader.buildVolume(); }, loader);
    }

}
