#pragma once
#include <cstdint>

#ifdef RT_ROOT_PATH
	#define RT_FILE_PREFIX_END (sizeof(RT_ROOT_PATH) - 1)
#else
	#define RT_FILE_PREFIX_END 0
#endif // RT_ROOT_PATH

#define RT_FILE &__builtin_FILE()[RT_FILE_PREFIX_END]
#define RT_LINE __builtin_LINE()
#define RT_FUNCTION __builtin_FUNCTION()

namespace RT::Utils
{

	struct FileInfo
	{
		constexpr explicit FileInfo(
			const char* file = RT_FILE,
			const char* function = RT_FUNCTION,
			const uint32_t line = RT_LINE)
			: file{file}, function{function}, line{line}
		{}

		const char* file;
		const char* function;
		const uint32_t line;
	};

	template <typename Derive>
	struct Trait
	{
		const Derive& getImpl() const
		{
			return static_cast<const Derive&>(*this);
		}

		Derive& getImpl()
		{
			return static_cast<Derive&>(*this);
		}
	};

}
