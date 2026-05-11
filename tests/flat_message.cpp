#include <gtest/gtest.h>
#include <yaff/builder.h>
#include <yaff/message.h>

#include "protoyaff/proto2.yaff.h"
#include "protoyaff/proto3.yaff.h"

using namespace NProtoYaFF::NTestYaFF;

TEST(FlatMessage, NestedMessage) {
    NYaFF::TBuilder yffb;
    const auto nested = CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, -0x10, 0xFFFF, 0x3333);
    yffb.Finish(CreateTStaticFlatMessage(yffb, 0xFF, std::nullopt, nested));

    EXPECT_EQ(yffb.GetSize(), 41ULL);

    const auto* buf = yffb.GetBufferPointer();

    const auto& root = NYaFF::ReadRoot<TStaticFlatMessage>(buf);

    const auto& simpleMessage = root.GetNested();
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
    EXPECT_EQ(root.GetScalar(), 0xFFULL);
    EXPECT_EQ(root.GetOtherScalar(), 0x5U);

    EXPECT_TRUE(root.HasScalar());
    EXPECT_FALSE(root.HasOtherScalar());
    EXPECT_TRUE(root.HasNested());
}

TEST(FlatMessage, ImplicitMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTImplicitMessage(yffb, 0, 1, 0));

    EXPECT_EQ(yffb.GetSize(), 19ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<TImplicitMessage>(buf);

    EXPECT_EQ(root.GetStringField(), "");
    EXPECT_EQ(root.GetSignedField(), 1LL);
    EXPECT_EQ(root.GetUnsignedField(), 0ULL);
}

TEST(FlatDynamicMessage, ImplicitMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, -0x10, 0xFFFF, 0x3333));

    EXPECT_EQ(yffb.GetSize(), 23ULL);

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicMessage, ExplicitMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, -0x10, 0xAF,
                                                                0x3333));  // Sets second field to default explicitly;

    EXPECT_EQ(yffb.GetSize(), 24ULL);  // Allocates 2 more byte for presence and sizes mask;

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicMessage, BadFillOrder) {
    NYaFF::TBuilder yffb;
    yffb.StartFlatMessage<TSimpleMessage::TMetaType>();
    yffb.AddField<int32_t>(TSimpleMessage::ID_SIGNEDFIELD, -0x10, 0);
    EXPECT_THROW(yffb.AddField<uint64_t>(TSimpleMessage::ID_LARGEFIELD, 0x3333, 0x6789), std::runtime_error);
}

TEST(FlatDynamicMessage, SkipFields) {
    NYaFF::TBuilder yffb;
    TSimpleMessageFlatBuilder builder(yffb);
    builder.add_largefield(0x3333);
    builder.add_signedfield(-0x10);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 23ULL);

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicMessage, SkipFieldsFront) {
    NYaFF::TBuilder yffb;
    TSimpleMessageFlatBuilder builder(yffb);
    builder.add_largefield(0x3333);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 23ULL);

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x0);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicMessage, SkipFieldsTail) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, 0x17));

    EXPECT_EQ(yffb.GetSize(), 11ULL);  // Tail size optimization;

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x17);
}

TEST(FlatDynamicMessage, DefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, -0x10, std::nullopt,
                                                                0x1234));  // Sets empty value for small field

    EXPECT_EQ(yffb.GetSize(), 23ULL);

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x1234ULL);
}

TEST(FlatDynamicMessage, TailDefaultValues) {
    NYaFF::TBuilder yffb;
    TSimpleMessageFlatBuilder builder(yffb);
    builder.add_signedfield(-0x10);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 11ULL);  // Does not store empty values in the end of object;

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicMessage, AllDefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb));

    EXPECT_EQ(yffb.GetSize(), 6ULL);  // root offset + flat message typed size;

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x0);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicMessage, NestedMessages) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, 0, 0xFFFF);
    auto nested2 = CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, 0x17);
    auto root = CreateTNestedMessage<TNestedMessageFlatBuilder>(yffb, nested1, 0x1234, nested2);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 41ULL);

    const auto& nestedMessage = NYaFF::ReadRoot<TNestedMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested2().GetSignedField(), 0x17);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(nestedMessage.HasNested1() && nestedMessage.HasNested2());
}

TEST(FlatDynamicMessage, NestedMessageEmpty) {
    NYaFF::TBuilder yffb;
    auto nested = CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, 0, 0xAF, 0x5678);
    auto root = CreateTNestedMessage<TNestedMessageFlatBuilder>(yffb, nested);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 31ULL);

    const auto& nestedMessage = NYaFF::ReadRoot<TNestedMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x5678ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSignedField(), 0x0);
    EXPECT_TRUE(nestedMessage.HasNested1() && !nestedMessage.HasNested2());
}

TEST(FlatDynamicMessage, FloatMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTFloatMessage<TFloatMessageFlatBuilder>(yffb, 1.1234f, std::nullopt));

    EXPECT_EQ(yffb.GetSize(), 11ULL);  // double empty value is not stored;

    const auto& floatMessage = NYaFF::ReadRoot<TFloatMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(floatMessage.GetFloatField(), 1.1234f);
    EXPECT_EQ(floatMessage.GetDoubleField(), 1e-6);
}

TEST(FlatDynamicMessage, UnionMessageEmpty) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTUnionMessage<TUnionMessageFlatBuilder>(yffb, 10));
    EXPECT_EQ(yffb.GetSize(), 11ULL);  // shared values is not stored;

    const auto& unionMessage = NYaFF::ReadRoot<TUnionMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(unionMessage.GetSomeValue(), 10U);
    EXPECT_EQ(unionMessage.Union_case(), TUnionMessage::UnionCase::UNION_NOT_SET);

    EXPECT_EQ(unionMessage.HasNested1(), false);
    EXPECT_EQ(unionMessage.HasStringVec(), false);
    EXPECT_EQ(unionMessage.HasNested2(), false);
    EXPECT_EQ(unionMessage.HasMediumField(), false);
    EXPECT_EQ(unionMessage.HasLargeField(), false);

    EXPECT_EQ(unionMessage.GetNested1().GetLargeField(), 0x6789ULL);
    EXPECT_EQ(unionMessage.GetStringVec().Size(), 0U);
    EXPECT_EQ(unionMessage.GetMediumField(), -1);
}

TEST(FlatDynamicMessage, UnionMessageExplicitPresence) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTUnionMessage<TUnionMessageFlatBuilder>(yffb, 10, 0, 0, 0, -1));  // -1 is default value;

    EXPECT_EQ(yffb.GetSize(), 28ULL);

    const auto& unionMessage = NYaFF::ReadRoot<TUnionMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(unionMessage.GetSomeValue(), 10U);
    EXPECT_EQ(unionMessage.Union_case(), TUnionMessage::UnionCase::kMediumField);
    EXPECT_EQ(unionMessage.GetMediumField(), -1);
    EXPECT_EQ(unionMessage.GetNested1().GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicMessage, UnionMessageNested) {
    NYaFF::TBuilder yffb;
    auto nested = CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, 10);
    yffb.Finish(CreateTUnionMessage<TUnionMessageFlatBuilder>(yffb, 12, 0, 0, nested));

    EXPECT_EQ(yffb.GetSize(), 30ULL);

    const auto& unionMessage = NYaFF::ReadRoot<TUnionMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(unionMessage.GetSomeValue(), 12U);
    EXPECT_EQ(unionMessage.Union_case(), TUnionMessage::UnionCase::kNested2);
    EXPECT_EQ(unionMessage.GetNested2().GetSignedField(), 10);
    EXPECT_EQ(unionMessage.GetNested1().GetSignedField(), 0);
}

TEST(FlatDynamicMessage, UnionMessageString) {
    std::string expected = "aaaa";

    NYaFF::TBuilder yffb;
    auto vec = yffb.CreateString(expected);
    yffb.Finish(CreateTUnionMessage<TUnionMessageFlatBuilder>(yffb, 14, 0, vec));

    EXPECT_EQ(yffb.GetSize(), 28ULL);

    const auto& unionMessage = NYaFF::ReadRoot<TUnionMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(unionMessage.GetSomeValue(), 14U);
    EXPECT_EQ(unionMessage.Union_case(), TUnionMessage::UnionCase::kStringVec);

    std::string str(unionMessage.GetStringVec());
    EXPECT_EQ(str, expected);
}

TEST(FlatDynamicMessage, MixedOneof) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTOneOfMix<TOneOfMixFlatBuilder>(yffb, 1, std::nullopt, 2, 4));

    EXPECT_EQ(yffb.GetSize(), 39ULL);

    const auto& oneofMix = NYaFF::ReadRoot<TOneOfMix>(yffb.GetBufferPointer());
    EXPECT_EQ(oneofMix.Shared_case(), TOneOfMix::SharedCase::kField4);
    EXPECT_EQ(oneofMix.GetField1(), 1ULL);
    EXPECT_EQ(oneofMix.GetField2(), 2ULL);
    EXPECT_EQ(oneofMix.HasField3(), false);
    EXPECT_EQ(oneofMix.GetField3(), 0ULL);
    EXPECT_EQ(oneofMix.HasField4(), true);
    EXPECT_EQ(oneofMix.GetField4(), 4ULL);
}

// ID=1, uint64;
// ID=2, uint32;
// ID=3, uint64;
// ID=4, bool;
// ID=5, uint64;
struct TExplicitMetaV1 {
    inline static constexpr std::array<NYaFF::TFieldOffset, 6> FLAT_OFFSETS = {0, 8, 12, 20, 21, 29};
    inline static constexpr std::array<NYaFF::TFieldId, 0> DELETED_IDS = {};
    inline static constexpr std::array<bool, 5> STATIC_FLAGS = {1, 1, 1, 1, 1};
};

// ID=1, uint64;
// ID=2, removed;
// ID=3, removed;
// ID=4, bool;
// ID=5, uint64;
// ID=6, (added) uint32;
struct TExplicitMetaV2 {
    inline static constexpr std::array<NYaFF::TFieldOffset, 7> FLAT_OFFSETS = {0, 8, 8, 8, 9, 17, 21};
    inline static constexpr std::array<NYaFF::TFieldId, 2> DELETED_IDS = {2, 3};
    inline static constexpr std::array<bool, 6> STATIC_FLAGS = {1, 1, 1, 0, 0, 0};
};

TEST(FlatDynamicMessage, ExplicitSparseCompatibility) {
    // Writes message with dense message info;
    NYaFF::TBuilder yffb;
    yffb.StartFlatMessage<TExplicitMetaV1>(/*implicit*/ false, /*sized*/ true);
    yffb.AddField<uint64_t>(5, 10, 10);
    yffb.AddField<bool>(4, true, false);
    yffb.AddField<uint64_t>(3, 15, 10);
    yffb.AddField<uint32_t>(2, 0, 0);
    yffb.AddField<uint64_t>(1, 20, 0);
    yffb.Finish(NYaFF::TInternalOffset<>{yffb.FinishFlatMessage()});

    EXPECT_EQ(yffb.GetSize(), 37ULL);

    // Reads message with updated sparse message info;
    const auto& msg = NYaFF::ReadRoot<NYaFF::TDynamicMessage<TExplicitMetaV2>>(yffb.GetBufferPointer());
    EXPECT_TRUE(msg.ReadPresence<uint64_t>(1));
    EXPECT_EQ(msg.ReadValue<uint64_t>(1, 0), 20ULL);

    EXPECT_TRUE(msg.ReadPresence<uint64_t>(4));
    EXPECT_EQ(msg.ReadValue<bool>(4, false), true);

    EXPECT_TRUE(msg.ReadPresence<uint64_t>(5));
    EXPECT_EQ(msg.ReadValue<uint64_t>(5, 10), 10ULL);

    EXPECT_FALSE(msg.ReadPresence<uint32_t>(6));
    EXPECT_EQ(msg.ReadValue<uint32_t>(6, 1337), 1337U);
}

// ID=1, bool;
// ID=2, uint32;
// ID=3, uint32;
// ID=4, uint64;
struct TImplicitMetaV1 {
    inline static constexpr std::array<NYaFF::TFieldOffset, 5> FLAT_OFFSETS = {0, 1, 5, 9, 17};
    inline static constexpr std::array<NYaFF::TFieldId, 0> DELETED_IDS = {};
    inline static constexpr std::array<bool, 4> STATIC_FLAGS = {1, 1, 1, 1};
};

// ID=1, removed;
// ID=2, uint32;
// ID=3, uint32;
// ID=4, removed;
struct TImplicitMetaV2 {
    inline static constexpr std::array<NYaFF::TFieldOffset, 4> FLAT_OFFSETS = {0, 0, 4, 8};
    inline static constexpr std::array<NYaFF::TFieldId, 1> DELETED_IDS = {1};
    inline static constexpr std::array<bool, 3> STATIC_FLAGS = {1, 0, 0};
};

TEST(FlatDynamicMessage, ImplicitSparseCompatibility) {
    NYaFF::TBuilder yffb;
    yffb.StartFlatMessage<TImplicitMetaV1>(/*implicit*/ true, /*sized*/ true);
    yffb.AddField<uint64_t>(4, 10, 0);
    yffb.AddField<uint32_t>(3, 8, 8);
    yffb.AddField<uint32_t>(2, 1, 2);
    yffb.AddField<bool>(1, false, false);
    yffb.Finish(NYaFF::TInternalOffset<>{yffb.FinishFlatMessage()});

    EXPECT_EQ(yffb.GetSize(), 24ULL);

    // Reads message with updated sparse message info;
    const auto& msg = NYaFF::ReadRoot<NYaFF::TDynamicMessage<TImplicitMetaV2>>(yffb.GetBufferPointer());
    EXPECT_TRUE(msg.ReadPresence<uint64_t>(2));
    EXPECT_EQ(msg.ReadValue<uint32_t>(2, 1), 2U);

    EXPECT_TRUE(msg.ReadPresence<uint64_t>(3));
    EXPECT_EQ(msg.ReadValue<uint32_t>(3, 8), 8U);
}
