#include <gtest/gtest.h>
#include <yaff/message.h>
#include <yaff/serializer.h>

#include "protoyaff/proto2.yaff.h"
#include "protoyaff/proto3.yaff.h"

using namespace protoyaff::test;

TEST(FlatMessage, NestedMessage) {
    yaff::Serializer ys;
    const auto nested = SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, -0x10, 0xFFFF, 0x3333);
    ys.Finish(SerializeStaticFlatMessage(ys, 0xFF, std::nullopt, nested));

    EXPECT_EQ(ys.Size(), 41ULL);

    const auto* buf = ys.Data();

    const auto& root = yaff::ReadRoot<StaticFlatMessage>(buf);

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
    yaff::Serializer ys;
    ys.Finish(SerializeImplicitMessage(ys, 0, 1, 0));

    EXPECT_EQ(ys.Size(), 19ULL);

    const auto* buf = ys.Data();
    const auto& root = yaff::ReadRoot<ImplicitMessage>(buf);

    EXPECT_EQ(root.GetStringField(), "");
    EXPECT_EQ(root.GetSignedField(), 1LL);
    EXPECT_EQ(root.GetUnsignedField(), 0ULL);
}

TEST(FlatDynamicMessage, ImplicitMessage) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, -0x10, 0xFFFF, 0x3333));

    EXPECT_EQ(ys.Size(), 23ULL);

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicMessage, ExplicitMessage) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, -0x10, 0xAF,
                                                                  0x3333));  // Sets second field to default explicitly;

    EXPECT_EQ(ys.Size(), 24ULL);  // Allocates 2 more byte for presence and sizes mask;

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicMessage, BadFillOrder) {
    yaff::Serializer ys;
    ys.StartFlatMessage<SimpleMessage::MetaType>();
    ys.AddField<int32_t>(SimpleMessage::ID_SIGNEDFIELD, -0x10, 0);
    EXPECT_THROW(ys.AddField<uint64_t>(SimpleMessage::ID_LARGEFIELD, 0x3333, 0x6789), std::runtime_error);
}

TEST(FlatDynamicMessage, SkipFields) {
    yaff::Serializer ys;
    SimpleMessageFlatSerializer serializer(ys);
    serializer.add_largefield(0x3333);
    serializer.add_signedfield(-0x10);
    auto root = std::move(serializer).Finish();
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 23ULL);

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicMessage, SkipFieldsFront) {
    yaff::Serializer ys;
    SimpleMessageFlatSerializer serializer(ys);
    serializer.add_largefield(0x3333);
    auto root = std::move(serializer).Finish();
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 23ULL);

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x0);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicMessage, SkipFieldsTail) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, 0x17));

    EXPECT_EQ(ys.Size(), 11ULL);  // Tail size optimization;

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x17);
}

TEST(FlatDynamicMessage, DefaultValues) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, -0x10, std::nullopt,
                                                                  0x1234));  // Sets empty value for small field

    EXPECT_EQ(ys.Size(), 23ULL);

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x1234ULL);
}

TEST(FlatDynamicMessage, TailDefaultValues) {
    yaff::Serializer ys;
    SimpleMessageFlatSerializer serializer(ys);
    serializer.add_signedfield(-0x10);
    auto root = std::move(serializer).Finish();
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 11ULL);  // Does not store empty values in the end of object;

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicMessage, AllDefaultValues) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys));

    EXPECT_EQ(ys.Size(), 6ULL);  // root offset + flat message typed size;

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x0);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicMessage, NestedMessages) {
    yaff::Serializer ys;
    auto nested1 = SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, 0, 0xFFFF);
    auto nested2 = SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, 0x17);
    auto root = SerializeNestedMessage<NestedMessageFlatSerializer>(ys, nested1, 0x1234, nested2);
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 41ULL);

    const auto& nestedMessage = yaff::ReadRoot<NestedMessage>(ys.Data());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested2().GetSignedField(), 0x17);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(nestedMessage.HasNested1() && nestedMessage.HasNested2());
}

TEST(FlatDynamicMessage, NestedMessageEmpty) {
    yaff::Serializer ys;
    auto nested = SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, 0, 0xAF, 0x5678);
    auto root = SerializeNestedMessage<NestedMessageFlatSerializer>(ys, nested);
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 31ULL);

    const auto& nestedMessage = yaff::ReadRoot<NestedMessage>(ys.Data());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x5678ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSignedField(), 0x0);
    EXPECT_TRUE(nestedMessage.HasNested1() && !nestedMessage.HasNested2());
}

TEST(FlatDynamicMessage, FloatMessage) {
    yaff::Serializer ys;
    ys.Finish(SerializeFloatMessage<FloatMessageFlatSerializer>(ys, 1.1234f, std::nullopt));

    EXPECT_EQ(ys.Size(), 11ULL);  // double empty value is not stored;

    const auto& floatMessage = yaff::ReadRoot<FloatMessage>(ys.Data());
    EXPECT_EQ(floatMessage.GetFloatField(), 1.1234f);
    EXPECT_EQ(floatMessage.GetDoubleField(), 1e-6);
}

TEST(FlatDynamicMessage, UnionMessageEmpty) {
    yaff::Serializer ys;
    ys.Finish(SerializeUnionMessage<UnionMessageFlatSerializer>(ys, 10));
    EXPECT_EQ(ys.Size(), 11ULL);  // shared values is not stored;

    const auto& unionMessage = yaff::ReadRoot<UnionMessage>(ys.Data());
    EXPECT_EQ(unionMessage.GetSomeValue(), 10U);
    EXPECT_EQ(unionMessage.Union_case(), UnionMessage::UnionCase::UNION_NOT_SET);

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
    yaff::Serializer ys;
    ys.Finish(SerializeUnionMessage<UnionMessageFlatSerializer>(ys, 10, 0, 0, 0, -1));  // -1 is default value;

    EXPECT_EQ(ys.Size(), 28ULL);

    const auto& unionMessage = yaff::ReadRoot<UnionMessage>(ys.Data());
    EXPECT_EQ(unionMessage.GetSomeValue(), 10U);
    EXPECT_EQ(unionMessage.Union_case(), UnionMessage::UnionCase::kMediumField);
    EXPECT_EQ(unionMessage.GetMediumField(), -1);
    EXPECT_EQ(unionMessage.GetNested1().GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicMessage, UnionMessageNested) {
    yaff::Serializer ys;
    auto nested = SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, 10);
    ys.Finish(SerializeUnionMessage<UnionMessageFlatSerializer>(ys, 12, 0, 0, nested));

    EXPECT_EQ(ys.Size(), 30ULL);

    const auto& unionMessage = yaff::ReadRoot<UnionMessage>(ys.Data());
    EXPECT_EQ(unionMessage.GetSomeValue(), 12U);
    EXPECT_EQ(unionMessage.Union_case(), UnionMessage::UnionCase::kNested2);
    EXPECT_EQ(unionMessage.GetNested2().GetSignedField(), 10);
    EXPECT_EQ(unionMessage.GetNested1().GetSignedField(), 0);
}

TEST(FlatDynamicMessage, UnionMessageString) {
    std::string expected = "aaaa";

    yaff::Serializer ys;
    auto vec = ys.SerializeString(expected);
    ys.Finish(SerializeUnionMessage<UnionMessageFlatSerializer>(ys, 14, 0, vec));

    EXPECT_EQ(ys.Size(), 28ULL);

    const auto& unionMessage = yaff::ReadRoot<UnionMessage>(ys.Data());
    EXPECT_EQ(unionMessage.GetSomeValue(), 14U);
    EXPECT_EQ(unionMessage.Union_case(), UnionMessage::UnionCase::kStringVec);

    std::string str(unionMessage.GetStringVec());
    EXPECT_EQ(str, expected);
}

TEST(FlatDynamicMessage, MixedOneof) {
    yaff::Serializer ys;
    ys.Finish(SerializeOneOfMix<OneOfMixFlatSerializer>(ys, 1, std::nullopt, 2, 4));

    EXPECT_EQ(ys.Size(), 39ULL);

    const auto& oneofMix = yaff::ReadRoot<OneOfMix>(ys.Data());
    EXPECT_EQ(oneofMix.Shared_case(), OneOfMix::SharedCase::kField4);
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
    inline static constexpr std::array<yaff::FieldOffset, 6> FLAT_OFFSETS = {0, 8, 12, 20, 21, 29};
    inline static constexpr std::array<yaff::FieldId, 0> DELETED_IDS = {};
    inline static constexpr std::array<bool, 5> STATIC_FLAGS = {1, 1, 1, 1, 1};
};

// ID=1, uint64;
// ID=2, removed;
// ID=3, removed;
// ID=4, bool;
// ID=5, uint64;
// ID=6, (added) uint32;
struct TExplicitMetaV2 {
    inline static constexpr std::array<yaff::FieldOffset, 7> FLAT_OFFSETS = {0, 8, 8, 8, 9, 17, 21};
    inline static constexpr std::array<yaff::FieldId, 2> DELETED_IDS = {2, 3};
    inline static constexpr std::array<bool, 6> STATIC_FLAGS = {1, 1, 1, 0, 0, 0};
};

TEST(FlatDynamicMessage, ExplicitSparseCompatibility) {
    // Writes message with dense message info;
    yaff::Serializer ys;
    ys.StartFlatMessage<TExplicitMetaV1>(/*implicit*/ false, /*sized*/ true);
    ys.AddField<uint64_t>(5, 10, 10);
    ys.AddField<bool>(4, true, false);
    ys.AddField<uint64_t>(3, 15, 10);
    ys.AddField<uint32_t>(2, 0, 0);
    ys.AddField<uint64_t>(1, 20, 0);
    ys.Finish(yaff::InternalOffset<>{ys.FinishFlatMessage()});

    EXPECT_EQ(ys.Size(), 37ULL);

    // Reads message with updated sparse message info;
    const auto& msg = yaff::ReadRoot<yaff::DynamicMessage<TExplicitMetaV2>>(ys.Data());
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
    inline static constexpr std::array<yaff::FieldOffset, 5> FLAT_OFFSETS = {0, 1, 5, 9, 17};
    inline static constexpr std::array<yaff::FieldId, 0> DELETED_IDS = {};
    inline static constexpr std::array<bool, 4> STATIC_FLAGS = {1, 1, 1, 1};
};

// ID=1, removed;
// ID=2, uint32;
// ID=3, uint32;
// ID=4, removed;
struct TImplicitMetaV2 {
    inline static constexpr std::array<yaff::FieldOffset, 4> FLAT_OFFSETS = {0, 0, 4, 8};
    inline static constexpr std::array<yaff::FieldId, 1> DELETED_IDS = {1};
    inline static constexpr std::array<bool, 3> STATIC_FLAGS = {1, 0, 0};
};

TEST(FlatDynamicMessage, ImplicitSparseCompatibility) {
    yaff::Serializer ys;
    ys.StartFlatMessage<TImplicitMetaV1>(/*implicit*/ true, /*sized*/ true);
    ys.AddField<uint64_t>(4, 10, 0);
    ys.AddField<uint32_t>(3, 8, 8);
    ys.AddField<uint32_t>(2, 1, 2);
    ys.AddField<bool>(1, false, false);
    ys.Finish(yaff::InternalOffset<>{ys.FinishFlatMessage()});

    EXPECT_EQ(ys.Size(), 24ULL);

    // Reads message with updated sparse message info;
    const auto& msg = yaff::ReadRoot<yaff::DynamicMessage<TImplicitMetaV2>>(ys.Data());
    EXPECT_TRUE(msg.ReadPresence<uint64_t>(2));
    EXPECT_EQ(msg.ReadValue<uint32_t>(2, 1), 2U);

    EXPECT_TRUE(msg.ReadPresence<uint64_t>(3));
    EXPECT_EQ(msg.ReadValue<uint32_t>(3, 8), 8U);
}
