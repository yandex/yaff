#include <gtest/gtest.h>
#include <yaff/any_message.h>
#include <yaff/reflect.h>
#include <yaff/serializer.h>

#include <array>

using namespace yaff;
using namespace yaff::reflect;

// Dense writer schema, ids 1..5: uint64, uint32, uint64, bool, uint64.
struct ExplicitMetaV1 {
    inline static constexpr std::array<FieldOffset, 6> FLAT_OFFSETS = {0, 8, 12, 20, 21, 29};
    inline static constexpr std::array<FieldId, 0> DELETED_IDS = {};
    inline static constexpr std::array<bool, 5> STATIC_FLAGS = {1, 1, 1, 1, 1};
};

// Dense writer schema, ids 1..4: bool, uint32, uint32, uint64.
struct ImplicitMetaV1 {
    inline static constexpr std::array<FieldOffset, 5> FLAT_OFFSETS = {0, 1, 5, 9, 17};
    inline static constexpr std::array<FieldId, 0> DELETED_IDS = {};
    inline static constexpr std::array<bool, 4> STATIC_FLAGS = {1, 1, 1, 1};
};

const AnyMessage* AsAnyMessage(const Serializer& ys) {
    const void* data = ys.Data();
    const void* msg = ResolveOffset<void>(data, ReadValue<Offset>(data));
    return ReadLayout<AnyMessage>(msg);
}

// Reader removed ids 2 and 3, added id 6: the old explicit flat data still stores 2 and 3,
// so AnyMessage must skip over them through the resolver's deleted ids.
TEST(AnyMessage, ExplicitFlatGapCorrection) {
    Serializer ys;
    ys.StartFlatMessage<ExplicitMetaV1>(/*implicit*/ false, /*sized*/ true);
    ys.AddField<uint64_t>(5, 10, 10);
    ys.AddField<bool>(4, true, false);
    ys.AddField<uint64_t>(3, 15, 10);
    ys.AddField<uint32_t>(2, 0, 0);
    ys.AddField<uint64_t>(1, 20, 0);
    ys.Finish(InternalOffset<>{ys.FinishFlatMessage()});

    const FieldDescriptor fields[] = {
        {.Id = 1},
        {.Id = 4},
        {.Id = 5},
        {.Id = 6},
    };
    // Dense offsets indexed by id-1 over ids 1..6 (gaps 2,3 carry the next present offset);
    // trailing entry is the message inline size. Mirrors the generated MetaType table.
    static constexpr FieldOffset flatOffsets[] = {0, 8, 8, 8, 9, 17, 21};
    static constexpr FieldId deleted[] = {2, 3};
    static constexpr MetaDescriptor meta = {
        .FlatOffsetCount = 6,
        .FlatOffsets = flatOffsets,
        .DeletedCount = 2,
        .DeletedIds = deleted,
    };
    const MessageDescriptor desc{
        .Meta = &meta,
        .Layout = MessageLayout::MESSAGE_LAYOUT_DYNAMIC,
        .FieldCount = 4,
        .Fields = fields,
    };
    const auto resolver = MakeFieldResolver(&desc);
    const auto layout = MessageLayout::MESSAGE_LAYOUT_DYNAMIC;
    const auto* msg = AsAnyMessage(ys);

    EXPECT_TRUE(msg->ReadPresence<uint64_t>(layout, 1, &resolver));
    EXPECT_EQ(msg->ReadValue<uint64_t>(layout, 1, 0, &resolver), 20ULL);

    EXPECT_TRUE(msg->ReadPresence<uint64_t>(layout, 4, &resolver));
    EXPECT_EQ(msg->ReadValue<bool>(layout, 4, false, &resolver), true);

    EXPECT_TRUE(msg->ReadPresence<uint64_t>(layout, 5, &resolver));
    EXPECT_EQ(msg->ReadValue<uint64_t>(layout, 5, 10, &resolver), 10ULL);

    EXPECT_FALSE(msg->ReadPresence<uint32_t>(layout, 6, &resolver));
    EXPECT_EQ(msg->ReadValue<uint32_t>(layout, 6, 1337, &resolver), 1337U);
}

// Reader removed leading id 1: the old implicit flat data still stores it, so the offsets of
// ids 2 and 3 shift by its inline size.
TEST(AnyMessage, ImplicitFlatGapCorrection) {
    Serializer ys;
    ys.StartFlatMessage<ImplicitMetaV1>(/*implicit*/ true, /*sized*/ true);
    ys.AddField<uint64_t>(4, 10, 0);
    ys.AddField<uint32_t>(3, 8, 8);
    ys.AddField<uint32_t>(2, 1, 2);
    ys.AddField<bool>(1, false, false);
    ys.Finish(InternalOffset<>{ys.FinishFlatMessage()});

    const FieldDescriptor fields[] = {
        {.Id = 2},
        {.Id = 3},
    };
    // Dense offsets indexed by id-1 over ids 1..3 (gap 1 carries the next present offset);
    // trailing entry is the message inline size. Mirrors the generated MetaType table.
    static constexpr FieldOffset flatOffsets[] = {0, 0, 4, 8};
    static constexpr FieldId deleted[] = {1};
    static constexpr MetaDescriptor meta = {
        .FlatOffsetCount = 3,
        .FlatOffsets = flatOffsets,
        .DeletedCount = 1,
        .DeletedIds = deleted,
    };
    const MessageDescriptor desc{
        .Meta = &meta,
        .Layout = MessageLayout::MESSAGE_LAYOUT_DYNAMIC,
        .FieldCount = 2,
        .Fields = fields,
    };
    const auto resolver = MakeFieldResolver(&desc);
    const auto layout = MessageLayout::MESSAGE_LAYOUT_DYNAMIC;
    const auto* msg = AsAnyMessage(ys);

    EXPECT_TRUE(msg->ReadPresence<uint64_t>(layout, 2, &resolver));
    EXPECT_EQ(msg->ReadValue<uint32_t>(layout, 2, 1, &resolver), 2U);

    EXPECT_TRUE(msg->ReadPresence<uint64_t>(layout, 3, &resolver));
    EXPECT_EQ(msg->ReadValue<uint32_t>(layout, 3, 8, &resolver), 8U);
}
