#pragma once
#include <memory>

#define EXPEND_MACRO(MACRO) MACRO

namespace RT
{

	template <typename T>
	using Local = std::unique_ptr<T>;

	template <typename T, typename... Args>
	inline constexpr decltype(auto) makeLocal(Args&&... args)
	{
		return std::make_unique<decltype(T{args...})>(std::forward<Args>(args)...);
	}
	
	template <typename T>
	using Share = std::shared_ptr<T>;

	template <typename T, typename... Args>
	inline constexpr decltype(auto) makeShare(Args&&... args)
	{
		return std::make_shared<decltype(T{args...}) > (std::forward<Args>(args)...);
	}

}
