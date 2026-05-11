#include "Log.h"
#include <array>
#include <memory>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/std.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace common
{
namespace
{

    constexpr spdlog::level::level_enum toSpdLogLevel(const Log::Level level)
    {
        switch (level)
        {
            case Log::Level::Trace:    return spdlog::level::trace;
            case Log::Level::Debug:    return spdlog::level::debug;
            case Log::Level::Info:     return spdlog::level::info;
            case Log::Level::Warn:     return spdlog::level::warn;
            case Log::Level::Error:    return spdlog::level::err;
            case Log::Level::Critical: return spdlog::level::critical;
            case Log::Level::Off:      return spdlog::level::off;
            default:                   return spdlog::level::info;
        }
    }

    struct LogImpl
    {
		void init()
        {
            registerLogger("CORE");
        }

		void shutdown()
        {
            spdlog::shutdown();
        }

        void registerLogger(const LoggerId& logger)
        {
            if (loggers.find(logger) != loggers.end())
            {
                loggers["CORE"]->warn("Logger with id {} already exists!", logger);
                return;
            }

            auto logSinks = std::array<spdlog::sink_ptr, 2>{
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
                std::make_shared<spdlog::sinks::basic_file_sink_mt>("backlog.log", true)
            };

            logSinks[0]->set_pattern("%^[%T:%e][%L] %n: %v%$");
            logSinks[1]->set_pattern("[%d-%m-%C %T:%e][%l] %n: %v");

            loggers[logger] = std::make_shared<spdlog::logger>(
                logger,
                logSinks.begin(),
                logSinks.end()
            );
            spdlog::register_logger(loggers[logger]);

            setLevel(spdlog::level::trace, logger);
        }

        void setLevel(const spdlog::level::level_enum level, const LoggerId& logger)
        {
            if (logger.empty())
            {
                for (const auto& [_, logger] : loggers)
                {
                    logger->set_level(level);
                    logger->flush_on(level);
                }
            }
            else
            {
                loggers[logger]->set_level(level);
                loggers[logger]->flush_on(level);
            }
        }

        std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers{};
    };

    static LogImpl impl{};
} // namespace

	void Log::init()
	{
        impl.init();
	}

	void Log::shutdown()
	{
        impl.shutdown();
	}

    void Log::registerLogger(const LoggerId& id)
    {
        impl.registerLogger(id);
    }

    void Log::setLevel(const Log::Level level, const std::string& logger)
    {
        impl.setLevel(toSpdLogLevel(level), logger);
    }

    template <Log::Level level>
    void Log::logBaseImpl(
        const LoggerId& logger,
        const std::string& msg)
    {
        impl.loggers[logger]->log(toSpdLogLevel(level), msg);
    }

    template void Log::logBaseImpl<Log::Level::Critical>(
        const LoggerId& logger,
        const std::string& msg);
    template void Log::logBaseImpl<Log::Level::Error>(
        const LoggerId& logger,
        const std::string& msg);
    template void Log::logBaseImpl<Log::Level::Warn>(
        const LoggerId& logger,
        const std::string& msg);
    template void Log::logBaseImpl<Log::Level::Info>(
        const LoggerId& logger,
        const std::string& msg);
    template void Log::logBaseImpl<Log::Level::Debug>(
        const LoggerId& logger,
        const std::string& msg);
    template void Log::logBaseImpl<Log::Level::Trace>(
        const LoggerId& logger,
        const std::string& msg);

} // namespace common
