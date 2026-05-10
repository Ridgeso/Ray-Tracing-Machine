#pragma once
#include <chrono>

namespace common
{

	namespace
	{
		using namespace std::chrono;
	}

	class Timer
	{
	public:

		Timer()
			: timePoint{high_resolution_clock::now()}
		{
		}

		float Ellapsed() const
		{
			return duration_cast<nanoseconds>(high_resolution_clock::now() - timePoint).count() * oneMicroseconds;
		}

	private:
		time_point<high_resolution_clock> timePoint;

		static constexpr float oneMicroseconds = 1e-6f;
	};

}
