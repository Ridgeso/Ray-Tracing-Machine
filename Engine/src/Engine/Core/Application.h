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
		Local<VertexBuffer> screenBuff;
		Share<RenderPass> renderPass;
		Camera camera;
		Scene scene;

		glm::ivec2 lastWinSize;
		glm::vec2 lastMousePos;
		static Application* MainApp;

		bool accumulation = false;
		uint32_t framesCount = 1;
		uint32_t maxBounces = 5;
		uint32_t maxFrames = 1;
		bool drawEnvironment = false;

		struct Vertices
		{
			float Coords[2];
			float TexCoords[2];
		} static constexpr screenVertices[] = {
			{ { -1.0f, -1.0f }, { 0.0f, 0.0f } },
			{ {  1.0f, -1.0f }, { 1.0f, 0.0f } },
			{ {  1.0f,  1.0f }, { 1.0f, 1.0f } },
			{ {  1.0f,  1.0f }, { 1.0f, 1.0f } },
			{ { -1.0f,  1.0f }, { 0.0f, 1.0f } },
			{ { -1.0f, -1.0f }, { 0.0f, 0.0f } }
		};
		static constexpr int32_t screenVerticesCount = sizeof(screenVertices) / sizeof(float);
	};

	Application* CreateApplication(ApplicationCommandLineArgs args);

}
