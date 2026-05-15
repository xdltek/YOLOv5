/** @file logger.h
 *
 * @brief
 * @author XDLTek
 * COPYRIGHT(c) 2020-2022 XDLTek.
 * ALL RIGHTS RESERVED
 *
 * This is Unpublished Proprietary Source Code of XDLTek
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include "logging.h"

namespace sample
{
    /**
     * @brief Global logger instance shared by runtime builders, parsers, and demos.
     */
    extern Logger gLogger;
    /**
     * @brief Cached verbose stream tied to the global logger.
     */
    extern LogStreamConsumer gLogVerbose;
    /**
     * @brief Cached info stream tied to the global logger.
     */
    extern LogStreamConsumer gLogInfo;
    /**
     * @brief Cached warning stream tied to the global logger.
     */
    extern LogStreamConsumer gLogWarning;
    /**
     * @brief Cached error stream tied to the global logger.
     */
    extern LogStreamConsumer gLogError;
    /**
     * @brief Cached fatal stream tied to the global logger.
     */
    extern LogStreamConsumer gLogFatal;

    /**
     * @brief Update reportable severity on the global logger and cached stream consumers.
     * @param severity Minimum severity printed by the demo logger.
     */
    void setReportableSeverity(Logger::Severity severity);

    /**
     * @brief Print a user-visible message through a selected runtime logger and optional file.
     */
    void user_visible_log(LogStreamConsumer& rt_logger, const std::string& log_path, const std::string& log_text);
    /**
     * @brief Print a user-visible informational message to console.
     */
    void user_visible_log(const std::string& log_text);
} // namespace sample

#endif // LOGGER_H
