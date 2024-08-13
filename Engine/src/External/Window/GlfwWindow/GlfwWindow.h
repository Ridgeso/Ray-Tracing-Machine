#pragma once
#include <cstdint>
#include <string>

#include "Engine/Window/Window.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

namespace RT
{

	class GlfwWindow : public Window
	{
	public:
		struct Context
		{
			std::string title = "";
			glm::ivec2 size = {};
			bool isMinimized = false;
		};

	public:
		GlfwWindow() = default;
		~GlfwWindow() = default;
		
		void init(const WindowSpecs& specs) final;
		void shutDown() final;

		void setTitleBar(const std::string& title) final;

		void update() final;

		void beginUI() final;
		void endUI() final;

		glm::vec2 getMousePos() const final;
		bool isKeyPressed(int32_t key) const final;
		bool isMousePressed(int32_t key) const final;
		glm::ivec2 getSize() const final;

		void cursorMode(int32_t state) const final;

		void* getNativWindow() override { return window; }

	private:
		void setCallbacks();
		void initImGui();

	private:
		Context context;

		GLFWwindow* window = nullptr;
	};

}
