#pragma once
#include <functional>
#include <vector>

#include "Engine/Core/Log.h"

namespace RT::Event
{
	
	template <typename EventType>
	using Callbacks = std::vector<std::function<void(const EventType&)>>;

	class Dispatcher
	{
	public:
		template <typename EventType>
		void dispatch(const EventType& event, const Callbacks<EventType>& callbacks)
		{
			RT_LOG_DEBUG("== PROCESSING Event [ {} ] ==", typeid(EventType).name());
			for (const auto& callback : callbacks)
			{
				callback(event);
			}
		}
	};

}
