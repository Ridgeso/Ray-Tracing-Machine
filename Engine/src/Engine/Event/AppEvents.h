#pragma once

namespace RT::Event
{

	struct AppClose {};

	struct WindowResize
	{
		int32_t width = 0;
		int32_t height = 0;
	};

}
