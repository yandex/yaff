#pragma once

#include <array>
#include <cstddef>
#include <unordered_set>
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

    template <typename T>
    void AddField(FieldId fieldId, T value, T def) {
        AddFieldDispatch<T>(fieldId, value, def);
    }

    template <typename T>
    void AddField(FieldId fieldId, InternalOffset<T> offset) {
        AddFieldDispatch<T>(fieldId, offset);
    }

    template <typename M>
    void StartFixedMessage() {
        // Nesting is possible for this call, but not another message, since no FieldOffset is set.
        YAFF_REQUIRE(std::holds_alternative<DummyMessageSerializer>(MessageSerializer_));
        Buf_.RightFill(M::LIMIT);
        StartMessageDispatch<FixedMessageSerializer>(Buf_, Buf_.RightSize(), M::FLAT_OFFSETS);
    }

    Offset FinishFixedMessage() {
        return FinishMessageDispatch<FixedMessageSerializer>();
    }

    template <typename M>
        requires(M::DELETED_IDS.empty())
    void StartFlatMessage(bool implicit = false, bool sized = false) {
        CheckNotNested();
        StartMessageDispatch<FlatMessageSerializer>(Buf_, sized, implicit, M::FLAT_OFFSETS);
    }

    Offset FinishFlatMessage() {
        return FinishMessageDispatch<FlatMessageSerializer>();
    }

    void StartSparseMessage(bool implicit = false) {
        CheckNotNested();
        StartMessageDispatch<SparseMessageSerializer>(Buf_, implicit);
    }

    Offset FinishSparseMessage() {
        return FinishMessageDispatch<SparseMessageSerializer>();
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
        const size_t objectStart = Buf_.RightSize() + len * sizeof(Offset);
        size_t idx = len;
        while (idx > 0) {
            const InternalOffset<T> offset = data[--idx];
            YAFF_REQUIRE(objectStart >= offset.O);
            Buf_.RightPushSmall<Offset>(!offset.IsNull() ? ToCheckedOffset(objectStart - offset.O) : offset.O);
        }
        Buf_.RightPushSmall<uint32_t>(len);
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
        size_t idx = vec.size();
        while (idx > 0) {
            Buf_.RightPushSmall<uint8_t>(vec[--idx]);
        }
        Buf_.RightPushSmall<uint32_t>(vec.size());
        return InternalOffset<Array<bool>>(FinishArray(start));
    }

    template <typename T, typename F>
        requires CElementGenerator<T, F>
    InternalOffset<Array<T>> SerializeArray(size_t len, F&& gen) {
        if (len == 0) {
            return 0;
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
            YAFF_REQUIRE(i + 1 <= producers.size());
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
            YAFF_REQUIRE(i + 1 <= producers.size());
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
    void Finish(InternalOffset<T> root) {
        CheckNotFinished();
        CheckNotNested();
        YAFF_REQUIRE(!root.IsNull());
        Buf_.LeftClear();
        YAFF_REQUIRE(Buf_.RightSize() + sizeof(Offset) >= root.O);
        Buf_.RightPushSmall<Offset>(ToCheckedOffset((Buf_.RightSize() + sizeof(Offset)) - root.O));
        Finished_ = true;
    }

    // N.B.: Incorrect use of this version of Finish function may result in UB.
    // Use this version of function only if you know exactly what you're doing.
    void FinishRootless() {
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
                   memcmp(Buf.RightDataAt(lhs.Offset), Buf.RightDataAt(rhs.Offset), lhs.ByteSize) == 0;
        }
        DualBuffer& Buf;
    };

    using ObjectOffsetSet = std::unordered_set<ObjectOffset, ObjectOffsetHash, ObjectOffsetEqual>;

    struct DummyMessageSerializer {
        template <typename T, typename... Ps>
        void AddField(FieldId, Ps&&...) {
            YAFF_THROW("impossible control flow");
        }
        Offset Finish() && {
            YAFF_THROW("impossible control flow");
        }
    };

    struct FixedMessageSerializer {
        FixedMessageSerializer(DualBuffer& buffer, size_t loc, OffsetsView offsets)
            : Buf(buffer), Loc(loc), Offsets(offsets) {
        }

        FixedMessageSerializer(const FixedMessageSerializer&) = delete;
        FixedMessageSerializer& operator=(const FixedMessageSerializer&) = delete;

        FixedMessageSerializer(FixedMessageSerializer&& other) = delete;
        FixedMessageSerializer& operator=(FixedMessageSerializer&& other) = delete;

        template <typename T>
        void AddField(FieldId id, T value, T def) {
            WriteField<T>(id, XorDef(value, def));
        }

        template <typename T>
        void AddField(FieldId id, InternalOffset<T> offset) {
            if (offset.IsNull()) {
                return;
            }
            YAFF_REQUIRE(Loc >= offset.O);
            WriteField<Offset>(id, ToCheckedOffset(Loc - offset.O));
        }

        Offset Finish() && {
            return ToCheckedOffset(Loc);
        }

        template <typename T>
        void WriteField(FieldId id, T value) {
            const FieldOffset fieldOffset = Offsets.Data[id - 1];
            YAFF_REQUIRE(Loc >= fieldOffset);
            WriteValue<T>(Buf.RightDataAt(Loc - fieldOffset), value);
        }

        DualBuffer& Buf;
        size_t Loc;
        OffsetsView Offsets;
    };

    struct FlatMessageSerializer {
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

        static void SetPresence(std::byte* maskStart, const FieldId id, const bool sized) {
            const size_t index = (id - 1) * CalculateFieldMetaSize(true, sized);
            *(maskStart - (index >> 3) - 1) |= (static_cast<std::byte>(1) << (index & 7));
        }

        static void SetSize(std::byte* maskStart, const FieldId id, const size_t size, const bool expl) {
            const size_t corr = CalculateCorrection(size);
            if (corr == 0) {
                return;
            }
            const size_t index = (id - 1) * CalculateFieldMetaSize(expl, true) + expl;
            *(maskStart - (index >> 3) - 1) |= (static_cast<std::byte>(corr) << (index & 7));
            if ((index & 7) == 7) {
                *(maskStart - (index >> 3) - 2) |= (static_cast<std::byte>(corr) >> 1);
            }
        }

        FlatMessageSerializer(DualBuffer& buffer, bool sized, bool implicit, OffsetsView offsets)
            : Buf(buffer),
              Start(Buf.RightSize()),
              End(0),
              MaxId(0),
              PrevOffset(0),
              EnableSizes(sized),
              EnableExplicit(!implicit),
              NeedExplicit(false),
              Offsets(offsets) {
        }

        FlatMessageSerializer(const FlatMessageSerializer&) = delete;
        FlatMessageSerializer& operator=(const FlatMessageSerializer&) = delete;

        FlatMessageSerializer(FlatMessageSerializer&& other) = delete;
        FlatMessageSerializer& operator=(FlatMessageSerializer&& other) = delete;

        template <typename T>
        void AddField(const FieldId id, const T value, const T def) {
            if (!EnableExplicit && IsEqual(value, def)) {
                return;
            }
            const FieldOffset offset = Offsets.Data[id - 1];
            TrackField<T>(id, offset, value, def);
            WriteField<T>(offset, XorDef(value, def));
        }

        template <typename T>
        void AddField(const FieldId id, const InternalOffset<T> value) {
            if (value.IsNull()) {
                return;
            }
            const FieldOffset offset = Offsets.Data[id - 1];
            TrackField<Offset>(id, offset, value.O, 0);
            YAFF_REQUIRE(End >= value.O);
            WriteField<Offset>(offset, ToCheckedOffset(End - value.O));
        }

        Offset Finish() && {
            YAFF_REQUIRE(MaxId < 0x2000);
            const bool trulyExplicit = (EnableExplicit && NeedExplicit);

            if (IsEmpty()) {
                YAFF_REQUIRE(Buf.RightSize() == Start);
                Buf.RightPushSmall<FieldId>(0x8000);
                return ToCheckedOffset(Buf.RightSize());
            }

            if (EnableExplicit && !trulyExplicit) {
                YAFF_REQUIRE(Buf.RightSize() >= End);
                Buf.RightPop(Buf.RightSize() - End);
            }

            if (EnableSizes) {
                // N.B.: This line ensures that the buffer is allocated
                // when we have not allocated or discarded the meta information.
                Buf.RightFill(!trulyExplicit * CalculateMetaSize(MaxId - 1, false, true));
                for (size_t i = 0; i < MaxId - 1; ++i) {
                    const size_t size = Offsets.Data[i + 1] - Offsets.Data[i];
                    SetSize(Buf.RightDataAt(End), i + 1, size, trulyExplicit);
                }
            }

            const FieldId typedLimit = ((MaxId << 2) | (0x8000 | (EnableSizes << 1) | trulyExplicit));
            WriteValue<FieldId>(Buf.RightDataAt(End), typedLimit);

            return ToCheckedOffset(End);
        }

        template <typename T>
        void TrackField(const FieldId id, const FieldOffset offset, const T val, const T def) {
            YAFF_REQUIRE(id > 0);

            if (IsEmpty()) {
                YAFF_REQUIRE(Buf.RightSize() == Start);

                const size_t dataSize = TYPED_LIMIT_SIZE + offset + sizeof(T);
                const size_t metaSize = EnableExplicit * CalculateMetaSize(id, EnableExplicit, EnableSizes);
                Buf.RightFill(dataSize + metaSize);

                End = Start + dataSize;
                MaxId = id + 1;
            } else {
                // Expects fields to be written only once and only in the reverse order of the declaration in the
                // schema.
                YAFF_REQUIRE(PrevOffset >= offset + sizeof(T));
            }
            PrevOffset = offset;

            NeedExplicit = (NeedExplicit || IsEqual<T>(val, def));
            if (EnableExplicit) {
                SetPresence(Buf.RightDataAt(End), id, EnableSizes);
            }
        }

        template <typename T>
        void WriteField(const FieldOffset offset, T value) {
            YAFF_REQUIRE(End >= offset + TYPED_LIMIT_SIZE);
            WriteValue<T>(Buf.RightDataAt(End - offset - TYPED_LIMIT_SIZE), value);
        }

        bool IsEmpty() const {
            return MaxId == 0;
        }

        DualBuffer& Buf;
        size_t Start;
        size_t End;

        FieldId MaxId;
        FieldOffset PrevOffset;

        bool EnableSizes;
        bool EnableExplicit;
        bool NeedExplicit;

        OffsetsView Offsets;
    };

    struct SparseMessageSerializer {
        inline static constexpr FieldId TINY_OFFSET_MAX_ID = 0x20;
        inline static constexpr FieldOffset SPARSE_META_OFFSET = sizeof(SignedOffset);

        static constexpr size_t CalculateMetaSize(const FieldId maxId) {
            return (maxId - 1) + (maxId > TINY_OFFSET_MAX_ID ? maxId - TINY_OFFSET_MAX_ID : 0);
        }

        YAFF_LAYOUT_BEGIN(TField) {
            Offset Offset;
            FieldId Id;
            bool IsScalar;
        };
        YAFF_LAYOUT_END

        YAFF_LAYOUT_BEGIN(TMeta) {
            Offset Offset;
            FieldId MaxId;
        };
        YAFF_LAYOUT_END

        SparseMessageSerializer(DualBuffer& buffer, bool implicit)
            : Buf(buffer),
              LeftStart(Buf.LeftSize()),
              RightStart(Buf.RightSize()),
              FieldsAdded(0),
              PrevId(0),
              MaxId(0),
              Implicit(implicit) {
        }

        SparseMessageSerializer(const SparseMessageSerializer&) = delete;
        SparseMessageSerializer& operator=(const SparseMessageSerializer&) = delete;

        SparseMessageSerializer(SparseMessageSerializer&& other) = delete;
        SparseMessageSerializer& operator=(SparseMessageSerializer&& other) = delete;

        template <typename T>
        void AddField(FieldId id, T value, T def) {
            if (Implicit && IsEqual(value, def)) {
                return;
            }
            Buf.RightPushSmall<T>(value);
            TrackField(id, Buf.RightSize());
        }

        template <typename T>
        void AddField(FieldId id, InternalOffset<T> offset) {
            if (offset.IsNull()) {
                return;
            }
            Buf.RightPushSmall<Offset>(offset.O);
            TrackField(id, Buf.RightSize(), /*isScalar*/ false);
        }

        Offset Finish() && {
            YAFF_REQUIRE(MaxId < 0x2000);

            if (IsEmpty()) {
                YAFF_REQUIRE(Buf.RightSize() == RightStart);
                Buf.RightPushSmall<FieldId>(2);
                return ToCheckedOffset(Buf.RightSize());
            }

            Buf.RightPushSmall<FieldId>((MaxId << 2) | 3);
            const size_t msgStart = Buf.RightSize();

            const size_t metaSize = CalculateMetaSize(MaxId);
            Buf.RightPushSmall<SignedOffset>(0);
            Buf.RightFill(metaSize);

            const size_t metaStart = Buf.RightSize();
            const size_t metaEnd = Buf.RightSize() - metaSize;
            for (uint16_t i = 0; i < FieldsAdded; ++i) {
                const auto* field = ReadLayout<TField>(Buf.LeftDataAt(LeftStart + i * sizeof(TField)));

                const FieldOffset offset = (msgStart - field->Offset);
                if (field->Id < TINY_OFFSET_MAX_ID) {
                    WriteValue<uint8_t>(Buf.RightDataAt(metaEnd + field->Id), offset);
                } else {
                    WriteValue<uint16_t>(Buf.RightDataAt(metaEnd + (field->Id << 1) - (TINY_OFFSET_MAX_ID - 1)),
                                         offset);
                }

                if (!field->IsScalar) {
                    std::byte* loc = Buf.RightDataAt(field->Offset);
                    const Offset offset = ReadValue<Offset>(loc);
                    YAFF_REQUIRE(msgStart >= offset);
                    WriteValue<Offset>(loc, ToCheckedOffset(msgStart - offset));
                }
            }
            Buf.LeftPop(FieldsAdded * sizeof(TField));

            Offset metaOffset = 0;  // Offset of meta can not be zero;
            for (size_t i = 0; i < Buf.LeftSize(); i += sizeof(TMeta)) {
                const auto* candidate = ReadLayout<TMeta>(Buf.LeftDataAt(i));
                const size_t candidateSize = CalculateMetaSize(candidate->MaxId);
                if (candidateSize != metaSize) {
                    continue;
                }

                const std::byte* candidateMeta = Buf.RightDataAt(candidate->Offset);
                const std::byte* meta = Buf.RightDataAt(metaStart);
                if (memcmp(candidateMeta, meta, metaSize)) {
                    continue;
                }

                metaOffset = candidate->Offset;
                Buf.RightPop(metaSize);
                break;
            }

            // This means we have unique meta, push for next generations;
            if (!metaOffset) {
                metaOffset = ToCheckedOffset(metaStart);
                Buf.LeftPushSmall<TMeta>(TMeta{.Offset = metaOffset, .MaxId = MaxId});
            }

            YAFF_REQUIRE(sizeof(msgStart) < sizeof(int64_t) || msgStart <= std::numeric_limits<int64_t>::max());
            WriteValue<SignedOffset>(Buf.RightDataAt(msgStart + SPARSE_META_OFFSET),
                                     ToCheckedSignedOffset(static_cast<int64_t>(msgStart) - (metaOffset - metaSize)));

            return ToCheckedOffset(msgStart);
        }

        void TrackField(FieldId id, size_t offset, bool isScalar = true) {
            YAFF_REQUIRE(id > 0);
            YAFF_REQUIRE(PrevId == 0 || PrevId > id);

            PrevId = id;
            if (IsEmpty()) {
                MaxId = id + 1;
            }

            TField field{.Offset = ToCheckedOffset(offset), .Id = id, .IsScalar = isScalar};
            Buf.LeftPushSmall<TField>(field);

            YAFF_REQUIRE(FieldsAdded < std::numeric_limits<uint16_t>::max());
            ++FieldsAdded;
        }

        bool IsEmpty() const {
            return MaxId == 0;
        }

        DualBuffer& Buf;
        size_t LeftStart;
        size_t RightStart;

        uint16_t FieldsAdded;
        FieldId PrevId;
        FieldId MaxId;

        bool Implicit;
    };

    using MessageSerializer =
        std::variant<DummyMessageSerializer, FixedMessageSerializer, FlatMessageSerializer, SparseMessageSerializer>;

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
        YAFF_REQUIRE(Depth_ > 0);
        --Depth_;
    }

    template <typename T, typename... Ps>
    void AddFieldDispatch(FieldId fieldId, Ps&&... params) {
        CheckNotFinished();
        CheckNested();
        std::visit([&](auto& b) { b.template AddField<T>(fieldId, std::forward<Ps>(params)...); }, MessageSerializer_);
    }

    template <typename T, typename... Ps>
    void StartMessageDispatch(Ps&&... params) {
        CheckNotFinished();
        IncrementDepth();
        MessageSerializer_.emplace<T>(std::forward<Ps>(params)...);
    }

    template <typename T>
    Offset FinishMessageDispatch() {
        CheckNotFinished();
        CheckNested();
        DecrementDepth();
        YAFF_REQUIRE(std::holds_alternative<T>(MessageSerializer_));
        const auto offset = std::visit([](auto&& b) { return std::move(b).Finish(); }, std::move(MessageSerializer_));
        MessageSerializer_.emplace<DummyMessageSerializer>();
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
            return ToCheckedOffset(it->Offset);
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

}  // namespace yaff
