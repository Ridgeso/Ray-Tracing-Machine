#pragma once
#include "Dispatcher.h"

namespace RT::Event
{

	template <typename EventType>
	class Event
	{
	public:
		void process() const
		{
			auto dispatcher = Dispatcher{};
			dispatcher.dispatch(event, callbacks);
		}

		template <typename Filler>
		void fill(Filler&& filler)
		{
			filler(event);
		}

		template <typename Callback>
		static void registerCallback(Callback&& callback)
		{
			callbacks.emplace_back(std::forward<Callback&&>(callback));
		}

	private:
		EventType event = {};

		inline static Callbacks<EventType> callbacks = {};
	};

}
