#include "OpenGlBuffer.h"

#include <glad/glad.h>

#include "OpenGlTexture.h"

namespace RT::OpenGl
{

	OpenGlVertexBuffer::OpenGlVertexBuffer(const uint32_t size)
		: bufferId{}, size{size}, count{0}
	{
		glCreateBuffers(1, &bufferId);
		glBindBuffer(GL_ARRAY_BUFFER, bufferId);
		glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGlVertexBuffer::OpenGlVertexBuffer(const uint32_t size, const void* data)
		: OpenGlVertexBuffer(size)
	{
		glCreateBuffers(1, &bufferId);
		glBindBuffer(GL_ARRAY_BUFFER, bufferId);
		glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
	}

	OpenGlVertexBuffer::~OpenGlVertexBuffer()
	{
		glDeleteBuffers(1, &bufferId);
	}

	void OpenGlVertexBuffer::registerAttributes(const VertexElements& elements) const
	{
		bind();
		const auto stride = calculateStride(elements);
		auto sumOfComp = 0;
		auto offset = 0;
		for (auto pos = 0; pos < elements.size(); pos++)
		{
			const auto nrOfComp = getNrOfComponents(elements[pos]);
			sumOfComp += nrOfComp;
			glVertexAttribPointer(pos, nrOfComp, elementType2GlType(elements[pos]), GL_FALSE, stride, (void*)offset);
			glEnableVertexAttribArray(pos);
			offset += elementType2Size(elements[pos]);
		}
		unbind();

		count = size / stride * sumOfComp;
	}

	void OpenGlVertexBuffer::setData(const uint32_t size, const void* data) const
	{
		glNamedBufferSubData(bufferId, 0, size, data);
	}

	void OpenGlVertexBuffer::bind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, bufferId);
	}
	
	void OpenGlVertexBuffer::unbind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	int32_t OpenGlVertexBuffer::calculateStride(const VertexElements& elements) const
	{
		int32_t stride = 0;
		for (const auto element : elements)
		{
			stride += elementType2Size(element);
		}
		return stride;
	}

	constexpr int32_t OpenGlVertexBuffer::elementType2Size(const VertexElement element)
	{
		switch (element)
		{
			case VertexElement::Int:
			case VertexElement::Float:
				return 4;
			case VertexElement::Int2:
			case VertexElement::Float2:
				return 4 * 2;
			case VertexElement::Int3:
			case VertexElement::Float3:
				return 4 * 3;
			case VertexElement::Int4:
			case VertexElement::Float4:
				return 4 * 4;
		}
		return 0;
	}
	
	constexpr uint32_t OpenGlVertexBuffer::elementType2GlType(const VertexElement element)
	{
		switch (element)
		{
			case VertexElement::Int:
			case VertexElement::Int2:
			case VertexElement::Int3:
			case VertexElement::Int4:
				return GL_INT;
			case VertexElement::Float:
			case VertexElement::Float2:
			case VertexElement::Float3:
			case VertexElement::Float4:
				return GL_FLOAT;
		}
		return 0;
	}

	constexpr int32_t OpenGlVertexBuffer::getNrOfComponents(const VertexElement element)
	{
		switch (element)
		{
		case VertexElement::Int:
		case VertexElement::Float:
			return 1;
		case VertexElement::Int2:
		case VertexElement::Float2:
			return 2;
		case VertexElement::Int3:
		case VertexElement::Float3:
			return 3;
		case VertexElement::Int4:
		case VertexElement::Float4:
			return 4;
		}
		return 0;
	}

	OpenGlUniform::OpenGlUniform(const UniformType uniformType, const uint32_t size)
		: maxSize{size}, uniformType{uniformType}
	{
		glCreateBuffers(1, &uniformId);
		glNamedBufferData(uniformId, maxSize, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGlUniform::OpenGlUniform(const Texture& sampler, const uint32_t binding)
		: uniformType{UniformType::Sampler}
	{
		static_cast<const OpenGlTexture&>(sampler).bind(binding);
		glCreateBuffers(1, &uniformId);
		glNamedBufferData(uniformId, sizeof(uint32_t), &binding, GL_DYNAMIC_DRAW);
		bind(binding);
	}

	OpenGlUniform::~OpenGlUniform()
	{
		glDeleteBuffers(1, &uniformId);
	}

	void OpenGlUniform::bind(const uint32_t binding) const
	{
		glBindBufferBase(UniformType2GlType(uniformType), binding, uniformId);
	}

	void OpenGlUniform::setData(const void* data, const uint32_t size, const uint32_t offset)
	{
		glNamedBufferSubData(uniformId, offset, size, data);
	}

	constexpr uint32_t OpenGlUniform::UniformType2GlType(const UniformType uniformType)
	{
		switch (uniformType)
		{
			case UniformType::Uniform: return GL_UNIFORM_BUFFER;
			case UniformType::Storage: return GL_SHADER_STORAGE_BUFFER;
			case UniformType::Sampler: return GL_TEXTURE_BUFFER;
		}
		return 0u;
	}



}
