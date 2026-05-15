/**
 * @file ErrorRecorder.h
 * @brief Simple runtime error recorder used by sample logging.
 */

#ifndef ERROR_RECORDER_H
#define ERROR_RECORDER_H
#include <vector>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <exception>
#include "rpp_runtime.h"

#include "Infer.h"
#include "logger.h"

/**
 * @brief Collect runtime errors in a thread-safe vector-backed recorder.
 */
class SampleErrorRecorder : public infer1::IErrorRecorder
{
    using errorPair = std::pair<infer1::ErrorCode, std::string>;
    using errorStack = std::vector<errorPair>;

    public:
        /**
         * @brief Construct an empty recorder with zero runtime references.
         */
        SampleErrorRecorder() = default;

        /**
         * @brief Destroy the recorder after runtime ownership has been released.
         */
        virtual ~SampleErrorRecorder() noexcept {}
        /**
         * @brief Return the number of recorded runtime errors.
         */
        int32_t getNbErrors() const noexcept final
        {
            return mErrorStack.size();
        }
        /**
         * @brief Return the runtime error code for a recorded error index.
         */
        infer1::ErrorCode getErrorCode(int32_t errorIdx) const noexcept final
        {
            return indexCheck(errorIdx) ? infer1::ErrorCode::kINVALID_ARGUMENT : (*this)[errorIdx].first;
        };
        /**
         * @brief Return the runtime error description for a recorded error index.
         */
        IErrorRecorder::ErrorDesc getErrorDesc(int32_t errorIdx) const noexcept final
        {
            return indexCheck(errorIdx) ? "errorIdx out of range." : (*this)[errorIdx].second.c_str();
        }
        /**
         * @brief Return whether the fixed recorder capacity has been exceeded.
         */
        bool hasOverflowed() const noexcept final
        {
            // This recorder can grow dynamically through std::vector.
            return false;
        }

        /**
         * @brief Clear all recorded errors while holding the stack mutex.
         */
        void clear() noexcept final
        {
            try 
            {
                // Hold the lock so no other thread appends while the stack is cleared.
                std::lock_guard<std::mutex> guard(mStackLock);
                mErrorStack.clear();
            }
            catch (const std::exception& e)
            {
                sample::gLogFatal << e.what() << std::endl;
            }
        };

        /**
         * @brief Return whether the recorder currently has no errors.
         */
        bool empty() const noexcept
        {
            return mErrorStack.empty();
        }

        /**
         * @brief Append a runtime error to the recorder.
         */
        bool reportError(infer1::ErrorCode val, IErrorRecorder::ErrorDesc desc) noexcept final {
            try
            {
                std::lock_guard<std::mutex> guard(mStackLock);
                mErrorStack.push_back(errorPair(val, desc));
            }
            catch(const std::exception& e)
            {
                sample::gLogFatal << e.what() << std::endl;
            }
            // Tell the runtime that the reported error should be treated as fatal.
            return true;
        }

        /**
         * @brief Atomically increment the runtime-owned reference counter.
         */
        IErrorRecorder::RefCount incRefCount() noexcept final
        {
            return ++mRefCount;
        }
        /**
         * @brief Atomically decrement the runtime-owned reference counter.
         */
        IErrorRecorder::RefCount decRefCount() noexcept final
        {
            return --mRefCount;
        }

    private:
        /**
         * @brief Return the stored error pair after callers have validated the index.
         */
        const errorPair& operator[](size_t index) const noexcept
        {
            return mErrorStack[index];
        }

        /**
         * @brief Check whether a signed error index is outside the current stack.
         */
        bool indexCheck(int32_t index) const noexcept
        {
            // By converting signed to unsigned, we only need a single check since
            // negative numbers turn into large positive greater than the size.
            size_t sIndex = index;
            return sIndex >= mErrorStack.size();
        }
        // Protects mErrorStack during report and clear operations.
        std::mutex mStackLock;

        // Reference count managed by the runtime interface.
        std::atomic<int32_t> mRefCount{0};

        // Stored runtime error codes and descriptions.
        errorStack mErrorStack;
}; // class SampleErrorRecorder
#endif // ERROR_RECORDER_H
