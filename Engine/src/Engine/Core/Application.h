#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <imgui.h>
#include <Engine/Core/Base.h>

#include "Engine/Window/Window.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/Buffer.h"
#include "Engine/Render/RenderPass.h"
#include "Engine/Render/Descriptors.h"
#include "Engine/Render/Pipeline.h"

namespace RT
{

	struct ApplicationCommandLineArgs
	{
		int32_t argc;
		char** argv;
	};

	struct ApplicationSpecs
	{
		std::string name;
		bool isRunning;
		ApplicationCommandLineArgs args;
	};

	class Application
	{
	public:
		Application(ApplicationSpecs specs);
		virtual ~Application();

		void run();
		void layout();
		void update();
		void updateView(float ts);

		static Application& Get() { return *MainApp; }
		Local<Window>& getWindow() { return mainWindow; }

	private:
		ApplicationSpecs specs;
		float lastFrameDuration;
		float appFrameDuration;
		ImVec2 viewportSize;

		Local<Window> mainWindow;
		Local<Renderer> renderer;
		
		Local<Shader> rtShader;

		// TODO: return renderPass and graphics pipeline for post processing
		//Local<VertexBuffer> screenBuff;
		//Share<RenderPass> renderPass;
		
		Local<Texture> accumulationTexture;
		Local<Texture> outTexture;

		Local<Uniform> cameraUniform;
		Local<Uniform> ammountsUniform;
		Local<Uniform> accumulationSamplerUniform;
		Local<Uniform> outSamplerUniform;
		Local<Uniform> materialsStorage;
		Local<Uniform> spheresStorage;
		
		Local<DescriptorLayout> commonDescriptorLayout;
		Local<DescriptorPool> commonDescriptorPool;
		Local<DescriptorSet> commonDescriptorSet;
		Local<DescriptorLayout> sceneDescriptorLayout;
		Local<DescriptorPool> sceneDescriptorPool;
		Local<DescriptorSet> sceneDescriptorSet;

		Local<Pipeline> pipeline;

		Camera camera;
		Scene scene;

		glm::ivec2 lastWinSize;
		glm::vec2 lastMousePos;
		static Application* MainApp;

		bool accumulation = false;
		bool drawEnvironmentTranslator = false;

		struct InfoUniform
		{
			float drawEnvironment = (float)false;
			uint32_t maxBounces = 5;
			uint32_t maxFrames = 1;
			uint32_t frameIndex = 1;
			glm::vec2 resolution = {};
			int32_t materialsCount = 0;
			int32_t spheresCount = 0;
			uint32_t renderTexture = 1;
		} infoUniform;

		//struct Vertices
		//{
		//	float Coords[2];
		//	float TexCoords[2];
		//} static constexpr screenVertices[] = {
		//	{ { -1.0f, -1.0f }, { 0.0f, 0.0f } },
		//	{ {  1.0f, -1.0f }, { 1.0f, 0.0f } },
		//	{ {  1.0f,  1.0f }, { 1.0f, 1.0f } },
		//	{ {  1.0f,  1.0f }, { 1.0f, 1.0f } },
		//	{ { -1.0f,  1.0f }, { 0.0f, 1.0f } },
		//	{ { -1.0f, -1.0f }, { 0.0f, 0.0f } }
		//};
		//static constexpr int32_t screenVerticesCount = sizeof(screenVertices) / sizeof(float);
	};

	Application* CreateApplication(ApplicationCommandLineArgs args);

}
