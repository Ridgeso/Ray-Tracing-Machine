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

        loadUniforms();
	}

    void OpenGlShader::setUniformImpl(const std::string& uniName, const int32_t size, const void* value) const
    {
        const auto& pos = uniforms.find(uniName);
        if (pos == uniforms.end())
        {
            return;
        }

        if (size == 1)
        {
            switch (pos->second.type)
            {
                case Uniform::Type::Int: glUniform1i(pos->second.id, *(int32_t*)value); break;
                case Uniform::Type::Uint: glUniform1ui(pos->second.id, *(uint32_t*)value); break;
                case Uniform::Type::Float: glUniform1f(pos->second.id, *(float*)value); break;
                case Uniform::Type::Float2: glUniform2f(pos->second.id, ((glm::vec2*)value)->x, ((glm::vec2*)value)->y); break;
            }
        }
        else
        {
            switch (pos->second.type)
            {
                case Uniform::Type::Buffer: setStorage(pos->second.id, size, value); break;
            }
        }
    }

    void OpenGlShader::setStorage(const int32_t pos, const size_t size, const void* data) const
    {
        const auto storageId = storages.at(pos);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, storageId);
        glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, pos, storageId);
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

    void OpenGlShader::loadUniforms()
    {
        constexpr size_t bufSize = 64;
        char name[bufSize];
        int32_t count, size, length;
        uint32_t type;

        glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &count);
        for (auto i = 0u; i < count; i++)
        {
            glGetActiveUniform(programId, i, bufSize, &length, &size, &type, name);
            uniforms[std::string(name)] = { glUniType2UniType(type), glGetUniformLocation(programId, name)};
        }

        glGetProgramInterfaceiv(programId, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &count);
        for (int i = 0; i < count; ++i) {
            constexpr uint32_t props[] = { GL_BUFFER_BINDING };
            constexpr uint32_t propsSize = sizeof(props) / sizeof(uint32_t);
            int32_t values[propsSize];

            glGetProgramResourceiv(programId, GL_SHADER_STORAGE_BLOCK, i, propsSize, props, propsSize, &length, values);
            glGetProgramResourceName(programId, GL_SHADER_STORAGE_BLOCK, i, bufSize, &length, name);
            uniforms[std::string(name)] = { Uniform::Type::Buffer, values[0]};
            glGenBuffers(1, &storages[values[0]]);
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

    constexpr Uniform::Type OpenGlShader::glUniType2UniType(const uint32_t glUniType)
    {
        switch (glUniType)
        {
            case GL_SAMPLER_2D:   return Uniform::Type::Int;
            case GL_INT:          return Uniform::Type::Int;
            case GL_UNSIGNED_INT: return Uniform::Type::Uint;
            case GL_FLOAT:        return Uniform::Type::Float;
            case GL_FLOAT_VEC2:   return Uniform::Type::Float2;
        }
        return Uniform::Type::None;
    }

}
