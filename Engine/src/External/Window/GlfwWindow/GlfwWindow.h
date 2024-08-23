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
		bool isKeyPressed(const Keys::Keyboard key) const final;
		bool isMousePressed(const Keys::Mouse button) const final;
		glm::ivec2 getSize() const final;
		bool isMinimize() const final { return context.isMinimized; }

		void cursorMode(const Keys::MouseMod mod) const final;

		void* getNativWindow() override { return window; }

	private:
		void setCallbacks();
		void initImGui();

	private:
		Context context;

		GLFWwindow* window = nullptr;
	};

}
