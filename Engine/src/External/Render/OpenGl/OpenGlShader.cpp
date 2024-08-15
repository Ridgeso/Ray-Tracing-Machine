#include "OpenGlShader.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>

#include "Engine/Core/Log.h"
#include "Engine/Core/Assert.h"

namespace RT::OpenGl
{

    void OpenGlShader::use() const
	{
		glUseProgram(programId);
	}

	void OpenGlShader::unuse() const
	{
		glUseProgram(0);
	}

    void OpenGlShader::destroy()
    {
        glDeleteProgram(programId);
        for (const auto& storId : storages)
        {
            glDeleteBuffers(1, &storId.second);
        }
        uniforms.clear();
        storages.clear();
    }

	void OpenGlShader::load(const std::string& shaderPath)
	{
        auto shadersSource = readSources(shaderPath);
        RT_CORE_ASSERT(shadersSource.size() != 0, "Failed to find source code!\n{}", shaderPath);

        auto shaderIds = std::vector<uint32_t>();
        programId = glCreateProgram();
        for (const auto& [type, source] : shadersSource)
        {
            auto shaderId = shaderIds.emplace_back(compile(type, source.str()));
            glAttachShader(programId, shaderId);
        }

        glLinkProgram(programId);
        logLinkError(shaderPath);

        for (auto id : shaderIds)
        {
            glDetachShader(programId, id);
            glDeleteShader(id);
        }

        reflect();
	}

    std::unordered_map<OpenGlShader::Type, std::stringstream> OpenGlShader::readSources(const std::string& shaderPath) const
    {
        auto shaders = std::ifstream(shaderPath, std::ios::in);
        if (!shaders.is_open())
        {
            return {};
        }

        auto shadersSource = std::unordered_map<Type, std::stringstream>();
        auto foundShaders = std::unordered_set<Type>();

        auto shaderType = Type::None;
        auto line = std::string();
        for (uint32_t lineNr = 1u; std::getline(shaders, line); lineNr++)
        {
            if (line.find("###SHADER") != std::string::npos)
            {
                shaderType
                    = line.find("VERTEX")          != std::string::npos ? Type::Vertex
                    : line.find("TESS_CONTROL")    != std::string::npos ? Type::TessControl
                    : line.find("TESS_EVAULATION") != std::string::npos ? Type::TessEvaulation
                    : line.find("GEOMETRY")        != std::string::npos ? Type::Geometry
                    : line.find("FRAGMENT")        != std::string::npos ? Type::Fragment
                    : line.find("FRAGMENT")        != std::string::npos ? Type::Compute
                    : Type::None;

                if (foundShaders.find(shaderType) == foundShaders.end())
                {
                    foundShaders.insert(shaderType);
                    shadersSource[shaderType] << std::string(lineNr, '\n');
                }
            }
            else if (shaderType != Type::None)
            {
                shadersSource[shaderType] << line << '\n';
            }
        }

        return shadersSource;
    }

    uint32_t OpenGlShader::compile(const Type type, const std::string& source) const
    {
        const auto glShaderType = shaderType2GlType(type);
        auto shaderId = static_cast<uint32_t>(glCreateShader(glShaderType));

        auto sourceData = source.c_str();
        auto sourceLenght = static_cast<int32_t>(source.length());
        glShaderSource(shaderId, 1, &sourceData, &sourceLenght);
        glCompileShader(shaderId);

        return shaderId;
    }

    // TODO: Leave for debugging purpose and delete unnecessery maps
    void OpenGlShader::reflect()
    {
        constexpr size_t bufSize = 64;
        char name[bufSize];
        int32_t count, size, length;
        uint32_t type;

        glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &count);
        for (auto i = 0u; i < count; i++)
        {
            glGetActiveUniform(programId, i, bufSize, &length, &size, &type, name);
            int32_t uniformType = glGetUniformLocation(programId, name);
            uniforms[std::string(name)] = uniformType;
        }

        glGetProgramInterfaceiv(programId, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &count);
        for (int i = 0; i < count; ++i) {
            constexpr uint32_t props[] = { GL_BUFFER_BINDING };
            constexpr uint32_t propsSize = sizeof(props) / sizeof(uint32_t);
            int32_t values[propsSize];

            glGetProgramResourceiv(programId, GL_SHADER_STORAGE_BLOCK, i, propsSize, props, propsSize, &length, values);
            glGetProgramResourceName(programId, GL_SHADER_STORAGE_BLOCK, i, bufSize, &length, name);
            int32_t itIsStorageType = values[0];
            uniforms[std::string(name)] = itIsStorageType;
            glCreateBuffers(1, &storages[values[0]]);
        }
    }

    void OpenGlShader::logLinkError(const std::string& shaderPath) const
    {
        int32_t isLinked;
        glGetProgramiv(programId, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength;
            glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<char> infoLog(maxLength);
            glGetProgramInfoLog(programId, maxLength, &maxLength, infoLog.data());
            RT_LOG_ERROR("[Shader] Linking failed {}:\n{}", shaderPath, infoLog.data());

            glDeleteProgram(programId);
        }
    }

    constexpr uint32_t OpenGlShader::shaderType2GlType(const Type type)
    {
        switch (type)
        {
            case Type::Vertex:         return GL_VERTEX_SHADER;
            case Type::TessEvaulation: return GL_TESS_EVALUATION_SHADER;
            case Type::TessControl:    return GL_TESS_CONTROL_SHADER;
            case Type::Geometry:       return GL_GEOMETRY_SHADER;
            case Type::Fragment:       return GL_FRAGMENT_SHADER;
            case Type::Compute:        return GL_COMPUTE_SHADER;
        }
        return 0u;
    }

}
