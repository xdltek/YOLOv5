/** @file rpp_buffer_manager.h
 *
 * @brief IO buffer manager
 * @author XDLTek Technologies
 * COPYRIGHT(c) 2020-2022 XDLTek Technologies.
 * ALL RIGHTS RESERVED
 *
 * This is Unpublished Proprietary Source Code of XDLTek Technologies
 */

#ifndef RPPRT_CORE_BUILDER_BUFFER_MANAGER_H_
#define RPPRT_CORE_BUILDER_BUFFER_MANAGER_H_

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#include "sampleCommon.h"

#include "Infer.h"
#include "InferRuntimeCommon.h"
#include "rpp_runtime.h"

namespace samplesCommon {
    /**
     * @brief Typed RAII byte buffer backed by caller-provided allocation and free functors.
     *
     * The buffer stores element count and data type separately so it can allocate the
     * correct byte count for RppRT binding tensors while still exposing raw pointers to
     * the execution context.
     */
    template<typename AllocFunc, typename FreeFunc>
    class GenericBuffer {
    public:
        /**
         * @brief Construct an empty buffer with a known tensor data type.
         */
        GenericBuffer(infer1::DataType type = infer1::DataType::kFLOAT)
                : mSize(0), mCapacity(0), mType(type), mBuffer(nullptr) {
        }

        /**
         * @brief Construct and allocate a buffer for the requested element count.
         * @param size Number of tensor elements.
         * @param type Tensor element data type.
         */
        GenericBuffer(size_t size, infer1::DataType type)
                : mSize(size), mCapacity(size), mType(type) {
            if (!allocFn(&mBuffer, this->nbBytes())) {
                throw std::bad_alloc();
            }
        }

        /**
         * @brief Move ownership from another buffer without copying device memory.
         */
        GenericBuffer(GenericBuffer &&buf)
                : mSize(buf.mSize), mCapacity(buf.mCapacity), mType(buf.mType), mBuffer(buf.mBuffer) {
            buf.mSize = 0;
            buf.mCapacity = 0;
            buf.mType = infer1::DataType::kFLOAT;
            buf.mBuffer = nullptr;
        }

        /**
         * @brief Move-assign ownership from another buffer.
         */
        GenericBuffer &operator=(GenericBuffer &&buf) {
            if (this != &buf) {
                freeFn(mBuffer);
                mSize = buf.mSize;
                mCapacity = buf.mCapacity;
                mType = buf.mType;
                mBuffer = buf.mBuffer;
                // Reset buf.
                buf.mSize = 0;
                buf.mCapacity = 0;
                buf.mBuffer = nullptr;
            }
            return *this;
        }

        /**
         * @brief Return a mutable raw pointer to the underlying allocation.
         */
        void *data() {
            return mBuffer;
        }

        /**
         * @brief Return a const raw pointer to the underlying allocation.
         */
        const void *data() const {
            return mBuffer;
        }

        /**
         * @brief Return the logical element count.
         */
        size_t size() const {
            return mSize;
        }

        /**
         * @brief Return the current allocation size in bytes.
         */
        size_t nbBytes() const {
            return this->size() * samplesCommon::getElementSize(mType);
        }

        /**
         * @brief Resize the logical element count and grow allocation when capacity is insufficient.
         * @param newSize New logical element count.
         */
        void resize(size_t newSize) {
            mSize = newSize;
            if (mCapacity < newSize) {
                freeFn(mBuffer);
                if (!allocFn(&mBuffer, this->nbBytes())) {
                    throw std::bad_alloc{};
                }
                mCapacity = newSize;
            }
        }

        /**
         * @brief Resize from an RppRT tensor dimension object.
         * @param dims Tensor dimensions whose volume becomes the new size.
         */
        void resize(const infer1::Dims &dims) {
            return this->resize(samplesCommon::volume(dims));
        }

        ~GenericBuffer() {
            freeFn(mBuffer);
        }

    private:
        size_t mSize{0}, mCapacity{0};
        infer1::DataType mType;
        void *mBuffer;
        AllocFunc allocFn;
        FreeFunc freeFn;
    };

    /**
     * @brief Device DDR allocator used by RppBufferManager binding buffers.
     */
    class DeviceAllocator {
    public:
        /**
         * @brief Allocate a device buffer and initialize it with 0xff.
         * @param ptr Output device pointer.
         * @param size Allocation size in bytes.
         */
        bool operator()(void **ptr, size_t size) const {
            if (rtMalloc(ptr, size) != rtError_t::rtSuccess) {
                return false;
            }
            // fill with 0xff
            const auto host_buffer_ptr = std::unique_ptr<void, decltype(free)*>{ malloc(size), free };
            memset(host_buffer_ptr.get(), 0xff, size);
            rtMemcpy(*ptr, host_buffer_ptr.get(), size, rtMemcpyHostToDevice);

            return true;
        }
    };

    /**
     * @brief Device DDR deleter used by GenericBuffer.
     */
    class DeviceFree {
    public:
        /**
         * @brief Release a device allocation owned by GenericBuffer.
         * @param ptr Device pointer to release.
         */
        void operator()(void *ptr) const {
            if (ptr != nullptr)
            {
                checkRTError(rtFree(ptr));
            }
        }
    };


    using DeviceBuffer = GenericBuffer<DeviceAllocator, DeviceFree>;

    /**
     * @brief One managed device binding buffer owned by the runtime buffer manager.
     */
    class ManagedBuffer {
    public:
        DeviceBuffer deviceBuffer;
    };

    /**
     * @brief Allocate and expose one device buffer for each RppRT engine binding.
     *
     * The manager keeps binding pointers stable for the lifetime of the engine
     * execution context and releases all buffers automatically.
     */
    class RppBufferManager {
    public:
        /**
         * @brief Allocate device buffers for every engine binding.
         * @param iengine Runtime engine that owns binding metadata.
         * @param batchSize Batch count multiplied into every binding volume.
         */
        RppBufferManager(std::shared_ptr<infer1::IEngine> iengine, const int batchSize = 1)
                : engine_(iengine) {
            // Allocate one device buffer per engine binding and keep the binding vector stable.
            for (int i = 0; i < engine_->getNbBindings(); i++) {
                auto dims = engine_->getBindingDimensions(i);
                size_t vol = batchSize;
                infer1::DataType type = engine_->getBindingDataType(i);

                vol *= samplesCommon::volume(dims);
                std::unique_ptr<ManagedBuffer> manBuf{new ManagedBuffer()};

                manBuf->deviceBuffer = DeviceBuffer(vol, type);

                device_bindings_.emplace_back(manBuf->deviceBuffer.data());
                managed_buffers_.emplace_back(std::move(manBuf));
            }
        }


        /**
         * @brief Return mutable device binding pointers for execution context calls.
         */
        std::vector<void *> &getDeviceBindings() {
            return device_bindings_;
        }

        /**
         * @brief Return const device binding pointers for read-only inspection.
         */
        const std::vector<void *> &getDeviceBindings() const {
            return device_bindings_;
        }

        /**
         * @brief Return the device buffer corresponding to a tensor name.
         * @param tensorName Engine binding name.
         */
        void *getDeviceBuffer(const std::string &tensorName) const {
            return getBuffer(tensorName);
        }

        ~RppBufferManager() = default;

    private:
        /**
         * @brief Resolve a binding name to its managed device buffer.
         * @param tensorName Engine binding name.
         */
        void *getBuffer(const std::string &tensorName) const {
            int index = engine_->getBindingIndex(tensorName.c_str());
            if (index == -1)
                return nullptr;
            return managed_buffers_[index]->deviceBuffer.data();
        }

        // Runtime engine that provides binding names, dimensions, and data types.
        std::shared_ptr<infer1::IEngine> engine_;
        // Owning storage for one managed buffer per engine binding.
        std::vector<std::unique_ptr<ManagedBuffer>> managed_buffers_;
        // Raw device pointers passed directly to the execution context.
        std::vector<void *> device_bindings_;
    };
}
#endif // RPPRT_CORE_BUILDER_BUFFER_MANAGER_H_
