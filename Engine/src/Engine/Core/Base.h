#pragma once
#include <memory>
#include <functional>

#define EXPEND_MACRO(MACRO) MACRO

namespace RT
{

	template <typename T>
	using Local = std::unique_ptr<T>;

	template <typename T, typename... Args>
	inline constexpr decltype(auto) makeLocal(Args&&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
	
	template <typename T>
	using Share = std::shared_ptr<T>;

	template <typename T, typename... Args>
	inline constexpr decltype(auto) makeShare(Args&&... args)
	{
		return std::make_shared<T> (std::forward<Args>(args)...);
	}

	template <typename T>
	using Ref = std::reference_wrapper<T>;

	template <typename T>
	inline constexpr decltype(auto) makeRef(T& v)
	{
		return std::ref(v);
	}

	template <typename T>
	inline constexpr decltype(auto) makeRef(const T& v)
	{
		return std::cref(v);
	}

}
