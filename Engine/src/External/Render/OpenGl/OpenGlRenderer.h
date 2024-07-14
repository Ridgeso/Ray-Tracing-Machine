#pragma once
#include <Engine/Core/Base.h>
#include <vector>
#include <cstdint>
#include <random>
#include <glm/glm.hpp>

#include "Engine/Render/Renderer.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/Scene.h"

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

		void init(); // final;
		void shutDown(); // final;

		void render(
			const RenderPass& renderPass,
			const Camera& camera,
			//const Shader& shader,
			const VertexBuffer& vbuffer,
			const Scene& scene); // final;

	private:
		static void loadOpenGlForGlfw();
	};

}
