/** @file logger.cpp
 *
 * @brief
 * @author XDLTek
 * COPYRIGHT(c) 2020-2022 XDLTek.
 * ALL RIGHTS RESERVED
 *
 * This is Unpublished Proprietary Source Code of XDLTek
 */

#include "logger.h"
#include "ErrorRecorder.h"
#include "logging.h"

SampleErrorRecorder gRecorder;
namespace sample
{
    Logger gLogger{Logger::Severity::kERROR};
    LogStreamConsumer gLogVerbose{LOG_VERBOSE(gLogger)};
    LogStreamConsumer gLogInfo{LOG_INFO(gLogger)};
    LogStreamConsumer gLogWarning{LOG_WARN(gLogger)};
    LogStreamConsumer gLogError{LOG_ERROR(gLogger)};
    LogStreamConsumer gLogFatal{LOG_FATAL(gLogger)};

    /**
     * @brief Update global logger verbosity and cached stream consumers together.
     */
    void setReportableSeverity(Logger::Severity severity)
    {
        // Keep the global logger and already-created stream consumers in sync.
        gLogger.setReportableSeverity(severity);
        gLogVerbose.setReportableSeverity(severity);
        gLogInfo.setReportableSeverity(severity);
        gLogWarning.setReportableSeverity(severity);
        gLogError.setReportableSeverity(severity);
        gLogFatal.setReportableSeverity(severity);
    }

    /**
     * @brief Print a customer-visible message to the selected logger, stdout, and optional file.
     */
    void user_visible_log(LogStreamConsumer& rt_logger, const std::string& log_path, const std::string& log_text)
    {
        // Mirror important customer-facing messages to the runtime log and stdout.
        rt_logger << log_text << std::endl;
        std::cout << log_text << std::endl;

        if (log_path.empty())
        {
            return;
        }

        // When requested, also append the message to a caller-provided log file.
        std::ofstream file_stream;
        file_stream.open(log_path, std::ios_base::app | std::ios_base::in);
        if (!file_stream.is_open()) {
            throw std::runtime_error("cannot open log file, path: " + log_path);
        }

        file_stream << log_text << std::endl;
        file_stream.close();
    }

    /**
     * @brief Print a customer-visible informational message.
     */
    void user_visible_log(const std::string& log_text)
    {
        return user_visible_log(gLogInfo, "", log_text);
    }
} // namespace sample
