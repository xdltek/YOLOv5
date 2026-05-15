/**
 * @file sampleCommon.h
 * @brief Minimal runtime helper utilities shared by the YOLOv5 RPP demo.
 */

#ifndef SAMPLES_COMMON_SAMPLECOMMON_H_
#define SAMPLES_COMMON_SAMPLECOMMON_H_

#include "logging.h"

#include <cstdlib>
#include <cstdint>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>

#include "Infer.h"
#include "rpp_runtime.h"

using namespace infer1;

#if defined(__aarch64__) || defined(__QNX__)
#define ENABLE_DLA_API 1
#endif

#define CHECK_RETURN_W_MSG(status, val, errMsg)                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(status))                                                                                                 \
        {                                                                                                              \
            sample::LOG_ERROR() << errMsg << " Error in " << __FILE__ << ", function " << FN_NAME << "(), line " << __LINE__     \
                      << std::endl;                                                                                    \
            return val;                                                                                                \
        }                                                                                                              \
    } while (0)

#undef ASSERT
#define ASSERT(condition)                                                   \
    do                                                                      \
    {                                                                       \
        if (!(condition))                                                   \
        {                                                                   \
            sample::LOG_ERROR() << "Assertion failure: " << #condition << std::endl;  \
            throw std::runtime_error("Assertion failure");                             \
        }                                                                   \
    } while (0)


#define CHECK_RETURN(status, val) CHECK_RETURN_W_MSG(status, val, "")

/**
 * @brief Abort immediately when an RPP runtime API call fails.
 * @param error Status returned by an RPP runtime API call.
 */
constexpr void checkRTError(rtError_t error)
{
    if (error == rtError_t::rtSuccess) {
        return;
    }

    sample::LOG_ERROR() << "Rpp Runtime failure: " << error << std::endl;
    std::abort();
}

/**
 * @brief Convert an integer literal into bytes measured in GiB.
 * @param val Integer literal value.
 */
constexpr long long int operator "" _GiB(unsigned long long val)
{
    return val * (1 << 30);
}

/**
 * @brief Convert an integer literal into bytes measured in MiB.
 * @param val Integer literal value.
 */
constexpr long long int operator "" _MiB(unsigned long long val)
{
    return val * (1 << 20);
}

/**
 * @brief Convert an integer literal into bytes measured in KiB.
 * @param val Integer literal value.
 */
constexpr long long int operator "" _KiB(unsigned long long val)
{
    return val * (1 << 10);
}


namespace samplesCommon {
/**
 * @brief Convert a runtime data type enum into readable text for diagnostics.
 * @param t Runtime tensor data type.
 */
inline std::string data_type_to_string(infer1::DataType t)
{
    switch (t) {
        case infer1::DataType::kFLOAT:
            return "kFLOAT";
        case infer1::DataType::kHALF:
            return "kHALF";
        case infer1::DataType::kINT8:
            return "kINT8";
        case infer1::DataType::kINT32:
            return "kINT32";
        case infer1::DataType::kUINT32:
            return "kUINT32";
        case infer1::DataType::kBF:
            return "kBF";
        case infer1::DataType::kUINT8:
            return "kUINT8";
        case infer1::DataType::kINT16:
            return "kINT16";
        case infer1::DataType::kUINT16:
            return "kUINT16";
        case infer1::DataType::kBOOL:
            return "kBOOL";
        default:
            return "";
    }
}

/**
 * @brief Return the byte size of one tensor element for supported runtime data types.
 * @param t Runtime tensor data type.
 */
inline uint32_t getElementSize(infer1::DataType t)
{
    switch (t) {
        case infer1::DataType::kINT32:
            return 4;
        case infer1::DataType::kFLOAT:
            return 4;
        case infer1::DataType::kHALF:
            return 2;
        case infer1::DataType::kINT8:
            return 1;
        default:
            throw std::runtime_error("unsupported data type: " + data_type_to_string(t));
    }
}

/**
 * @brief Compute the number of elements described by an infer1::Dims object.
 * @param d Runtime tensor dimensions.
 */
inline int64_t volume(const infer1::Dims &d)
{
    return std::accumulate(d.d, d.d + d.nbDims, 1, std::multiplies<int64_t>());
}
} // namespace samplesCommon

#endif // SAMPLES_COMMON_SAMPLECOMMON_H_
