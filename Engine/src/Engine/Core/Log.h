#pragma once
#include "Engine/Core/Base.h"
#include "Engine/Core/Utils.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace RT::Core
{

	class Log
	{
	public:
		static void init();
		static void shutdown();

		template <spdlog::level::level_enum Level, typename... Args>
		static void logBase(
			const Share<spdlog::logger>& logger,
			const Utils::FileInfo& fileInfo,
			fmt::format_string<Args...> msg,
			Args&&... args)
		{
			auto prefix = fmt::format("{}:{}", fileInfo.file, fileInfo.line);
			const auto logBuf = fmt::vformat(msg, fmt::make_format_args(std::forward<Args>(args)...));
			logger->log(Level, "{} {}", prefix, logBuf);
		}

		static void setLevel(const spdlog::level::level_enum level);

		static const Share<spdlog::logger>& getEngineLogger();
		static const Share<spdlog::logger>& getClientLogger();

	private:
		static Share<spdlog::logger> engineLogger;
		static Share<spdlog::logger> clientLogger;
	};

}

// Engine Logs Macros
#define RT_LOG_CRITICAL(...) ::RT::Core::Log::logBase<spdlog::level::critical>(::RT::Core::Log::getEngineLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define RT_LOG_ERROR(...)    ::RT::Core::Log::logBase<spdlog::level::err>(::RT::Core::Log::getEngineLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define RT_LOG_WARN(...)     ::RT::Core::Log::logBase<spdlog::level::warn>(::RT::Core::Log::getEngineLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define RT_LOG_INFO(...)     ::RT::Core::Log::logBase<spdlog::level::info>(::RT::Core::Log::getEngineLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define RT_LOG_DEBUG(...)    ::RT::Core::Log::logBase<spdlog::level::debug>(::RT::Core::Log::getEngineLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define RT_LOG_TRACE(...)    ::RT::Core::Log::logBase<spdlog::level::trace>(::RT::Core::Log::getEngineLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)

// Clients Logs Macros
#define LOG_CRITICAL(...)    ::RT::Core::Log::logBase<spdlog::level::critical>(::RT::Core::Log::getClientLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define LOG_ERROR(...)       ::RT::Core::Log::logBase<spdlog::level::err>(::RT::Core::Log::getClientLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define LOG_WARN(...)        ::RT::Core::Log::logBase<spdlog::level::warn>(::RT::Core::Log::getClientLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define LOG_INFO(...)        ::RT::Core::Log::logBase<spdlog::level::info>(::RT::Core::Log::getClientLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define LOG_DEBUG(...)       ::RT::Core::Log::logBase<spdlog::level::debug>(::RT::Core::Log::getClientLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
#define LOG_TRACE(...)       ::RT::Core::Log::logBase<spdlog::level::trace>(::RT::Core::Log::getClientLogger(), ::RT::Utils::FileInfo(), __VA_ARGS__)
