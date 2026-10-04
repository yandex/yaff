#pragma once

#include <array>
#include <cstddef>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "array.h"
#include "base.h"
#include "buffer.h"

namespace yaff::exp {

class Serializer;

}

namespace yaff {

enum class SerializerType : int32_t {
    SERIALIZER_TYPE_DEFAULT = 0,
    SERIALIZER_TYPE_EXPERIMENTAL = 1,
};

template <typename P, typename E>
concept CElementProducerResult = requires(P p) {
    { p.first } -> std::convertible_to<E>;
    { p.second } -> std::convertible_to<bool>;
};

template <typename E, typename G>
concept CElementGenerator = std::invocable<G, size_t> and std::convertible_to<std::invoke_result_t<G, size_t>, E>;

template <typename E, typename P>
concept CElementProducer = std::invocable<P, size_t> and CElementProducerResult<std::invoke_result_t<P, size_t>, E>;

template <typename E, typename U, typename D>
concept CElementDeferrer =
    std::invocable<D, const U&> and std::convertible_to<std::invoke_result_t<std::invoke_result_t<D, const U&>>, E>;

struct MessageAssume {
    enum class Flag {
        UNKNOWN,
        NO,
        YES,
    };

    constexpr bool IsSpecified() const {
        return Layout != MessageLayout::MESSAGE_LAYOUT_UNKNOWN || Explicit != Flag::UNKNOWN || Sized != Flag::UNKNOWN ||
               Preallocated != Flag::UNKNOWN || Guarded != Flag::UNKNOWN;
    }

    constexpr bool IsMatching(MessageAssume other) const {
        if (Layout != MessageLayout::MESSAGE_LAYOUT_UNKNOWN && Layout != other.Layout) {
            if (Layout != MessageLayout::MESSAGE_LAYOUT_DYNAMIC) {
                return false;
            }
            if (other.Layout != MessageLayout::MESSAGE_LAYOUT_FLAT &&
                other.Layout != MessageLayout::MESSAGE_LAYOUT_SPARSE) {
                return false;
            }
        }
        if (Explicit != Flag::UNKNOWN && Explicit != other.Explicit) {
            return false;
        }
        if (Sized != Flag::UNKNOWN && Sized != other.Sized) {
            return false;
        }
        if (Preallocated != Flag::UNKNOWN && Preallocated != other.Preallocated) {
            return false;
        }
        if (Guarded != Flag::UNKNOWN && Guarded != other.Guarded) {
            return false;
        }
        return true;
    }

    // A specified layout guarantees an unfinished serializer with an active message.
    MessageLayout Layout = MessageLayout::MESSAGE_LAYOUT_UNKNOWN;
    Flag Explicit = Flag::UNKNOWN;
    Flag Sized = Flag::UNKNOWN;
    Flag Preallocated = Flag::UNKNOWN;
    Flag Guarded = Flag::UNKNOWN;
};

class SparseMessageAssume {
public:
    explicit SparseMessageAssume(bool implicit = false) noexcept : FieldsSize_(0), MaxId_(0), Implicit_(implicit) {
    }

    template <typename T>
    YAFF_ALWAYS_INLINE void AccountField(FieldId id, T value, T def) {
        if (Implicit_ && IsEqual(value, def)) {
            return;
        }
        TrackField<T>(id);
    }

    template <typename T>
    YAFF_ALWAYS_INLINE void AccountField(FieldId id, InternalOffset<T> offset) {
        if (offset.IsNull()) {
            return;
        }
        TrackField<Offset>(id);
    }

private:
    friend class Serializer;

    template <typename T>
    YAFF_ALWAYS_INLINE void TrackField(FieldId id) {
        static_assert(sizeof(T) <= sizeof(uint64_t));
        YAFF_REQUIRE(id > 0 && id < 0x1FFF);
        YAFF_REQUIRE(FieldsSize_ <= std::numeric_limits<FieldOffset>::max() - sizeof(FieldId) - sizeof(T));
        FieldsSize_ += sizeof(T);
        MaxId_ = std::max(MaxId_, id);
    }

    uint32_t FieldsSize_;
    FieldId MaxId_;

    const bool Implicit_;
};

class Serializer {
public:
    explicit Serializer(size_t initialSize = 0, bool = false)
        : Serializer(SerializerType::SERIALIZER_TYPE_DEFAULT, initialSize) {
    }

    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;

    Serializer(Serializer&& other) = delete;
    Serializer& operator=(Serializer&& other) = delete;

    ~Serializer() = default;

    void EnforceDynamicAlternative(MessageLayout layout) noexcept {
        ForcedAlternative_ = layout;
    }

    MessageLayout GetForcedDynamicAlternative() const noexcept {
        return ForcedAlternative_;
    }

    void Reset() noexcept {
        Clear();
        Buf_.Reset();
    }

    void Clear() noexcept {
        Buf_.Clear();
        MessageSerializer_.emplace<DummyMessageSerializer>();
        Depth_ = 0;
        Finished_ = false;
        ObjectSet_.clear();
    }

    template <typename T, MessageAssume A = MessageAssume{}>
    YAFF_ALWAYS_INLINE void AddField(FieldId fieldId, T value, T def) {
        AddFieldDispatch<A>(fieldId, value, def);
    }

    template <typename T, MessageAssume A = MessageAssume{}>
    YAFF_ALWAYS_INLINE void AddField(FieldId fieldId, InternalOffset<T> offset) {
        AddFieldDispatch<A>(fieldId, InternalOffset<>{offset.O});
    }

    template <typename M, bool G = true>
    YAFF_ALWAYS_INLINE void StartFixedMessage() {
        // Nesting is possible for this call, but not another message, since no FieldOffset is set.
        YAFF_REQUIRE(std::holds_alternative<DummyMessageSerializer>(MessageSerializer_));
        StartMessageDispatch<FixedMessageSerializer<G>>(Buf_, std::in_place_type<M>);
    }

    template <MessageAssume A = MessageAssume{}>
    YAFF_ALWAYS_INLINE Offset FinishFixedMessage() {
        return FinishMessageDispatch<A, FixedMessageSerializer<true>, FixedMessageSerializer<false>>();
    }

    template <typename M, bool G = true>
        requires(M::DELETED_IDS.empty())
    YAFF_ALWAYS_INLINE void StartFlatMessage(bool implicit = false, bool sized = false) {
        CheckNotNested();
        if (implicit) {
            if (sized) {
                StartMessageDispatch<FlatMessageSerializer<false, true, G>>(Buf_, std::in_place_type<M>);
            } else {
                StartMessageDispatch<FlatMessageSerializer<false, false, G>>(Buf_, std::in_place_type<M>);
            }
        } else {
            if (sized) {
                StartMessageDispatch<FlatMessageSerializer<true, true, G>>(Buf_, std::in_place_type<M>);
            } else {
                StartMessageDispatch<FlatMessageSerializer<true, false, G>>(Buf_, std::in_place_type<M>);
            }
        }
    }

    template <MessageAssume A = MessageAssume{}>
    YAFF_ALWAYS_INLINE Offset FinishFlatMessage() {
        return FinishMessageDispatch<
            A, FlatMessageSerializer<true, true, true>, FlatMessageSerializer<true, false, true>,
            FlatMessageSerializer<false, true, true>, FlatMessageSerializer<false, false, true>,
            FlatMessageSerializer<true, true, false>, FlatMessageSerializer<true, false, false>,
            FlatMessageSerializer<false, true, false>, FlatMessageSerializer<false, false, false>>();
    }

    template <bool G = true>
    YAFF_ALWAYS_INLINE void StartSparseMessage(bool implicit = false) {
        CheckNotNested();
        if (implicit) {
            StartMessageDispatch<SparseMessageSerializer<false, false, G>>(Buf_);
        } else {
            StartMessageDispatch<SparseMessageSerializer<true, false, G>>(Buf_);
        }
    }

    template <bool G = true>
    YAFF_ALWAYS_INLINE void StartSparseMessage(SparseMessageAssume assume) {
        CheckNotNested();
        if (assume.Implicit_) {
            auto& serializer = StartMessageDispatch<SparseMessageSerializer<false, true, G>>(Buf_);
            serializer.Initialize(assume);
        } else {
            auto& serializer = StartMessageDispatch<SparseMessageSerializer<true, true, G>>(Buf_);
            serializer.Initialize(assume);
        }
    }

    template <MessageAssume A = MessageAssume{}>
    YAFF_ALWAYS_INLINE Offset FinishSparseMessage() {
        return FinishMessageDispatch<
            A, SparseMessageSerializer<true, false, true>, SparseMessageSerializer<false, false, true>,
            SparseMessageSerializer<true, true, true>, SparseMessageSerializer<false, true, true>,
            SparseMessageSerializer<true, false, false>, SparseMessageSerializer<false, false, false>,
            SparseMessageSerializer<true, true, false>, SparseMessageSerializer<false, true, false>>();
    }

    template <typename T>
    InternalOffset<Array<T>> SerializeArray(const T* data, size_t len) {
        CheckNotFinished();
        CheckNotNested();
        CheckLength(len);
        if (len == 0) {
            return 0;
        }
        const size_t start = StartArray();
        Buf_.RightPush(reinterpret_cast<const std::byte*>(data), len * sizeof(T));
        Buf_.RightPushSmall<uint32_t>(len);
        return InternalOffset<Array<T>>(FinishArray(start));
    }

    template <typename T>
    InternalOffset<Array<InternalOffset<T>>> SerializeArray(const InternalOffset<T>* data, size_t len) {
        CheckNotFinished();
        CheckNotNested();
        CheckLength(len);
        if (len == 0) {
            return 0;
        }
        const size_t start = StartArray();
        const size_t byteSize = len * sizeof(Offset) + sizeof(uint32_t);
        const Offset end = ToCheckedOffset(Buf_.RightSize() + byteSize);
        const Offset objectStart = end - sizeof(uint32_t);
        std::byte* const block = Buf_.RightAllocate(byteSize);
        YAFF_ASSUME_SEPARATE_STORAGE(block, this);
        WriteValue<uint32_t>(block, len);
        for (size_t idx = len; idx > 0;) {
            const InternalOffset<T> offset = data[--idx];
            YAFF_REQUIRE(objectStart >= offset.O);
            const Offset relative = !offset.IsNull() ? objectStart - offset.O : 0;
            WriteValue<Offset>(block + sizeof(uint32_t) + idx * sizeof(Offset), relative);
        }
        // Disable deduplication for offset vectors because it breaks the relational offset logic.
        return InternalOffset<Array<InternalOffset<T>>>(FinishArray(start, /* noDedup */ true));
    }

    template <typename T>
    InternalOffset<Array<T>> SerializeArray(const std::vector<T>& vec) {
        return SerializeArray(vec.data(), vec.size());
    }

    InternalOffset<Array<bool>> SerializeArray(const std::vector<bool>& vec) {
        CheckNotFinished();
        CheckNotNested();
        CheckLength(vec.size());
        if (vec.size() == 0) {
            return 0;
        }
        const size_t start = StartArray();
        std::byte* const block = Buf_.RightAllocate(sizeof(uint32_t) + vec.size());
        YAFF_ASSUME_SEPARATE_STORAGE(block, this);
        WriteValue<uint32_t>(block, vec.size());
        for (size_t idx = vec.size(); idx > 0;) {
            --idx;
            WriteValue<uint8_t>(block + sizeof(uint32_t) + idx, vec[idx]);
        }
        return InternalOffset<Array<bool>>(FinishArray(start));
    }

    template <typename T, typename F>
        requires CElementGenerator<T, F>
    InternalOffset<Array<T>> SerializeArray(size_t len, F&& gen) {
        if (len == 0) {
            return 0;
        }
        if (len == 1) {
            const T element = gen(0);
            return SerializeArray(&element, 1);
        }
        std::vector<T> elements;
        elements.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            elements.emplace_back(gen(i));
        }
        return SerializeArray(elements);
    }

    template <typename T, typename F>
        requires CElementProducer<T, F>
    InternalOffset<Array<T>> SerializeArray(F&& produce) {
        // N.B.: This function cannot produce empty vectors, so the length must be checked on caller side.
        CheckNotFinished();
        CheckNotNested();
        bool next = true;
        size_t len = 0;
        size_t elementSize = 0;
        const size_t start = StartArray();
        while (next) {
            size_t before = Buf_.RightSize();
            std::tie(std::ignore, next) = produce(len++);
            size_t after = Buf_.RightSize();

            YAFF_REQUIRE(after >= before);
            const size_t delta = after - before;
            if (len == 1) {
                // N.B.: Since there is no way to check for emptiness in this function,
                // produce must generate at least one element.
                YAFF_REQUIRE(delta > 0);
                elementSize = delta;
                continue;
            }
            YAFF_REQUIRE(elementSize == delta);
            YAFF_REQUIRE(len <= std::numeric_limits<uint32_t>::max());
        }
        Buf_.RightPushSmall<uint32_t>(len);
        // TODO: support better interface to enable deduplication where it possible.
        return InternalOffset<Array<T>>(FinishArray(start, /* noDedup */ true));
    }

    template <typename T, typename F>
        requires CElementDeferrer<T, size_t, F>
    InternalOffset<Array<T>> SerializeArray(size_t len, F&& deferrer) {
        if (len == 0) {
            return 0;
        }
        std::vector<std::invoke_result_t<F, size_t>> producers;
        producers.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            producers.emplace_back(deferrer(i));
        }
        return SerializeArray<T>([&](size_t i) {
            return std::make_pair(static_cast<T>(producers[producers.size() - i - 1]()), i + 1 < producers.size());
        });
    }

    template <typename T, typename U, typename F>
        requires CElementDeferrer<T, U, F>
    InternalOffset<Array<T>> SerializeArray(const std::vector<U>& args, F&& deferrer) {
        if (args.size() == 0) {
            return 0;
        }
        std::vector<std::invoke_result_t<F, const U&>> producers;
        producers.reserve(args.size());
        for (const auto& arg : args) {
            producers.emplace_back(deferrer(arg));
        }
        return SerializeArray<T>([&](size_t i) {
            return std::make_pair(static_cast<T>(producers[producers.size() - i - 1]()), i + 1 < producers.size());
        });
    }

    template <std::convertible_to<std::string_view> T>
    InternalOffset<String> SerializeString(const T& str) {
        const auto sv = static_cast<std::string_view>(str);

        CheckNotFinished();
        CheckNotNested();
        CheckLength(sv.size());
        const size_t start = StartArray(/*nullTerminated*/ true);
        if (sv.size() > 0) {
            Buf_.RightPush(sv.data(), sv.size());
        }
        Buf_.RightPushSmall<uint32_t>(sv.size());
        return InternalOffset<String>(FinishArray(start));
    }

    template <typename T>
    void Finish(InternalOffset<T> offset) {
        CheckNotFinished();
        CheckNotNested();
        YAFF_REQUIRE(!offset.IsNull());
        Buf_.LeftClear();
        YAFF_REQUIRE(Buf_.RightSize() + sizeof(Offset) >= offset.O);
        Buf_.RightPushSmall<Offset>(ToCheckedOffset((Buf_.RightSize() + sizeof(Offset)) - offset.O));
        Finished_ = true;
    }

    // N.B.: Incorrect use of this version of Finish function may result in UB.
    // Use this version of function only if you know exactly what you're doing.
    void FinishRaw() {
        CheckNotFinished();
        CheckNotNested();
        Buf_.LeftClear();
        Finished_ = true;
    }

    DetachedSegment Release() {
        CheckFinished();
        auto buffer = Buf_.RightDetach();
        Clear();
        return buffer;
    }

    const std::byte* Data() const {
        CheckFinished();
        return Buf_.RightData();
    }

    size_t Size() const {
        CheckFinished();
        return Buf_.RightSize();
    }

private:
    friend class yaff::exp::Serializer;

    struct OffsetsView {
        const FieldOffset* Data;
        size_t Size;

        template <size_t N>
        OffsetsView(const std::array<FieldOffset, N>& o) : Data(o.data()), Size(o.size()) {
        }
    };

    struct ObjectOffset {
        // It is possible that the message size will exceed the allowed MAX_OFFSET,
        // but due to deduplication it will return to normal.
        // To handle this case, Offset uses size_t to store offsets.
        size_t Offset = 0;
        size_t ByteSize = 0;
    };

    struct ObjectOffsetHash {
        inline size_t operator()(const ObjectOffset& offset) const noexcept {
            return std::hash<std::string_view>{}(
                {reinterpret_cast<const char*>(Buf.RightDataAt(offset.Offset)), offset.ByteSize});
        }
        DualBuffer& Buf;
    };

    struct ObjectOffsetEqual {
        inline bool operator()(const ObjectOffset& lhs, const ObjectOffset& rhs) const noexcept {
            return lhs.ByteSize == rhs.ByteSize &&
                   IsMemoryEqual(Buf.RightDataAt(lhs.Offset), Buf.RightDataAt(rhs.Offset), lhs.ByteSize);
        }
        DualBuffer& Buf;
    };

    using ObjectOffsetSet = std::unordered_set<ObjectOffset, ObjectOffsetHash, ObjectOffsetEqual>;

    struct DummyMessageSerializer {
        template <typename... Args>
        void AddField(FieldId, Args...) {
            YAFF_THROW("no message is being serialized");
        }
        Offset Finish() && {
            YAFF_THROW("no message is being serialized");
        }
    };

    template <bool G>
    struct FixedMessageSerializer {
        template <typename M>
        FixedMessageSerializer(DualBuffer& buffer, std::in_place_type_t<M>) : Buf(buffer), Offsets(M::FLAT_OFFSETS) {
            Buf.RightFill(M::LIMIT);
            Loc = ToCheckedOffset(Buf.RightSize());
        }

        FixedMessageSerializer(const FixedMessageSerializer&) = delete;
        FixedMessageSerializer& operator=(const FixedMessageSerializer&) = delete;

        FixedMessageSerializer(FixedMessageSerializer&& other) = delete;
        FixedMessageSerializer& operator=(FixedMessageSerializer&& other) = delete;

        template <typename T>
        void AddField(FieldId id, T value, T def) {
            WriteField<T>(id, XorDef(value, def));
        }

        void AddField(FieldId id, InternalOffset<> offset) {
            if (offset.IsNull()) {
                return;
            }
            Guard<G>(Loc >= offset.O);
            WriteField<Offset>(id, Loc - offset.O);
        }

        Offset Finish() && {
            return Loc;
        }

        template <typename T>
        void WriteField(FieldId id, T value) {
            Guard<G>(id > 0 && id < Offsets.Size);
            const FieldOffset fieldOffset = Offsets.Data[id - 1];
            WriteValue<T>(Buf.RightDataAt(Loc - fieldOffset), value);
        }

        DualBuffer& Buf;
        Offset Loc;
        OffsetsView Offsets;
    };

    template <bool E, bool S, bool G>
    struct FlatMessageSerializer {
        struct SizeMasks {
            const std::byte* Expl = nullptr;
            const std::byte* Impl = nullptr;
        };

        template <bool B>
        struct Order {
            YAFF_ALWAYS_INLINE void Reset(FieldOffset) noexcept {
            }

            YAFF_ALWAYS_INLINE void Check(FieldOffset, size_t) noexcept {
            }
        };

        template <bool B>
            requires(B)
        struct Order<B> {
            YAFF_ALWAYS_INLINE void Reset(FieldOffset offset) {
                PrevOffset = offset;
            }

            YAFF_ALWAYS_INLINE void Check(FieldOffset offset, size_t size) {
                Guard<B>(PrevOffset >= offset + size);
                PrevOffset = offset;
            }

            FieldOffset PrevOffset = 0;
        };

        inline static constexpr size_t TYPED_LIMIT_SIZE = sizeof(FieldId);

        static constexpr size_t CalculateFieldMetaSize(const bool expl, const bool sized) {
            return static_cast<size_t>(expl) | (static_cast<size_t>(sized) << 1);
        }

        static constexpr size_t CalculateMetaSize(const FieldId maxId, const bool expl, const bool sized) {
            return (maxId * CalculateFieldMetaSize(expl, sized) + 7) >> 3;
        }

        static constexpr size_t CalculateCorrection(const size_t size) {
            return (size > 0) + (size > 1) + (size > 4);
        }

        template <typename M>
        static consteval auto BuildSizeMask() {
            constexpr size_t count = M::FLAT_OFFSETS.size();
            std::array<std::byte, CalculateMetaSize(count - 1, E, true)> mask{};
            for (size_t i = 0; i + 1 < count; ++i) {
                const size_t corr = CalculateCorrection(M::FLAT_OFFSETS[i + 1] - M::FLAT_OFFSETS[i]);
                const size_t index = i * CalculateFieldMetaSize(E, true) + E;
                mask[index >> 3] |= static_cast<std::byte>(corr) << (index & 7);
                if ((index & 7) == 7) {
                    mask[(index >> 3) + 1] |= static_cast<std::byte>(corr) >> 1;
                }
            }
            return mask;
        }

        template <typename M>
        inline static constexpr auto SIZE_MASK = BuildSizeMask<M>();

        template <bool M>
        static void ApplySizeMask(std::byte* maskStart, const std::byte* mask, const FieldId maxId, const bool expl) {
            const size_t bits = (maxId - 1) * CalculateFieldMetaSize(expl, true);
            if (bits <= 8) {
                const std::byte value = mask[0] & static_cast<std::byte>((1u << bits) - 1);
                if constexpr (M) {
                    *(maskStart - 1) |= value;
                } else {
                    *(maskStart - 1) = value;
                }
                return;
            }
            const size_t metaSize = (bits + 7) >> 3;
            for (size_t j = 0; j + 1 < metaSize; ++j) {
                if constexpr (M) {
                    *(maskStart - j - 1) |= mask[j];
                } else {
                    *(maskStart - j - 1) = mask[j];
                }
            }
            const size_t tailBits = bits - ((metaSize - 1) << 3);
            const std::byte tail = mask[metaSize - 1] & static_cast<std::byte>((1u << tailBits) - 1);
            if constexpr (M) {
                *(maskStart - metaSize) |= tail;
            } else {
                *(maskStart - metaSize) = tail;
            }
        }

        static void SetPresence(std::byte* maskStart, const FieldId id) {
            const size_t index = (id - 1) * CalculateFieldMetaSize(true, S);
            *(maskStart - (index >> 3) - 1) |= (static_cast<std::byte>(1) << (index & 7));
        }

        template <typename M>
        YAFF_ALWAYS_INLINE FlatMessageSerializer(DualBuffer& buffer, std::in_place_type_t<M>)
            : Buf(buffer),
              Start(Buf.RightSize()),
              End(0),
              MaxId(0),
              NeedExplicit(false),
              Base(nullptr),
              Offsets(M::FLAT_OFFSETS),
              Sizes{FlatMessageSerializer<true, true, true>::SIZE_MASK<M>.data(),
                    FlatMessageSerializer<false, true, true>::SIZE_MASK<M>.data()} {
            // N.B.: FLAT_OFFSETS has one entry per field ID plus a trailing offset, so its size is the
            // largest field ID plus one. AddField bounds id by this table, keeping id + 1 within 13 bits.
            static_assert(M::FLAT_OFFSETS.size() < 0x2000);
        }

        FlatMessageSerializer(const FlatMessageSerializer&) = delete;
        FlatMessageSerializer& operator=(const FlatMessageSerializer&) = delete;

        FlatMessageSerializer(FlatMessageSerializer&& other) = delete;
        FlatMessageSerializer& operator=(FlatMessageSerializer&& other) = delete;

        template <typename T>
        YAFF_ALWAYS_INLINE void AddField(const FieldId id, const T value, const T def) {
            if constexpr (!E) {
                if (IsEqual<T>(value, def)) {
                    return;
                }
            }

            Guard<G>(id > 0 && id < Offsets.Size);
            const FieldOffset offset = Offsets.Data[id - 1];
            TrackField<T>(id, offset, value, def);

            WriteField<T>(offset, XorDef(value, def));
        }

        YAFF_ALWAYS_INLINE void AddField(const FieldId id, const InternalOffset<> value) {
            if (value.IsNull()) {
                return;
            }

            Guard<G>(id > 0 && id < Offsets.Size);
            const FieldOffset offset = Offsets.Data[id - 1];
            TrackField<Offset>(id, offset, value.O, 0);

            Guard<G>(End >= value.O);
            WriteField<Offset>(offset, End - value.O);
        }

        YAFF_ALWAYS_INLINE Offset Finish() && {
            DualBuffer& buf = Buf;
            const Offset end = End;
            const FieldId maxId = MaxId;
            const bool trulyExplicit = (E && NeedExplicit);
            const std::byte* const sizes = trulyExplicit ? Sizes.Expl : Sizes.Impl;

            if (maxId == 0) {
                buf.RightPushSmall<FieldId>(0x8000);
                return ToCheckedOffset(buf.RightSize());
            }

            if (E && !trulyExplicit) {
                const size_t metaSize = CalculateMetaSize(maxId - 1, false, S);
                buf.RightPop(buf.RightSize() - end - metaSize);
            }

            if constexpr (S) {
                if (trulyExplicit) {
                    ApplySizeMask<true>(buf.RightDataAt(end), sizes, maxId, true);
                } else {
                    if constexpr (!E) {
                        buf.RightAllocate(CalculateMetaSize(maxId - 1, false, true));
                    }
                    ApplySizeMask<false>(buf.RightDataAt(end), sizes, maxId, false);
                }
            }

            const FieldId typedLimit = ((maxId << 2) | (0x8000 | (S << 1) | trulyExplicit));
            WriteValue<FieldId>(buf.RightDataAt(end), typedLimit);

            return end;
        }

        template <typename T>
        YAFF_ALWAYS_INLINE void TrackField(const FieldId id, const FieldOffset offset, const T val, const T def) {
            if (YAFF_UNLIKELY(IsEmpty())) {
                Initialize(id, offset, sizeof(T));
            } else {
                Ord.Check(offset, sizeof(T));
            }

            YAFF_ASSUME_SEPARATE_STORAGE(Base, this);
            if constexpr (E) {
                if (YAFF_UNLIKELY(IsEqual<T>(val, def))) {
                    NeedExplicit = true;
                }
                SetPresence(Base, id);
            }
        }

        YAFF_ALWAYS_INLINE void Initialize(const FieldId id, const FieldOffset offset, const size_t valueSize) {
            const size_t dataSize = TYPED_LIMIT_SIZE + offset + valueSize;
            const size_t metaSize = E * CalculateMetaSize(id, E, S);
            const size_t end = Start + dataSize;
            const OffsetsView offsets = Offsets;
            const SizeMasks sizes = Sizes;
            std::byte* const block = Buf.RightAllocate(dataSize + metaSize);
            std::memset(block, 0, dataSize + metaSize);

            YAFF_ASSUME(Offsets.Data == offsets.Data && Offsets.Size == offsets.Size);
            YAFF_ASSUME(Sizes.Expl == sizes.Expl && Sizes.Impl == sizes.Impl);

            End = ToCheckedOffset(end);
            MaxId = id + 1;
            Ord.Reset(offset);
            NeedExplicit = false;

            // Base stays valid while writing fields. Finish may reallocate
            // the buffer for implicit sized layouts.
            Base = block + metaSize;
            YAFF_ASSUME_SEPARATE_STORAGE(Base, this);
        }

        template <typename T>
        void WriteField(const FieldOffset offset, T value) {
            std::byte* const base = Base;
            YAFF_ASSUME_SEPARATE_STORAGE(base, this);
            WriteValue<T>(base + offset + TYPED_LIMIT_SIZE, value);
        }

        bool IsEmpty() const {
            return MaxId == 0;
        }

        DualBuffer& Buf;
        Offset Start;
        Offset End;

        FieldId MaxId;
        bool NeedExplicit;

        Order<G> Ord;
        std::byte* Base;

        const OffsetsView Offsets;
        const SizeMasks Sizes;
    };

    struct SparseMeta {
        inline static constexpr FieldId TINY_OFFSET_MAX_ID = 0x20;
        inline static constexpr FieldOffset SPARSE_META_OFFSET = sizeof(SignedOffset);

        static constexpr size_t CalculateMetaSize(const FieldId maxId) {
            return (maxId - 1) + (maxId > TINY_OFFSET_MAX_ID ? maxId - TINY_OFFSET_MAX_ID : 0);
        }

        // Signature: bits 0-12 hold maxId; bits 13-15 hold bits 2-4 of the first metadata byte.
        YAFF_LAYOUT_BEGIN(TMeta) {
            yaff::Offset Offset;
            yaff::FieldId Signature;
        };
        YAFF_LAYOUT_END

        static void WriteFieldMeta(std::byte* base, const FieldId id, const FieldOffset offset) {
            std::byte* const metaEnd = base - SPARSE_META_OFFSET;
            if (id < TINY_OFFSET_MAX_ID) {
                WriteValue<uint8_t>(metaEnd - id, offset);
            } else {
                WriteValue<FieldOffset>(metaEnd - ((id << 1) - (TINY_OFFSET_MAX_ID - 1)), offset);
            }
        }

        static Offset FinishMetadata(DualBuffer& buf, FieldId maxId, Offset msgStart) {
            const size_t metaSize = CalculateMetaSize(maxId);
            const size_t metaStart = buf.RightSize();
            const FieldId signature = maxId | ((std::to_integer<uint8_t>(*buf.RightData()) << 11) & 0xE000);
            Offset metaOffset = 0;  // Offset of meta can not be zero;
            for (size_t i = 0; i < buf.LeftSize(); i += sizeof(TMeta)) {
                const auto* candidate = ReadLayout<TMeta>(buf.LeftDataAt(i));
                if (candidate->Signature != signature) {
                    continue;
                }

                const std::byte* candidateMeta = buf.RightDataAt(candidate->Offset);
                const std::byte* meta = buf.RightDataAt(metaStart);
                if (!IsMemoryEqual(candidateMeta, meta, metaSize)) {
                    continue;
                }

                metaOffset = candidate->Offset;
                buf.RightPop(metaSize);
                break;
            }

            // This means we have unique meta, push for next generations;
            if (!metaOffset) {
                metaOffset = ToCheckedOffset(metaStart);
                buf.LeftPushSmall<TMeta>(TMeta{.Offset = metaOffset, .Signature = signature});
            }

            const SignedOffset relativeOffset =
                static_cast<SignedOffset>(msgStart) - static_cast<SignedOffset>(metaOffset - metaSize);
            WriteValue<SignedOffset>(buf.RightDataAt(msgStart + SPARSE_META_OFFSET), relativeOffset);
            return msgStart;
        }
    };

    template <bool E, bool P, bool G>
    struct SparseMessageSerializer {
        template <bool B>
        struct Order {
            YAFF_ALWAYS_INLINE void Check(FieldId) noexcept {
            }
        };

        template <bool B>
            requires(B)
        struct Order<B> {
            YAFF_ALWAYS_INLINE void Check(FieldId id) {
                Guard<B>(PrevId == 0 || PrevId > id);
                PrevId = id;
            }

            FieldId PrevId = 0;
        };

        YAFF_LAYOUT_BEGIN(TField) {
            yaff::Offset Offset;
            yaff::FieldId Id;
            bool IsScalar;
        };
        YAFF_LAYOUT_END

        SparseMessageSerializer(DualBuffer& buffer) : Buf(buffer), LeftStart(Buf.LeftSize()), FieldsAdded(0), MaxId(0) {
        }

        SparseMessageSerializer(const SparseMessageSerializer&) = delete;
        SparseMessageSerializer& operator=(const SparseMessageSerializer&) = delete;

        SparseMessageSerializer(SparseMessageSerializer&& other) = delete;
        SparseMessageSerializer& operator=(SparseMessageSerializer&& other) = delete;

        template <typename T>
        void AddField(FieldId id, T value, T def) {
            if constexpr (!E) {
                if (IsEqual(value, def)) {
                    return;
                }
            }
            Buf.RightPushSmall<T>(value);
            TrackField(id, Buf.RightSize());
        }

        void AddField(FieldId id, InternalOffset<> offset) {
            if (offset.IsNull()) {
                return;
            }
            Buf.RightPushSmall<Offset>(offset.O);
            TrackField(id, Buf.RightSize(), /*isScalar*/ false);
        }

        Offset Finish() && {
            if (IsEmpty()) {
                Buf.RightPushSmall<FieldId>(2);
                return ToCheckedOffset(Buf.RightSize());
            }

            Buf.RightPushSmall<FieldId>((MaxId << 2) | 3);
            const Offset msgStart = ToCheckedOffset(Buf.RightSize());

            const size_t metaSize = SparseMeta::CalculateMetaSize(MaxId);
            Buf.RightPushSmall<SignedOffset>(0);
            Buf.RightFill(metaSize);

            std::byte* const base = Buf.RightDataAt(msgStart);
            for (uint16_t i = 0; i < FieldsAdded; ++i) {
                const auto* field = ReadLayout<TField>(Buf.LeftDataAt(LeftStart + i * sizeof(TField)));

                const FieldOffset offset = (msgStart - field->Offset);
                SparseMeta::WriteFieldMeta(base, field->Id, offset);

                if (!field->IsScalar) {
                    std::byte* loc = Buf.RightDataAt(field->Offset);
                    const Offset offset = ReadValue<Offset>(loc);
                    Guard<G>(msgStart >= offset);
                    WriteValue<Offset>(loc, msgStart - offset);
                }
            }
            Buf.LeftPop(FieldsAdded * sizeof(TField));

            return SparseMeta::FinishMetadata(Buf, MaxId, msgStart);
        }

        void TrackField(FieldId id, size_t offset, bool isScalar = true) {
            Guard<G>(id > 0);
            Ord.Check(id);
            if (IsEmpty()) {
                Guard<G>(id < 0x1FFF);
                MaxId = id + 1;
            }

            TField field{.Offset = ToCheckedOffset(offset), .Id = id, .IsScalar = isScalar};
            Buf.LeftPushSmall<TField>(field);

            ++FieldsAdded;
        }

        bool IsEmpty() const {
            return MaxId == 0;
        }

        DualBuffer& Buf;
        Offset LeftStart;

        uint16_t FieldsAdded;
        FieldId MaxId;

        Order<G> Ord;
    };

    template <bool E, bool P, bool G>
        requires(P)
    struct SparseMessageSerializer<E, P, G> {
        template <bool B>
        struct Order {
            YAFF_ALWAYS_INLINE void Check(FieldId) noexcept {
            }
        };

        template <bool B>
            requires(B)
        struct Order<B> {
            YAFF_ALWAYS_INLINE void Check(FieldId id) {
                Guard<B>(PrevId == 0 || PrevId > id);
                PrevId = id;
            }

            FieldId PrevId = 0;
        };

        YAFF_ALWAYS_INLINE explicit SparseMessageSerializer(DualBuffer& buffer)
            : Buf(buffer), End(0), MaxId(0), PrevOffset(0), Base(nullptr) {
        }

        SparseMessageSerializer(const SparseMessageSerializer&) = delete;
        SparseMessageSerializer& operator=(const SparseMessageSerializer&) = delete;

        SparseMessageSerializer(SparseMessageSerializer&& other) = delete;
        SparseMessageSerializer& operator=(SparseMessageSerializer&& other) = delete;

        template <typename T>
        YAFF_ALWAYS_INLINE void AddField(const FieldId id, const T value, const T def) {
            if constexpr (!E) {
                if (IsEqual(value, def)) {
                    return;
                }
            }
            WriteField<T>(id, value);
        }

        YAFF_ALWAYS_INLINE void AddField(const FieldId id, const InternalOffset<> value) {
            if (value.IsNull()) {
                return;
            }
            Guard<G>(End >= value.O);
            WriteField<Offset>(id, End - value.O);
        }

        YAFF_ALWAYS_INLINE Offset Finish() && {
            PrevOffset = sizeof(FieldId);
            return IsEmpty() ? End : SparseMeta::FinishMetadata(Buf, MaxId, End);
        }

        YAFF_ALWAYS_INLINE void Initialize(SparseMessageAssume assume) {
            const size_t dataSize = sizeof(FieldId) + assume.FieldsSize_;
            const Offset end = ToCheckedOffset(Buf.RightSize() + dataSize);
            const FieldId maxId = assume.MaxId_ ? assume.MaxId_ + 1 : 0;
            const size_t metaSize = maxId ? SparseMeta::CalculateMetaSize(maxId) + SparseMeta::SPARSE_META_OFFSET : 0;

            std::byte* const block = Buf.RightAllocate(metaSize + dataSize);
            std::memset(block, 0, metaSize);

            End = end;
            MaxId = maxId;
            PrevOffset = dataSize;

            // Base is valid while filling fields; FinishMetadata may reallocate the buffer.
            Base = block + metaSize;
            YAFF_ASSUME_SEPARATE_STORAGE(Base, this);

            const FieldId tl = maxId ? (maxId << 2) | 3 : 2;
            WriteValue<FieldId>(Base, tl);
        }

        template <typename T>
        YAFF_ALWAYS_INLINE void WriteField(const FieldId id, T value) {
            Guard<G>(id > 0);
            Ord.Check(id);

            const FieldOffset offset = PrevOffset - sizeof(T);
            std::byte* const base = Base;
            YAFF_ASSUME_SEPARATE_STORAGE(base, this);

            SparseMeta::WriteFieldMeta(base, id, offset);
            WriteValue<T>(base + offset, value);

            PrevOffset = offset;
        }

        bool IsEmpty() const {
            return MaxId == 0;
        }

        DualBuffer& Buf;
        Offset End;

        FieldId MaxId;
        Offset PrevOffset;

        Order<G> Ord;
        std::byte* Base;
    };

    using MessageSerializer =
        std::variant<DummyMessageSerializer, FixedMessageSerializer<true>, FixedMessageSerializer<false>,
                     FlatMessageSerializer<true, true, true>, FlatMessageSerializer<true, false, true>,
                     FlatMessageSerializer<false, true, true>, FlatMessageSerializer<false, false, true>,
                     FlatMessageSerializer<true, true, false>, FlatMessageSerializer<true, false, false>,
                     FlatMessageSerializer<false, true, false>, FlatMessageSerializer<false, false, false>,
                     SparseMessageSerializer<true, false, true>, SparseMessageSerializer<false, false, true>,
                     SparseMessageSerializer<true, true, true>, SparseMessageSerializer<false, true, true>,
                     SparseMessageSerializer<true, false, false>, SparseMessageSerializer<false, false, false>,
                     SparseMessageSerializer<true, true, false>, SparseMessageSerializer<false, true, false>>;

    static consteval MessageAssume DescribeSerializerType(std::in_place_type_t<DummyMessageSerializer>) {
        return {};
    }

    template <bool G>
    static consteval MessageAssume DescribeSerializerType(std::in_place_type_t<FixedMessageSerializer<G>>) {
        return {.Layout = MessageLayout::MESSAGE_LAYOUT_FIXED,
                .Guarded = G ? MessageAssume::Flag::YES : MessageAssume::Flag::NO};
    }

    template <bool E, bool S, bool G>
    static consteval MessageAssume DescribeSerializerType(std::in_place_type_t<FlatMessageSerializer<E, S, G>>) {
        return {.Layout = MessageLayout::MESSAGE_LAYOUT_FLAT,
                .Explicit = E ? MessageAssume::Flag::YES : MessageAssume::Flag::NO,
                .Sized = S ? MessageAssume::Flag::YES : MessageAssume::Flag::NO,
                .Guarded = G ? MessageAssume::Flag::YES : MessageAssume::Flag::NO};
    }

    template <bool E, bool P, bool G>
    static consteval MessageAssume DescribeSerializerType(std::in_place_type_t<SparseMessageSerializer<E, P, G>>) {
        return {.Layout = MessageLayout::MESSAGE_LAYOUT_SPARSE,
                .Explicit = E ? MessageAssume::Flag::YES : MessageAssume::Flag::NO,
                .Preallocated = P ? MessageAssume::Flag::YES : MessageAssume::Flag::NO,
                .Guarded = G ? MessageAssume::Flag::YES : MessageAssume::Flag::NO};
    }

    template <bool G>
    YAFF_ALWAYS_INLINE static void Guard(bool condition) noexcept(!G) {
        if constexpr (G) {
            YAFF_REQUIRE(condition);
        } else {
            YAFF_ASSUME(condition);
        }
    }

    explicit Serializer(SerializerType type, size_t initialSize = 0, bool = false)
        : Type_(type),
          Buf_(initialSize),
          MessageSerializer_(),
          Depth_(0),
          ForcedAlternative_(MessageLayout::MESSAGE_LAYOUT_FLAT),
          ObjectSet_(8, ObjectOffsetHash(Buf_), ObjectOffsetEqual(Buf_)),
          Finished_(false) {
    }

    void CheckFinished() const {
        YAFF_REQUIRE(Finished_);
    }

    void CheckNotFinished() const {
        YAFF_REQUIRE(!Finished_);
    }

    void CheckNested() const {
        YAFF_REQUIRE(Depth_ > 0);
    }

    void CheckNotNested() const {
        YAFF_REQUIRE(Depth_ == 0);
    }

    void CheckLength(size_t len) const {
        YAFF_REQUIRE(len <= std::numeric_limits<uint32_t>::max());
    }

    void IncrementDepth() {
        ++Depth_;
    }

    void DecrementDepth() {
        --Depth_;
    }

    template <MessageAssume A>
    YAFF_ALWAYS_INLINE void AssumeMessage() const {
        if constexpr (A.Layout != MessageLayout::MESSAGE_LAYOUT_UNKNOWN) {
            YAFF_ASSUME(Depth_ > 0);
        }
        const bool matches = [&]<size_t... Is>(std::index_sequence<Is...>) {
            return ((A.IsMatching(DescribeSerializerType(
                         std::in_place_type<std::variant_alternative_t<Is, MessageSerializer>>)) &&
                     MessageSerializer_.index() == Is) ||
                    ...);
        }(std::make_index_sequence<std::variant_size_v<MessageSerializer>>{});
        YAFF_ASSUME(matches);
    }

    template <MessageAssume A, typename V, typename... Ps>
    YAFF_ALWAYS_INLINE bool TryAddField(FieldId fieldId, Ps... params) {
        if constexpr (A.IsMatching(DescribeSerializerType(std::in_place_type<V>))) {
            if (auto* serializer = std::get_if<V>(&MessageSerializer_)) {
                serializer->AddField(fieldId, params...);
                return true;
            }
        }
        return false;
    }

    template <MessageAssume A, size_t... Is, typename... Ps>
    YAFF_ALWAYS_INLINE void TryAddField(std::index_sequence<Is...>, FieldId fieldId, Ps... params) {
        (TryAddField<A, std::variant_alternative_t<Is, MessageSerializer>>(fieldId, params...) || ...);
    }

    template <MessageAssume A, typename... Ps>
    YAFF_ALWAYS_INLINE void AddFieldDispatch(FieldId fieldId, Ps... params) {
        if constexpr (A.IsSpecified()) {
            AssumeMessage<A>();
            TryAddField<A>(std::make_index_sequence<std::variant_size_v<MessageSerializer>>{}, fieldId, params...);
        } else {
            AddFieldDispatch(fieldId, params...);
        }
    }

    YAFF_NOINLINE void AddFieldDispatchSlow(FieldId fieldId, InternalOffset<> offset) {
        std::visit([&](auto& b) { b.AddField(fieldId, offset); }, MessageSerializer_);
    }

    YAFF_ALWAYS_INLINE void AddFieldDispatch(FieldId fieldId, InternalOffset<> offset) {
        // FlatMessageSerializer<true, true, true> is the most frequent alternative,
        // being effectively the default one: the dynamic layout requires sized
        // messages, and a message usually has some explicit fields.
        if (auto* flat = std::get_if<FlatMessageSerializer<true, true, true>>(&MessageSerializer_)) {
            flat->AddField(fieldId, offset);
            return;
        }
        AddFieldDispatchSlow(fieldId, offset);
    }

    template <typename T>
    YAFF_NOINLINE void AddFieldDispatchSlow(FieldId fieldId, T value, T def) {
        std::visit([&](auto& b) { b.AddField(fieldId, value, def); }, MessageSerializer_);
    }

    template <typename T>
    YAFF_ALWAYS_INLINE void AddFieldDispatch(FieldId fieldId, T value, T def) {
        // FlatMessageSerializer<true, true, true> is the most frequent alternative,
        // being effectively the default one: the dynamic layout requires sized
        // messages, and a message usually has some explicit fields.
        if (auto* flat = std::get_if<FlatMessageSerializer<true, true, true>>(&MessageSerializer_)) {
            flat->AddField(fieldId, value, def);
            return;
        }
        AddFieldDispatchSlow<T>(fieldId, value, def);
    }

    template <typename T, typename... Ps>
    YAFF_ALWAYS_INLINE T& StartMessageDispatch(Ps&&... params) {
        CheckNotFinished();
        IncrementDepth();
        return MessageSerializer_.emplace<T>(std::forward<Ps>(params)...);
    }

    template <MessageAssume A, typename V>
    YAFF_ALWAYS_INLINE Offset TryFinishMessage() {
        if constexpr (A.IsMatching(DescribeSerializerType(std::in_place_type<V>))) {
            if (auto* serializer = std::get_if<V>(&MessageSerializer_)) {
                return std::move(*serializer).Finish();
            }
        }
        return Offset{};
    }

    template <MessageAssume A, typename... Vs>
    YAFF_ALWAYS_INLINE Offset FinishMessageDispatch() {
        if constexpr (A.IsSpecified()) {
            AssumeMessage<A>();
        }
        CheckNested();
        const auto offset = (TryFinishMessage<A, Vs>() | ...);
        YAFF_REQUIRE(offset != 0);
        MessageSerializer_.emplace<DummyMessageSerializer>();
        DecrementDepth();
        return offset;
    }

    template <typename S>
    Offset ToDeduplicatedOffset(S& pool, size_t start, bool noDedup = false) {
        const size_t offset = Buf_.RightSize();
        if (noDedup) {
            return ToCheckedOffset(offset);
        }
        const size_t byteSize = Buf_.RightSize() - start;
        auto it = pool.find(ObjectOffset{offset, byteSize});
        if (it != pool.end()) {
            Buf_.RightPop(byteSize);
            return it->Offset;
        }
        const Offset checked = ToCheckedOffset(offset);
        pool.insert({checked, byteSize});
        return checked;
    }

    size_t StartArray(bool nullTerminated = false) {
        IncrementDepth();
        const size_t start = Buf_.RightSize();
        if (nullTerminated) {
            Buf_.RightFill(1);
        }
        return start;
    }

    Offset FinishArray(size_t start, bool noDedup = false) {
        CheckNested();
        DecrementDepth();
        return ToDeduplicatedOffset(ObjectSet_, start, noDedup);
    }

    SerializerType Type_;

    DualBuffer Buf_;
    MessageSerializer MessageSerializer_;
    size_t Depth_;

    MessageLayout ForcedAlternative_;
    ObjectOffsetSet ObjectSet_;

    bool Finished_;
};

template <typename M, typename... As>
inline DetachedBuffer Serialize(Serializer& ys, As&&... as) {
    ys.Reset();
    ys.Finish(M::Serialize(ys, std::forward<As>(as)...));
    return ys.Release();
}

template <typename M, typename... As>
inline DetachedBuffer Serialize(As&&... as) {
    Serializer ys;
    return Serialize<M>(ys, std::forward<As>(as)...);
}

}  // namespace yaff
