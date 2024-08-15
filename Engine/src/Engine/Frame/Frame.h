#pragma once

namespace RT
{
	
	struct Frame
	{
		Frame() {}
		virtual ~Frame() {}

		virtual void onInit() {}
		virtual void onShutdown() {}

		virtual void layout() {}
		virtual void update(const float ts) {}
	};

}
