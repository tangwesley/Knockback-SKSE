#include <Knockback/Log.h>

#include "SKSE/SKSE.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <format>
#include <memory>

namespace Knockback
{
    void SetupLog()
    {
        auto logsFolder = SKSE::log::log_directory();
        if (!logsFolder) {
            SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
        }

        auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
        auto logFilePath = *logsFolder / std::format("{}.log", pluginName);

        auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
        auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));

        spdlog::set_default_logger(std::move(loggerPtr));

        // Start quiet; LoadConfig() re-applies this once the config is known.
        SetVerboseLogging(false);
    }

    void SetVerboseLogging(bool a_enabled)
    {
        const auto level = a_enabled ? spdlog::level::trace : spdlog::level::info;
        spdlog::set_level(level);
        spdlog::flush_on(level);
    }
}
