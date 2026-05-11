#pragma once
#include <memory>
#include <string>

#include "common/utils/FileInfo.h"

#define SPDLOG_COMPILED_LIB
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/std.h>

namespace common
{

	using LoggerId = std::string;

	struct Log
	{
		enum class Level : uint8_t
		{
			Trace = 0,
			Debug,
			Info,
			Warn,
			Error,
			Critical,
			Off
		};

		static void init();
		static void shutdown();

		static void registerLogger(const LoggerId& logger);

		template <Level level, typename... Args>
		static void logBase(
			const LoggerId& logger,
			const common::FileInfo& fileInfo,
			fmt::format_string<Args...> msg,
			Args&&... args)
		{
			const auto logBuf = fmt::vformat(msg, fmt::make_format_args(args...));
			logBaseImpl<level>(logger, fmt::format("{}:{} ::: {}", fileInfo.file, fileInfo.line, logBuf));
		}

		static void setLevel(const Level level, const LoggerId& logger = "");
	
	private:
		template <Level level>
		static void logBaseImpl(
			const LoggerId& logger,
			const std::string& msg);
	};

	#define REGISTER_FMT_FORMAT(TYPE, BASE_TYPE, ...)								\
		template <>																	\
		struct fmt::formatter<TYPE> : formatter<BASE_TYPE>							\
		{																			\
			template <typename FormatContext>										\
			auto format(const TYPE& type, FormatContext& ctx)						\
			{ 																		\
				return formatter<BASE_TYPE>::format(fmt::format(__VA_ARGS__), ctx); \
			}																		\
		};

}

// Logs Macros
#define LOG_CRITICAL(LOGGER_ID, ...) ::common::Log::logBase<::common::Log::Level::Critical>(LOGGER_ID, ::common::FileInfo(), __VA_ARGS__)
#define LOG_ERROR(	 LOGGER_ID, ...) ::common::Log::logBase<::common::Log::Level::Error>(   LOGGER_ID, ::common::FileInfo(), __VA_ARGS__)
#define LOG_WARN(	 LOGGER_ID, ...) ::common::Log::logBase<::common::Log::Level::Warn>(    LOGGER_ID, ::common::FileInfo(), __VA_ARGS__)
#define LOG_INFO(	 LOGGER_ID, ...) ::common::Log::logBase<::common::Log::Level::Info>(    LOGGER_ID, ::common::FileInfo(), __VA_ARGS__)
#define LOG_DEBUG(	 LOGGER_ID, ...) ::common::Log::logBase<::common::Log::Level::Debug>(   LOGGER_ID, ::common::FileInfo(), __VA_ARGS__)
#define LOG_TRACE(	 LOGGER_ID, ...) ::common::Log::logBase<::common::Log::Level::Trace>(   LOGGER_ID, ::common::FileInfo(), __VA_ARGS__)
