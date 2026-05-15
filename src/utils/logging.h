/** @file logging.h
 *
 * @brief
 * @author XDLTek Technologies
 * COPYRIGHT(c) 2020-2022 XDLTek Technologies.
 * ALL RIGHTS RESERVED
 *
 * This is Unpublished Proprietary Source Code of XDLTek Technologies
 */
#ifndef RPPRT_LOGGING_H
#define RPPRT_LOGGING_H

#include <ctime>
#include <iostream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <fstream>
#include <string>

#include "InferRuntimeCommon.h"

namespace sample
{
    using Severity = infer1::ILogger::Severity;

    /**
     * @brief Return the optional file path used by stream-style logging.
     */
    const std::string& get_log_file_path();
    /**
     * @brief Set the optional file path used by stream-style logging.
     * @param path Destination log file path.
     */
    void set_log_file_path(const std::string& path);

    /**
     * @brief Forward declaration for the global RppRT-compatible logger.
     */
    class Logger;
    extern Logger gLogger;

    /**
     * @brief Stream buffer that formats one log line and mirrors it to an optional log file.
     */
    class LogStreamConsumerBuffer : public std::stringbuf
    {
    public:
        /**
         * @brief Construct a formatting buffer for one severity-prefixed stream.
         */
        LogStreamConsumerBuffer(std::ostream& stream, const std::string& prefix, bool shouldLog)
                : mOutput(stream)
                , mPrefix(prefix)
                , mShouldLog(shouldLog)
        {
        }

        /**
         * @brief Move a temporary stream buffer while keeping the output stream reference valid.
         */
        LogStreamConsumerBuffer(LogStreamConsumerBuffer&& other)
                : mOutput(other.mOutput)
        {
        }

        /**
         * @brief Flush any pending text before the buffer is destroyed.
         */
        ~LogStreamConsumerBuffer()
        {
            // Flush any buffered text when a temporary log stream goes out of scope.
            if (pbase() != pptr())
            {
                putOutput();
            }
        }

        /**
         * @brief Flush buffered log text into the target stream.
         */
        virtual int sync()
        {
            putOutput();
            return 0;
        }

        /**
         * @brief Add timestamp/severity metadata and write buffered text to console and optional file.
         */
        void putOutput()
        {
            if (mShouldLog)
            {
                std::string log_text = str();

                // Prepend a timestamp so runtime messages can be correlated with demo stages.
                std::time_t timestamp = std::time(nullptr);
                tm* tm_local = std::localtime(&timestamp);
                mOutput << "[";
                mOutput << std::setw(2) << std::setfill('0') << 1 + tm_local->tm_mon << "/";
                mOutput << std::setw(2) << std::setfill('0') << tm_local->tm_mday << "/";
                mOutput << std::setw(4) << std::setfill('0') << 1900 + tm_local->tm_year << "-";
                mOutput << std::setw(2) << std::setfill('0') << tm_local->tm_hour << ":";
                mOutput << std::setw(2) << std::setfill('0') << tm_local->tm_min << ":";
                mOutput << std::setw(2) << std::setfill('0') << tm_local->tm_sec << "] ";
                // Write the severity-prefixed message to console and optional file.
                mOutput << mPrefix << log_text;

                const std::string& log_file = get_log_file_path();
                if (!log_file.empty())
                {
                    std::ofstream file_stream;
                    file_stream.open(log_file, std::ios_base::app | std::ios_base::in);
                    if (!file_stream.is_open()) {
                        throw std::runtime_error("cannot open log file");
                    }

                    file_stream << mPrefix << log_text;

                    file_stream.close();
                }

                // Reset the buffer so subsequent stream writes start fresh.
                str("");
                mOutput.flush();
            }
        }

        /**
         * @brief Enable or disable output for this cached stream buffer.
         */
        void setShouldLog(bool shouldLog)
        {
            mShouldLog = shouldLog;
        }

    private:
        std::ostream& mOutput;
        std::string mPrefix;
        bool mShouldLog;
    };

    /**
     * @brief Base object that initializes LogStreamConsumerBuffer before std::ostream.
     */
    class LogStreamConsumerBase
    {
    public:
        /**
         * @brief Construct the stream buffer before std::ostream receives its address.
         */
        LogStreamConsumerBase(std::ostream& stream, const std::string& prefix, bool shouldLog)
                : mBuffer(stream, prefix, shouldLog)
        {
        }

    protected:
        LogStreamConsumerBuffer mBuffer;
    };

    /**
     * @brief Stream facade that lets callers write log messages with C++ stream syntax.
     *
     * LogStreamConsumerBase must appear before std::ostream in the base-class list so
     * the stream buffer is constructed before it is passed to std::ostream.
     */
    class LogStreamConsumer : protected LogStreamConsumerBase, public std::ostream
    {
    public:
        /**
         * @brief Create a stream that emits messages when severity is reportable.
         * @param reportableSeverity Maximum verbosity allowed by the logger.
         * @param severity Severity of this individual log stream.
         */
        LogStreamConsumer(Severity reportableSeverity, Severity severity)
                : LogStreamConsumerBase(severityOstream(severity), severityPrefix(severity), severity <= reportableSeverity)
                , std::ostream(&mBuffer) // links the stream buffer with the stream
                , mShouldLog(severity <= reportableSeverity)
                , mSeverity(severity)
        {
        }

        /**
         * @brief Move a temporary stream while preserving its severity and output target.
         */
        LogStreamConsumer(LogStreamConsumer&& other)
                : LogStreamConsumerBase(severityOstream(other.mSeverity), severityPrefix(other.mSeverity), other.mShouldLog)
                , std::ostream(&mBuffer) // links the stream buffer with the stream
                , mShouldLog(other.mShouldLog)
                , mSeverity(other.mSeverity)
        {
        }

        /**
         * @brief Update whether this cached stream should emit after logger verbosity changes.
         */
        void setReportableSeverity(Severity reportableSeverity)
        {
            // Cached streams must recompute the filter because their severity is fixed at construction.
            mShouldLog = mSeverity <= reportableSeverity;
            mBuffer.setShouldLog(mShouldLog);
        }

    private:
        /**
         * @brief Choose stdout or stderr for a severity.
         */
        static std::ostream& severityOstream(Severity severity)
        {
            return severity >= Severity::kINFO ? std::cout : std::cerr;
        }

        /**
         * @brief Convert a severity into the short prefix used by log lines.
         */
        static std::string severityPrefix(Severity severity)
        {
            switch (severity)
            {
                case Severity::kINTERNAL_ERROR: return "[F] ";
                case Severity::kERROR: return "[E] ";
                case Severity::kWARNING: return "[W] ";
                case Severity::kINFO: return "[I] ";
                case Severity::kVERBOSE: return "[V] ";
                default: return "";
            }
        }

        bool mShouldLog;
        Severity mSeverity;
    };

    /**
     * @brief Shared logger used by RppRT builders, parsers, and demo helper code.
     *
     * The class keeps direct infer1::ILogger inheritance so OpenRT/RppRT APIs can
     * use the same logger instance as the demo command-line tools.
     */
    class Logger : public infer1::ILogger
    {
    public:
        /**
         * @brief Construct a logger with the requested reportable severity.
         */
        Logger(Severity severity = Severity::kVERBOSE)
                : mReportableSeverity(severity)
        {
        }

        /**
         * @brief Lifecycle state used by legacy sample-style test reporting helpers.
         */
        enum class TestResult
        {
            kRUNNING, // The test is running.
            kPASSED,  // The test passed.
            kFAILED,  // The test failed.
            kWAIVED   // The test was waived.
        };

        /**
         * @brief Return the infer1::ILogger interface required by RppRT APIs.
         */
        infer1::ILogger& getLogger()
        {
            return *this;
        }

        /**
         * @brief Receive runtime log messages from RppRT and forward them through stream logging.
         */
        void log(Severity severity, const char* msg) override
        {
            // Preserve runtime-origin context so parser/build errors are distinguishable from demo messages.
            LogStreamConsumer(mReportableSeverity, severity) << "[OpenRT] " << std::string(msg) << std::endl;
        }

        /**
         * @brief Update the maximum verbosity emitted by this logger.
         * @param severity The logger emits messages with this severity or higher priority.
         */
        void setReportableSeverity(Severity severity)
        {
            mReportableSeverity = severity;
        }

        /**
         * @brief Opaque handle that stores legacy test-report metadata.
         */
        class TestAtom
        {
        public:
            /**
             * @brief Move a test handle without copying stored strings.
             */
            TestAtom(TestAtom&&) = default;

        private:
            friend class Logger;

            /**
             * @brief Construct a test atom with its initial state and reproduction command.
             */
            TestAtom(bool started, const std::string& name, const std::string& cmdline)
                    : mStarted(started)
                    , mName(name)
                    , mCmdline(cmdline)
            {
            }

            bool mStarted;
            std::string mName;
            std::string mCmdline;
        };

        /**
         * @brief Create a test-report handle from a name and command-line string.
         * @param name Test name printed in the report line.
         * @param cmdline Command line used to reproduce the test.
         */
        static TestAtom defineTest(const std::string& name, const std::string& cmdline)
        {
            return TestAtom(false, name, cmdline);
        }

        /**
         * @brief Create a test-report handle from an argv-style command line.
         */
        static TestAtom defineTest(const std::string& name, int argc, char const* const* argv)
        {
            // Reconstruct argv into a stable string before storing it in the handle.
            auto cmdline = genCmdlineString(argc, argv);
            return defineTest(name, cmdline);
        }

        /**
         * @brief Print a legacy test-start report line and mark the handle as started.
         */
        static void reportTestStart(TestAtom& testAtom)
        {
            reportTestResult(testAtom, TestResult::kRUNNING);
            testAtom.mStarted = true;
        }

        /**
         * @brief Print a legacy test-end report line.
         */
        static void reportTestEnd(const TestAtom& testAtom, TestResult result)
        {
            reportTestResult(testAtom, result);
        }

        /**
         * @brief Report a passed test and return process success.
         */
        static int reportPass(const TestAtom& testAtom)
        {
            reportTestEnd(testAtom, TestResult::kPASSED);
            return EXIT_SUCCESS;
        }

        /**
         * @brief Report a failed test and return process failure.
         */
        static int reportFail(const TestAtom& testAtom)
        {
            reportTestEnd(testAtom, TestResult::kFAILED);
            return EXIT_FAILURE;
        }

        /**
         * @brief Report a waived test and return process success.
         */
        static int reportWaive(const TestAtom& testAtom)
        {
            reportTestEnd(testAtom, TestResult::kWAIVED);
            return EXIT_SUCCESS;
        }

        /**
         * @brief Report pass/fail from a boolean result.
         */
        static int reportTest(const TestAtom& testAtom, bool pass)
        {
            return pass ? reportPass(testAtom) : reportFail(testAtom);
        }

        /**
         * @brief Return the current maximum verbosity emitted by this logger.
         */
        Severity getReportableSeverity() const
        {
            return mReportableSeverity;
        }

    private:
        /**
         * @brief Return the prefix used for a severity in legacy formatting.
         */
        static const char* severityPrefix(Severity severity)
        {
            switch (severity)
            {
                case Severity::kINTERNAL_ERROR: return "[F] ";
                case Severity::kERROR: return "[E] ";
                case Severity::kWARNING: return "[W] ";
                case Severity::kINFO: return "[I] ";
                case Severity::kVERBOSE: return "[V] ";
                default: return "";
            }
        }

        /**
         * @brief Return the printed text for a legacy test result.
         */
        static const char* testResultString(TestResult result)
        {
            switch (result)
            {
                case TestResult::kRUNNING: return "RUNNING";
                case TestResult::kPASSED: return "PASSED";
                case TestResult::kFAILED: return "FAILED";
                case TestResult::kWAIVED: return "WAIVED";
                default: return "";
            }
        }

        /**
         * @brief Return the stream used by legacy test-report helpers.
         */
        static std::ostream& severityOstream(Severity severity)
        {
            // Current demos send legacy test-report lines to stdout regardless of severity.
            (void)severity;
            return std::cout;
        }

        /**
         * @brief Print one legacy test-report line.
         */
        static void reportTestResult(const TestAtom& testAtom, TestResult result)
        {
            severityOstream(Severity::kINFO) << "&&&& " << testResultString(result)
                                             << " " << testAtom.mName << " # " << testAtom.mCmdline
                                             << std::endl;
        }

        /**
         * @brief Generate a command-line string from argv values.
         */
        static std::string genCmdlineString(int argc, char const* const* argv)
        {
            std::stringstream ss;
            for (int i = 0; i < argc; i++)
            {
                if (i > 0)
                    ss << " ";
                ss << argv[i];
            }
            return ss.str();
        }

        Severity mReportableSeverity;
    };

    namespace
    {

        /**
         * @brief Create a verbose log stream using an explicit logger.
         */
        inline LogStreamConsumer LOG_VERBOSE(const Logger& logger)
        {
            return LogStreamConsumer(logger.getReportableSeverity(), Severity::kVERBOSE);
        }

        /**
         * @brief Create a verbose log stream using the global demo logger.
         */
        inline LogStreamConsumer LOG_VERBOSE() {
            return LOG_VERBOSE(gLogger);
        }

        /**
         * @brief Create an info log stream using an explicit logger.
         */
        inline LogStreamConsumer LOG_INFO(const Logger& logger)
        {
            return LogStreamConsumer(logger.getReportableSeverity(), Severity::kINFO);
        }

        /**
         * @brief Create an info log stream using the global demo logger.
         */
        inline LogStreamConsumer LOG_INFO() {
            return LOG_INFO(gLogger);
        }

        /**
         * @brief Create a warning log stream using an explicit logger.
         */
        inline LogStreamConsumer LOG_WARN(const Logger& logger)
        {
            return LogStreamConsumer(logger.getReportableSeverity(), Severity::kWARNING);
        }

        /**
         * @brief Create a warning log stream using the global demo logger.
         */
        inline LogStreamConsumer LOG_WARN() {
            return LOG_WARN(gLogger);
        }

        /**
         * @brief Create an error log stream using an explicit logger.
         */
        inline LogStreamConsumer LOG_ERROR(const Logger& logger)
        {
            return LogStreamConsumer(logger.getReportableSeverity(), Severity::kERROR);
        }

        /**
         * @brief Create an error log stream using the global demo logger.
         */
        inline LogStreamConsumer LOG_ERROR() {
            return LOG_ERROR(gLogger);
        }

        /**
         * @brief Create a fatal log stream using an explicit logger.
         */
        inline LogStreamConsumer LOG_FATAL(const Logger& logger)
        {
            return LogStreamConsumer(logger.getReportableSeverity(), Severity::kINTERNAL_ERROR);
        }

        /**
         * @brief Create a fatal log stream using the global demo logger.
         */
        inline LogStreamConsumer LOG_FATAL() {
            return LOG_FATAL(gLogger);
        }

    } // anonymous namespace
}
#endif // RPPRT_LOGGING_H
