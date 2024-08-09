#pragma once

namespace RT
{
	
	struct Frame
	{
		virtual ~Frame() = 0 {}

		virtual void onInit() {}
		virtual void onShutdown() {}

		virtual void layout() = 0;
		virtual void update() = 0;
	};

}
