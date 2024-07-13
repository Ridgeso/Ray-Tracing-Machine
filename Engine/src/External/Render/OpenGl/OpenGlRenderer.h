#pragma once
#include <Engine/Core/Base.h>
#include <vector>
#include <cstdint>
#include <random>
#include <glm/glm.hpp>

#include "Engine/Render/Renderer.h"
#include "OpenGlRenderPass.h"
#include "OpenGlShader.h"
#include "OpenGlTexture.h"
#include "OpenGlBuffer.h"

namespace RT::OpenGl
{

	class OpenGlRenderer // : public Renderer
	{
	public:
		OpenGlRenderer() = default;
		~OpenGlRenderer() = default;

		void init(const RenderSpecs& specs); // final;
		void shutDown(); // final;

		void render(
			const RenderPass& renderPass,
			const Camera& camera,
			//const Shader& shader,
			const VertexBuffer& vbuffer,
			const Scene& scene); // final;

		const RenderSpecs& getSpecs() const { return specs; }

	private:
		static void loadOpenGlForGlfw();

	private:
		RenderSpecs specs;
	};

}
