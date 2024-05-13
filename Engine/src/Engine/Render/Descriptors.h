#pragma once
#include <vector>
#include <tuple>
#include "Engine/Core/Base.h"

#include "Engine/Render/Buffer.h"

namespace RT
{

	enum class DescriptorType
	{
		Uniform,
		Storage,
		Sampler
	};

	using DescriptorLayoutSpec = std::vector<
		std::tuple<
			uint32_t,
			DescriptorType>>;

	struct DescriptorLayout
	{
		virtual ~DescriptorLayout() = 0;

		static Local<DescriptorLayout> create(const DescriptorLayoutSpec& spec);
	};

	using DescriptorPoolSpec = std::vector<
		std::tuple<
			uint32_t,
			DescriptorType>>;

	struct DescriptorPool
	{
		virtual ~DescriptorPool() = 0;

		static Local<DescriptorPool> create(const DescriptorPoolSpec& spec);
	};

	struct DescriptorSet
	{
		virtual ~DescriptorSet() = 0;

		virtual void write(const uint32_t binding, const Uniform& uniform) const = 0;
		virtual void bind(const uint32_t binding) const = 0;

		static Local<DescriptorSet> create(const DescriptorLayout& layout, const DescriptorPool& pool);
	};

}
