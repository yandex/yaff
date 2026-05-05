#pragma once

#include <algorithm>
#include <cstring>

#include "base.h"

namespace NYaFF {

class TDetachedSegment {
public:
    TDetachedSegment() noexcept : Allocated_(0), Size_(0), Buf_(nullptr), Cur_(nullptr) {
    }

    TDetachedSegment(char* buf, size_t allocated, char* cur, size_t size) noexcept
        : Allocated_(allocated), Size_(size), Buf_(buf), Cur_(cur) {
    }

    friend void swap(TDetachedSegment& a, TDetachedSegment& b) noexcept {
        std::swap(a.Allocated_, b.Allocated_);
        std::swap(a.Size_, b.Size_);
        std::swap(a.Buf_, b.Buf_);
        std::swap(a.Cur_, b.Cur_);
    }

    TDetachedSegment(const TDetachedSegment&) = delete;
    TDetachedSegment& operator=(const TDetachedSegment&) = delete;

    TDetachedSegment(TDetachedSegment&& r) noexcept : TDetachedSegment() {
        swap(*this, r);
    }

    TDetachedSegment& operator=(TDetachedSegment&& r) noexcept {
        swap(*this, r);
        return *this;
    }

    ~TDetachedSegment() {
        Clear();
    }

    const char* Data() const noexcept {
        return Cur_;
    }

    size_t Size() const noexcept {
        return Size_;
    }

private:
    void Clear() {
        delete[] Buf_;
        Allocated_ = 0;
        Size_ = 0;
        Buf_ = nullptr;
        Cur_ = nullptr;
    }

    size_t Allocated_;
    size_t Size_;

    char* Buf_;
    char* Cur_;
};

class TDualBuffer {
public:
    explicit TDualBuffer(size_t initialSize = 0) noexcept
        : InitialSize_(initialSize), Allocated_(0), Buf_(nullptr), Left_(nullptr), Right_(nullptr) {
    }

    friend void swap(TDualBuffer& a, TDualBuffer& b) noexcept {
        std::swap(a.InitialSize_, b.InitialSize_);
        std::swap(a.Allocated_, b.Allocated_);
        std::swap(a.Buf_, b.Buf_);
        std::swap(a.Left_, b.Left_);
        std::swap(a.Right_, b.Right_);
    }

    TDualBuffer(const TDualBuffer&) = delete;
    TDualBuffer& operator=(const TDualBuffer&) = delete;

    TDualBuffer(TDualBuffer&& other) noexcept : TDualBuffer() {
        swap(*this, other);
    }

    TDualBuffer& operator=(TDualBuffer&& other) noexcept {
        swap(*this, other);
        return *this;
    }

    ~TDualBuffer() {
        ClearBuffer();
    }

    void Reset() noexcept {
        ClearBuffer();
        Clear();
    }

    void Clear() noexcept {
        LeftClear();
        RightClear();
        Allocated_ = (Buf_ ? Allocated_ : 0);
    }

    // Left side: grows upward from the beginning of the buffer.
    char* LeftAllocate(size_t len) {
        if (len) {
            EnsureSpace(len);
            char* ptr = Left_;
            Left_ += len;
            return ptr;
        }
        return Left_;
    }

    template <typename T>
    void LeftPush(const T* data, size_t len) {
        if (len > 0) {
            const size_t bytes = len * sizeof(T);
            YAFF_MEMCPY(LeftAllocate(bytes), data, bytes);
        }
    }

    template <typename T>
    void LeftPushSmall(const T& value) {
        YAFF_MEMCPY(LeftAllocate(sizeof(T)), &value, sizeof(T));
    }

    void LeftFill(size_t len) {
        if (len) {
            std::memset(LeftAllocate(len), 0, len);
        }
    }

    void LeftPop(size_t len) noexcept {
        Left_ -= len;
    }

    void LeftClear() noexcept {
        Left_ = Buf_;
    }

    char* LeftData() const noexcept {
        return Buf_;
    }

    size_t LeftSize() const noexcept {
        return static_cast<size_t>(Left_ - Buf_);
    }

    char* LeftDataAt(size_t offset) const noexcept {
        return Buf_ + offset;
    }

    size_t LeftOffsetAt(const char* data) const noexcept {
        return static_cast<size_t>(data - Buf_);
    }

    TDetachedSegment LeftDetach() noexcept {
        TDetachedSegment detached(Buf_, Allocated_, Left_, LeftSize());
        Buf_ = nullptr;
        Clear();
        return detached;
    }

    // Right side: grows downward from the end of the buffer.
    char* RightAllocate(size_t len) {
        if (len) {
            EnsureSpace(len);
            Right_ -= len;
        }
        return Right_;
    }

    template <typename T>
    void RightPush(const T* data, size_t len) {
        if (len > 0) {
            const size_t bytes = len * sizeof(T);
            YAFF_MEMCPY(RightAllocate(bytes), data, bytes);
        }
    }

    template <typename T>
    void RightPushSmall(const T& value) {
        YAFF_MEMCPY(RightAllocate(sizeof(T)), &value, sizeof(T));
    }

    void RightFill(size_t len) {
        if (len) {
            std::memset(RightAllocate(len), 0, len);
        }
    }

    void RightPop(size_t len) noexcept {
        Right_ += len;
    }

    void RightClear() noexcept {
        Right_ = (Buf_ ? Buf_ + Allocated_ : nullptr);
    }

    char* RightData() const noexcept {
        return Right_;
    }

    size_t RightSize() const noexcept {
        return static_cast<size_t>(Buf_ + Allocated_ - Right_);
    }

    char* RightDataAt(size_t offset) const noexcept {
        return Buf_ + Allocated_ - offset;
    }

    size_t RightOffsetAt(const char* data) const noexcept {
        return static_cast<size_t>(Buf_ + Allocated_ - data);
    }

    TDetachedSegment RightDetach() noexcept {
        TDetachedSegment detached(Buf_, Allocated_, Right_, RightSize());
        Buf_ = nullptr;
        Clear();
        return detached;
    }

private:
    size_t UnusedSize() const noexcept {
        return static_cast<size_t>(Right_ - Left_);
    }

    void ClearBuffer() noexcept {
        delete[] Buf_;
        Buf_ = nullptr;
    }

    void EnsureSpace(size_t len) {
        if (len > UnusedSize()) {
            Reallocate(len);
        }
    }

    void Reallocate(size_t additional) {
        const size_t oldAllocated = Allocated_;
        const size_t leftSize = LeftSize();
        const size_t rightSize = RightSize();

        const size_t growth = std::max<size_t>(additional, oldAllocated ? oldAllocated / 2 : InitialSize_);
        if (growth > std::numeric_limits<size_t>::max() - Allocated_) {
            throw std::bad_alloc{};
        }
        Allocated_ += growth;

        char* newBuf = new char[Allocated_];
        if (Buf_) {
            YAFF_MEMCPY(newBuf + Allocated_ - rightSize, Buf_ + oldAllocated - rightSize, rightSize);
            YAFF_MEMCPY(newBuf, Buf_, leftSize);
            delete[] Buf_;
        }

        Buf_ = newBuf;
        Right_ = Buf_ + Allocated_ - rightSize;
        Left_ = Buf_ + leftSize;
    }

    size_t InitialSize_;
    size_t Allocated_;

    char* Buf_;
    char* Left_;
    char* Right_;
};

using TDetachedBuffer = TDetachedSegment;

}  // namespace NYaFF
