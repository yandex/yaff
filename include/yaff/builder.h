#pragma once

#include <unordered_set>
#include <variant>
#include <vector>

#include "base.h"
#include "buffer.h"
#include "vector.h"

namespace NYaFF::NExp {

class TBuilder;

}

namespace NYaFF {

enum class EBuilderType : int32_t {
    BUILDER_TYPE_DEFAULT = 0,
    BUILDER_TYPE_EXPERIMENTAL = 1,
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

class TBuilder {
public:
    explicit TBuilder(size_t initialSize = 0, bool = false)
        : TBuilder(EBuilderType::BUILDER_TYPE_DEFAULT, initialSize) {
    }

    TBuilder(const TBuilder&) = delete;
    TBuilder& operator=(const TBuilder&) = delete;

    TBuilder(TBuilder&& other) = delete;
    TBuilder& operator=(TBuilder&& other) = delete;

    ~TBuilder() = default;

    void EnforceDynamicAlternative(EMessageLayout layout) noexcept {
        ForcedAlternative_ = layout;
    }

    EMessageLayout GetForcedDynamicAlternative() const noexcept {
        return ForcedAlternative_;
    }

    void Reset() noexcept {
        Clear();
        Buf_.Reset();
    }

    void Clear() noexcept {
        Buf_.Clear();
        MessageBuilder_.emplace<TDummyMessageBuilder>();
        Depth_ = 0;
        Finished_ = false;
        ObjectSet_.clear();
    }

    template <typename T>
    void AddField(TFieldId fieldId, T value, T def) {
        AddFieldDispatch<T>(fieldId, value, def);
    }

    template <typename T>
    void AddField(TFieldId fieldId, TInternalOffset<T> offset) {
        AddFieldDispatch<T>(fieldId, offset);
    }

    template <typename M>
    void StartFixedMessage() {
        // Nesting is possible for this call, but not another message, since no TFieldOffset is set.
        YAFF_REQUIRE(std::holds_alternative<TDummyMessageBuilder>(MessageBuilder_));
        Buf_.RightFill(M::LIMIT);
        StartMessageDispatch<TFixedMessageBuilder>(Buf_, Buf_.RightSize(), &M::ResolveField);
    }

    TOffset FinishFixedMessage() {
        return FinishMessageDispatch<TFixedMessageBuilder>();
    }

    template <typename M>
    void StartFlatMessage(bool implicit = false) {
        CheckNotNested();
        StartMessageDispatch<TFlatMessageBuilder>(Buf_, implicit, &M::ResolveField);
    }

    TOffset FinishFlatMessage() {
        return FinishMessageDispatch<TFlatMessageBuilder>();
    }

    void StartSparseMessage(bool implicit = false) {
        CheckNotNested();
        StartMessageDispatch<TSparseMessageBuilder>(Buf_, implicit);
    }

    TOffset FinishSparseMessage() {
        return FinishMessageDispatch<TSparseMessageBuilder>();
    }

    template <typename T>
    TInternalOffset<TVector<T>> CreateVector(const T* data, size_t len) {
        CheckNotFinished();
        CheckNotNested();
        CheckLength(len);
        if (len == 0) {
            return 0;
        }
        const size_t start = StartVector();
        Buf_.RightPush(reinterpret_cast<const std::byte*>(data), len * sizeof(T));
        Buf_.RightPushSmall<uint32_t>(len);
        return TInternalOffset<TVector<T>>(FinishVector(start));
    }

    template <typename T>
    TInternalOffset<TVector<TInternalOffset<T>>> CreateVector(const TInternalOffset<T>* data, size_t len) {
        CheckNotFinished();
        CheckNotNested();
        CheckLength(len);
        if (len == 0) {
            return 0;
        }
        const size_t start = StartVector();
        const size_t objectStart = Buf_.RightSize() + len * sizeof(TOffset);
        size_t idx = len;
        while (idx > 0) {
            const TInternalOffset<T> offset = data[--idx];
            YAFF_REQUIRE(objectStart >= offset.O);
            Buf_.RightPushSmall<TOffset>(!offset.IsNull() ? ToCheckedOffset(objectStart - offset.O) : offset.O);
        }
        Buf_.RightPushSmall<uint32_t>(len);
        // Disable deduplication for offset vectors because it breaks the relational offset logic.
        return TInternalOffset<TVector<TInternalOffset<T>>>(FinishVector(start, /* noDedup */ true));
    }

    template <typename T>
    TInternalOffset<TVector<T>> CreateVector(const std::vector<T>& vec) {
        return CreateVector(vec.data(), vec.size());
    }

    // May be implemented using a bit-set.
    TInternalOffset<TVector<bool>> CreateVector(const std::vector<bool>& vec) {
        CheckNotFinished();
        CheckNotNested();
        CheckLength(vec.size());
        if (vec.size() == 0) {
            return 0;
        }
        const size_t start = StartVector();
        size_t idx = vec.size();
        while (idx > 0) {
            Buf_.RightPushSmall<uint8_t>(vec[--idx]);
        }
        Buf_.RightPushSmall<uint32_t>(vec.size());
        return TInternalOffset<TVector<bool>>(FinishVector(start));
    }

    template <typename T, typename F>
        requires CElementGenerator<T, F>
    TInternalOffset<TVector<T>> CreateVector(size_t len, F&& gen) {
        if (len == 0) {
            return 0;
        }
        std::vector<T> elements;
        elements.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            elements.emplace_back(gen(i));
        }
        return CreateVector(elements);
    }

    // TODO: (gavrilovsky): move this function to private section;
    template <typename T, typename F>
        requires CElementProducer<T, F>
    TInternalOffset<TVector<T>> CreateVector(F&& produce) {
        // N.B.: This function cannot produce empty vectors, so the length must be checked on caller side.
        CheckNotFinished();
        CheckNotNested();
        bool next = true;
        size_t len = 0;
        size_t elementSize = 0;
        const size_t start = StartVector();
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
        // TODO: (gavrilovsky): support better interface to enable deduplication where it possible.
        return TInternalOffset<TVector<T>>(FinishVector(start, /* noDedup */ true));
    }

    template <typename T, typename F>
        requires CElementDeferrer<T, size_t, F>
    TInternalOffset<TVector<T>> CreateVector(size_t len, F&& deferrer) {
        if (len == 0) {
            return 0;
        }
        std::vector<std::invoke_result_t<F, size_t>> producers;
        producers.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            producers.emplace_back(deferrer(i));
        }
        return CreateVector<T>([&](size_t i) {
            YAFF_REQUIRE(i + 1 <= producers.size());
            return std::make_pair(static_cast<T>(producers[producers.size() - i - 1]()), i + 1 < producers.size());
        });
    }

    template <typename T, typename U, typename F>
        requires CElementDeferrer<T, U, F>
    TInternalOffset<TVector<T>> CreateVector(const std::vector<U>& args, F&& deferrer) {
        if (args.size() == 0) {
            return 0;
        }
        std::vector<std::invoke_result_t<F, const U&>> producers;
        producers.reserve(args.size());
        for (const auto& arg : args) {
            producers.emplace_back(deferrer(arg));
        }
        return CreateVector<T>([&](size_t i) {
            YAFF_REQUIRE(i + 1 <= producers.size());
            return std::make_pair(static_cast<T>(producers[producers.size() - i - 1]()), i + 1 < producers.size());
        });
    }

    template <std::convertible_to<std::string_view> T>
    TInternalOffset<TString> CreateString(const T& str) {
        const auto sv = static_cast<std::string_view>(str);

        CheckNotFinished();
        CheckNotNested();
        CheckLength(sv.size());
        const size_t start = StartVector(/*nullTerminated*/ true);
        if (sv.size() > 0) {
            Buf_.RightPush(sv.data(), sv.size());
        }
        Buf_.RightPushSmall<uint32_t>(sv.size());
        return TInternalOffset<TString>(FinishVector(start));
    }

    // Incorrect use of this version of Finish function may result in UB.
    // Use this version of function only if you know exactly what you're doing.
    void Finish() {
        CheckNotFinished();
        CheckNotNested();
        Buf_.LeftClear();
        Finished_ = true;
    }

    template <typename T>
    void Finish(TInternalOffset<T> root) {
        CheckNotFinished();
        CheckNotNested();
        YAFF_REQUIRE(!root.IsNull());
        Buf_.LeftClear();
        YAFF_REQUIRE(Buf_.RightSize() + sizeof(TOffset) >= root.O);
        Buf_.RightPushSmall<TOffset>(ToCheckedOffset((Buf_.RightSize() + sizeof(TOffset)) - root.O));
        Finished_ = true;
    }

    TDetachedSegment Release() {
        CheckFinished();
        auto buffer = Buf_.RightDetach();
        Clear();
        return buffer;
    }

    char* GetBufferPointer() const {
        CheckFinished();
        return Buf_.RightData();
    }

    size_t GetSize() const {
        CheckFinished();
        return Buf_.RightSize();
    }

private:
    friend class NYaFF::NExp::TBuilder;

    struct TObjectOffset {
        // It is possible that the message size will exceed the allowed MAX_OFFSET,
        // but due to deduplication it will return to normal.
        // To handle this case, Offset uses size_t to store offsets.
        size_t Offset = 0;
        size_t ByteSize = 0;
    };

    struct TObjectOffsetHash {
        inline size_t operator()(const TObjectOffset& offset) const noexcept {
            return std::hash<std::string_view>{}({Buf.RightDataAt(offset.Offset), offset.ByteSize});
        }
        TDualBuffer& Buf;
    };

    struct TObjectOffsetEqual {
        inline bool operator()(const TObjectOffset& lhs, const TObjectOffset& rhs) const noexcept {
            return lhs.ByteSize == rhs.ByteSize &&
                   memcmp(Buf.RightDataAt(lhs.Offset), Buf.RightDataAt(rhs.Offset), lhs.ByteSize) == 0;
        }
        TDualBuffer& Buf;
    };

    using TObjectOffsetSet = std::unordered_set<TObjectOffset, TObjectOffsetHash, TObjectOffsetEqual>;

    struct TDummyMessageBuilder {
        template <typename T, typename... Ps>
        void AddField(TFieldId, Ps&&...) {
            YAFF_THROW("impossible control flow");
        }
        TOffset Finish() && {
            YAFF_THROW("impossible control flow");
        }
    };

    struct TFixedMessageBuilder {
        TFixedMessageBuilder(TDualBuffer& buffer, size_t loc, TFieldResolverFunc resolver)
            : Buf(buffer), Loc(loc), ResolverFunc(std::move(resolver)) {
        }

        TFixedMessageBuilder(const TFixedMessageBuilder&) = delete;
        TFixedMessageBuilder& operator=(const TFixedMessageBuilder&) = delete;

        TFixedMessageBuilder(TFixedMessageBuilder&& other) = delete;
        TFixedMessageBuilder& operator=(TFixedMessageBuilder&& other) = delete;

        template <typename T>
        void AddField(TFieldId id, T value, T def) {
            WriteField<T>(id, XorDef(value, def));
        }

        template <typename T>
        void AddField(TFieldId id, TInternalOffset<T> offset) {
            if (offset.IsNull()) {
                return;
            }
            YAFF_REQUIRE(Loc >= offset.O);
            WriteField<TOffset>(id, ToCheckedOffset(Loc - offset.O));
        }

        TOffset Finish() && {
            return ToCheckedOffset(Loc);
        }

        template <typename T>
        void WriteField(TFieldId id, T value) {
            const TFieldOffset fieldOffset = ResolverFunc(id);
            YAFF_REQUIRE(Loc >= fieldOffset);
            WriteValue<T>(Buf.RightDataAt(Loc - fieldOffset), value);
        }

        TDualBuffer& Buf;
        size_t Loc;
        TFieldResolverFunc ResolverFunc;
    };

    struct TFlatMessageBuilder {
        inline static constexpr size_t TYPED_LIMIT_SIZE = sizeof(TFieldId);

        static constexpr size_t CalculatePresenceSize(const TFieldId maxId) {
            return (maxId + 0x7) >> 0x3;
        }

        static void SetPresence(char* maskStart, TFieldId id) {
            maskStart[-((id - 0x1) >> 0x3) - 0x1] |= (static_cast<char>(0x1) << ((id - 0x1) & 0x7));
        }

        TFlatMessageBuilder(TDualBuffer& buffer, bool implicit, TFieldResolverFunc resolver)
            : Buf(buffer),
              Start(Buf.RightSize()),
              End(0),
              MaxId(0),
              PrevOffset(0),
              EnableExplicit(!implicit),
              NeedExplicit(false),
              ResolverFunc(std::move(resolver)) {
        }

        TFlatMessageBuilder(const TFlatMessageBuilder&) = delete;
        TFlatMessageBuilder& operator=(const TFlatMessageBuilder&) = delete;

        TFlatMessageBuilder(TFlatMessageBuilder&& other) = delete;
        TFlatMessageBuilder& operator=(TFlatMessageBuilder&& other) = delete;

        template <typename T>
        void AddField(const TFieldId id, const T value, const T def) {
            if (!EnableExplicit && IsEqual(value, def)) {
                return;
            }
            const TFieldOffset offset = ResolverFunc(id);
            TrackField<T>(id, offset, value, def);
            WriteField<T>(offset, XorDef(value, def));
        }

        template <typename T>
        void AddField(const TFieldId id, const TInternalOffset<T> value) {
            if (value.IsNull()) {
                return;
            }
            const TFieldOffset offset = ResolverFunc(id);
            TrackField<TOffset>(id, offset, value.O, 0);
            YAFF_REQUIRE(End >= value.O);
            WriteField<TOffset>(offset, ToCheckedOffset(End - value.O));
        }

        TOffset Finish() && {
            YAFF_REQUIRE(MaxId < 0x2000);

            if (IsEmpty()) {
                YAFF_REQUIRE(Buf.RightSize() == Start);
                Buf.RightPushSmall<TFieldId>(0x8000);
                return ToCheckedOffset(Buf.RightSize());
            }

            if (EnableExplicit && !NeedExplicit) {
                YAFF_REQUIRE(Buf.RightSize() >= End);
                Buf.RightPop(Buf.RightSize() - End);
            }

            const TFieldId typedLimit = ((MaxId << 0x2) | (0x8000 | (EnableExplicit && NeedExplicit)));
            WriteValue<TFieldId>(Buf.RightDataAt(End), typedLimit);

            return ToCheckedOffset(End);
        }

        template <typename T>
        void TrackField(const TFieldId id, const TFieldOffset offset, const T val, const T def) {
            YAFF_REQUIRE(id > 0);

            if (IsEmpty()) {
                YAFF_REQUIRE(Buf.RightSize() == Start);

                const size_t dataSize = TYPED_LIMIT_SIZE + offset + sizeof(T);
                const size_t presenceSize = EnableExplicit * CalculatePresenceSize(id);
                Buf.RightFill(dataSize + presenceSize);

                End = Start + dataSize;
                MaxId = id + 1;
            } else {
                // Expects fields to be written only once and only in the reverse order of the declaration in the
                // schema.
                YAFF_REQUIRE(PrevOffset >= offset + sizeof(T));
            }
            PrevOffset = offset;

            NeedExplicit |= IsEqual<T>(val, def);
            if (EnableExplicit) {
                SetPresence(Buf.RightDataAt(End), id);
            }
        }

        template <typename T>
        void WriteField(const TFieldOffset offset, T value) {
            YAFF_REQUIRE(End >= offset + TYPED_LIMIT_SIZE);
            WriteValue<T>(Buf.RightDataAt(End - offset - TYPED_LIMIT_SIZE), value);
        }

        bool IsEmpty() const {
            return MaxId == 0;
        }

        TDualBuffer& Buf;
        size_t Start;
        size_t End;

        TFieldId MaxId;
        TFieldOffset PrevOffset;

        bool EnableExplicit;
        bool NeedExplicit;

        TFieldResolverFunc ResolverFunc;
    };

    struct TSparseMessageBuilder {
        inline static constexpr TFieldId TINY_OFFSET_MAX_ID = 0x20;
        inline static constexpr TFieldOffset SPARSE_META_OFFSET = sizeof(TFieldId);

        static constexpr size_t CalculateMetaSize(const TFieldId maxId) {
            return (maxId - 0x1) + (maxId > TINY_OFFSET_MAX_ID ? maxId - TINY_OFFSET_MAX_ID : 0x0);
        }

        YAFF_LAYOUT_BEGIN(TField) {
            TOffset Offset;
            TFieldId Id;
            bool IsScalar;
        };
        YAFF_LAYOUT_END

        YAFF_LAYOUT_BEGIN(TMeta) {
            TOffset Offset;
            TFieldId MaxId;
        };
        YAFF_LAYOUT_END

        TSparseMessageBuilder(TDualBuffer& buffer, bool implicit)
            : Buf(buffer),
              LeftStart(Buf.LeftSize()),
              RightStart(Buf.RightSize()),
              FieldsAdded(0),
              PrevId(0),
              MaxId(0),
              Implicit(implicit) {
        }

        TSparseMessageBuilder(const TSparseMessageBuilder&) = delete;
        TSparseMessageBuilder& operator=(const TSparseMessageBuilder&) = delete;

        TSparseMessageBuilder(TSparseMessageBuilder&& other) = delete;
        TSparseMessageBuilder& operator=(TSparseMessageBuilder&& other) = delete;

        template <typename T>
        void AddField(TFieldId id, T value, T def) {
            if (Implicit && IsEqual(value, def)) {
                return;
            }
            Buf.RightPushSmall<T>(value);
            TrackField(id, Buf.RightSize());
        }

        template <typename T>
        void AddField(TFieldId id, TInternalOffset<T> offset) {
            if (offset.IsNull()) {
                return;
            }
            Buf.RightPushSmall<TOffset>(offset.O);
            TrackField(id, Buf.RightSize(), /*isScalar*/ false);
        }

        TOffset Finish() && {
            YAFF_REQUIRE(MaxId < 0x2000);

            if (IsEmpty()) {
                YAFF_REQUIRE(Buf.RightSize() == RightStart);
                Buf.RightPushSmall<TFieldId>(0x1);
                return ToCheckedOffset(Buf.RightSize());
            }

            Buf.RightPushSmall<TSignedOffset>(0);
            Buf.RightPushSmall<TFieldId>((MaxId << 0x2) | 0x1);
            const size_t msgStart = Buf.RightSize();

            const size_t metaSize = CalculateMetaSize(MaxId);
            Buf.RightFill(metaSize);

            const size_t metaStart = Buf.RightSize();
            for (uint16_t i = 0; i < FieldsAdded; ++i) {
                const auto* field = ReadLayout<TField>(Buf.LeftDataAt(LeftStart + i * sizeof(TField)));

                if (field->Id < TINY_OFFSET_MAX_ID) {
                    YAFF_REQUIRE(msgStart >= field->Offset &&
                                 msgStart - field->Offset <= std::numeric_limits<uint8_t>::max());
                    WriteValue<uint8_t>(Buf.RightDataAt(metaStart - (field->Id - 0x1)), msgStart - field->Offset);
                } else {
                    YAFF_REQUIRE(msgStart >= field->Offset &&
                                 msgStart - field->Offset <= std::numeric_limits<uint16_t>::max());
                    WriteValue<uint16_t>(Buf.RightDataAt(metaStart - (field->Id << 0x1) + (TINY_OFFSET_MAX_ID + 1)),
                                         msgStart - field->Offset);
                }

                if (!field->IsScalar) {
                    char* loc = Buf.RightDataAt(field->Offset);
                    const TOffset offset = ReadValue<TOffset>(loc);
                    YAFF_REQUIRE(msgStart >= offset);
                    WriteValue<TOffset>(loc, ToCheckedOffset(msgStart - offset));
                }
            }
            Buf.LeftPop(FieldsAdded * sizeof(TField));

            TOffset metaOffset = 0;  // Offset of meta can not be zero;
            for (size_t i = 0; i < Buf.LeftSize(); i += sizeof(TMeta)) {
                const auto* candidate = ReadLayout<TMeta>(Buf.LeftDataAt(i));
                const size_t candidateSize = CalculateMetaSize(candidate->MaxId);
                if (candidateSize != metaSize) {
                    continue;
                }

                const char* candidateMeta = Buf.RightDataAt(candidate->Offset);
                const char* meta = Buf.RightDataAt(metaStart);
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
            WriteValue<TSignedOffset>(Buf.RightDataAt(msgStart - SPARSE_META_OFFSET),
                                      ToCheckedSignedOffset(static_cast<int64_t>(msgStart) - metaOffset));

            return ToCheckedOffset(msgStart);
        }

        void TrackField(TFieldId id, size_t offset, bool isScalar = true) {
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

        TDualBuffer& Buf;
        size_t LeftStart;
        size_t RightStart;

        uint16_t FieldsAdded;
        TFieldId PrevId;
        TFieldId MaxId;

        bool Implicit;
    };

    using TMessageBuilder =
        std::variant<TDummyMessageBuilder, TFixedMessageBuilder, TFlatMessageBuilder, TSparseMessageBuilder>;

    explicit TBuilder(EBuilderType type, size_t initialSize = 0, bool = false)
        : Type_(type),
          Buf_(initialSize),
          MessageBuilder_(),
          Depth_(0),
          ForcedAlternative_(EMessageLayout::MESSAGE_LAYOUT_FLAT),
          ObjectSet_(8, TObjectOffsetHash(Buf_), TObjectOffsetEqual(Buf_)),
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
    void AddFieldDispatch(TFieldId fieldId, Ps&&... params) {
        CheckNotFinished();
        CheckNested();
        std::visit([&](auto& b) { b.template AddField<T>(fieldId, std::forward<Ps>(params)...); }, MessageBuilder_);
    }

    template <typename T, typename... Ps>
    void StartMessageDispatch(Ps&&... params) {
        CheckNotFinished();
        IncrementDepth();
        MessageBuilder_.emplace<T>(std::forward<Ps>(params)...);
    }

    template <typename T>
    TOffset FinishMessageDispatch() {
        CheckNotFinished();
        CheckNested();
        DecrementDepth();
        YAFF_REQUIRE(std::holds_alternative<T>(MessageBuilder_));
        const auto offset = std::visit([](auto&& b) { return std::move(b).Finish(); }, std::move(MessageBuilder_));
        MessageBuilder_.emplace<TDummyMessageBuilder>();
        return offset;
    }

    template <typename S>
    TOffset ToDeduplicatedOffset(S& pool, size_t start, bool noDedup = false) {
        const size_t offset = Buf_.RightSize();
        if (noDedup) {
            return ToCheckedOffset(offset);
        }
        const size_t byteSize = Buf_.RightSize() - start;
        auto it = pool.find(TObjectOffset{offset, byteSize});
        if (it != pool.end()) {
            Buf_.RightPop(byteSize);
            return ToCheckedOffset(it->Offset);
        }
        const TOffset checked = ToCheckedOffset(offset);
        pool.insert({checked, byteSize});
        return checked;
    }

    size_t StartVector(bool nullTerminated = false) {
        IncrementDepth();
        const size_t start = Buf_.RightSize();
        if (nullTerminated) {
            Buf_.RightFill(1);
        }
        return start;
    }

    TOffset FinishVector(size_t start, bool noDedup = false) {
        DecrementDepth();
        return ToDeduplicatedOffset(ObjectSet_, start, noDedup);
    }

    EBuilderType Type_;

    TDualBuffer Buf_;
    TMessageBuilder MessageBuilder_;
    size_t Depth_;

    EMessageLayout ForcedAlternative_;
    TObjectOffsetSet ObjectSet_;

    bool Finished_;
};

}  // namespace NYaFF
