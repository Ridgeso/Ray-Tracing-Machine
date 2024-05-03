#pragma once
#include <cstdint>
#include <tuple>

#include "Engine/Render/Buffer.h"

namespace RT::OpenGl
{

	class OpenGlVertexBuffer : public VertexBuffer
	{
	public:
		OpenGlVertexBuffer(const uint32_t size);
		OpenGlVertexBuffer(const uint32_t size, const void* data);
		~OpenGlVertexBuffer() final;

		void registerAttributes(const VertexElements& elements) const final;
		void setData(const uint32_t size, const void* data) const final;
		const int32_t getCount() const final { return count; }

		void bind() const final;
		void unbind() const final;

	private:
		int32_t calculateStride(const VertexElements& elements) const;

		constexpr static int32_t elementType2Size(const VertexElement element);
		constexpr static uint32_t elementType2GlType(const VertexElement element);
		constexpr static int32_t getNrOfComponents(const VertexElement element);
	private:
		uint32_t bufferId;
		uint32_t size;
		mutable int32_t count;
	};

	class OpenGlUniform : public Uniform
	{
	public:
		OpenGlUniform(const UniformType uniformType, const uint32_t size);
		OpenGlUniform(const Texture& sampler, const uint32_t binding);
		~OpenGlUniform() final;

		void bind(const uint32_t binding) const final;
		void setData(const void* data, const uint32_t size, const uint32_t offset = 0) final;

	private:
		constexpr static uint32_t UniformType2GlType(const UniformType uniformType);

	private:
		uint32_t uniformId = 0u;
		uint32_t maxSize = 0u;
		UniformType uniformType = UniformType::None;
	};

}
