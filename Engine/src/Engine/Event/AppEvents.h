#pragma once
#include "Engine/Utils/KeyCodes.h"

namespace RT::Event
{

	struct AppClose {};

	struct WindowResize
	{
		int32_t width = 0;
		int32_t height = 0;
		bool isMinimized = true;
	};

	struct KeyPressed
	{
		Keys::Keyboard key = Keys::Keyboard::None;
		Keys::Action action = Keys::Action::Halt;
		Keys::KeyCode mod = Keys::Mod::None;
	};

	struct MousePressed
	{
		Keys::Keyboard button = Keys::Keyboard::None;
		Keys::Action action = Keys::Action::Halt;
		Keys::KeyCode mod = Keys::Mod::None;
	};

	struct MouseMove
	{
		int32_t xPos = 0;
		int32_t yPos = 0;
	};

	struct ScrollMoved
	{
		int32_t xOffset = 0;
		int32_t yOffset = 0;
	};

}
