#include <gtest/gtest.h>
#include <yaff/message.h>
#include <yaff/serializer.h>

#include "protoyaff/proto2.yaff.h"
#include "protoyaff/proto3.yaff.h"

using namespace protoyaff::test;

TEST(SparseMessage, NestedMessage) {
    yaff::Serializer ys;
    const auto nested = SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, -0x10, 0xFFFF, 0x3333);
    ys.Finish(SerializeStaticSparseMessage(ys, 0xFF, nested));

    EXPECT_EQ(ys.Size(), 49ULL);

    const auto* buf = ys.Data();
    const auto& root = yaff::ReadRoot<StaticSparseMessage>(buf);

    const auto& simpleMessage = root.GetNested();
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
    EXPECT_EQ(root.GetScalar(), 0xFFULL);
}

TEST(SparseMessage, ImplicitMessage) {
    yaff::Serializer ys;
    ys.Finish(SerializeImplicitMessage<ImplicitMessageSparseSerializer>(ys, 0, 1, 0));

    EXPECT_EQ(ys.Size(), 20ULL);

    const auto* buf = ys.Data();
    const auto& root = yaff::ReadRoot<ImplicitMessage>(buf);

    EXPECT_EQ(root.GetStringField(), "");
    EXPECT_EQ(root.GetSignedField(), 1LL);
    EXPECT_EQ(root.GetUnsignedField(), 0ULL);
}

TEST(SparseDynamicMessage, SimpleMessage) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, -0x10, 0xFFFF, 0x3333));

    EXPECT_EQ(ys.Size(), 29ULL);

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicMessage, BadId) {
    yaff::Serializer ys;
    ys.StartSparseMessage();
    EXPECT_THROW(ys.AddField<int32_t>(0, -0x10, 0), std::runtime_error);
}

TEST(SparseDynamicMessage, BadFillOrder) {
    yaff::Serializer ys;
    ys.StartSparseMessage();
    ys.AddField<uint16_t>(SimpleMessage::ID_SIGNEDFIELD, 0xFFFF, 0xAF);
    EXPECT_THROW(ys.AddField<uint64_t>(SimpleMessage::ID_LARGEFIELD, 0x3333, 0x6789), std::runtime_error);
}

TEST(SparseDynamicMessage, BadMessageOffset) {
    yaff::Serializer ys;
    std::vector<uint8_t> largeBytes(2147483638, 0);  // 2GB - 10B;
    const auto vectorOffset = ys.SerializeArray(largeBytes);

    ys.StartSparseMessage();
    ys.AddField(1, vectorOffset);
    EXPECT_THROW(ys.FinishSparseMessage(), std::runtime_error);
}

TEST(SparseDynamicMessage, SkipFields) {
    yaff::Serializer ys;
    SimpleMessageSparseSerializer serializer(ys);
    serializer.add_largefield(0x3333);
    auto root = std::move(serializer).Finish();
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 21ULL);  // Sparse Message does not store skipped fields on wire.

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x0);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicMessage, SkipFieldsTail) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, 0x17));

    EXPECT_EQ(ys.Size(), 15ULL);  // Meta tail size optimization;

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x17);
}

TEST(SparseDynamicMessage, DefaultValues) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, -0x10, std::nullopt, 0x3333));

    EXPECT_EQ(ys.Size(), 25ULL);  // Message is smaller, because empty value does not stored on wire;

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicMessage, TailDefaultValues) {
    yaff::Serializer ys;
    SimpleMessageSparseSerializer serializer(ys);
    serializer.add_signedfield(-0x10);
    auto root = std::move(serializer).Finish();
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 15ULL);

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicMessage, AllDefaultValues) {
    yaff::Serializer ys;
    ys.Finish(SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys));

    EXPECT_EQ(ys.Size(), 6ULL);  // root offset (4) + typed limit (2)

    const auto& simpleMessage = yaff::ReadRoot<SimpleMessage>(ys.Data());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x0);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicMessage, NestedMessages) {
    yaff::Serializer ys;
    auto nested1 = SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, std::nullopt, 0xFFFF);
    auto nested2 = SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, 0x17);
    auto root = SerializeNestedMessage<NestedMessageSparseSerializer>(ys, nested1, 0x1234, nested2);
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 52ULL);

    const auto& nestedMessage = yaff::ReadRoot<NestedMessage>(ys.Data());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested2().GetSignedField(), 0x17);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(nestedMessage.HasNested1() && nestedMessage.HasNested2());
}

TEST(SparseDynamicMessage, NestedMessageEmpty) {
    yaff::Serializer ys;
    auto nested = SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, std::nullopt, std::nullopt, 0x5678);
    auto root = SerializeNestedMessage<NestedMessageSparseSerializer>(ys, nested);
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 32ULL);

    const auto& nestedMessage = yaff::ReadRoot<NestedMessage>(ys.Data());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x5678ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSignedField(), 0x0);
    EXPECT_TRUE(nestedMessage.HasNested1() && !nestedMessage.HasNested2());
}

TEST(SparseDynamicMessage, MetaDeduplication) {
    yaff::Serializer ys;
    auto nested1 = SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, std::nullopt, 0x20);
    auto nested2 = SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, std::nullopt, 0x10);
    auto root = SerializeNestedMessage<NestedMessageSparseSerializer>(ys, nested1, std::nullopt, nested2);
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 43ULL);  // because nested2 meta is deduplicated;

    const auto& nestedMessage = yaff::ReadRoot<NestedMessage>(ys.Data());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0x20U);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0x10U);
}

TEST(SparseDynamicMessage, FloatMessage) {
    yaff::Serializer ys;
    ys.Finish(SerializeFloatMessage<FloatMessageSparseSerializer>(ys, 1.1234f, std::nullopt));

    EXPECT_EQ(ys.Size(), 15ULL);

    const auto& floatMessage = yaff::ReadRoot<FloatMessage>(ys.Data());
    EXPECT_EQ(floatMessage.GetFloatField(), 1.1234f);
    EXPECT_EQ(floatMessage.GetDoubleField(), 1e-6);
}

TEST(SparseDynamicMessage, UnionMessageEmpty) {
    yaff::Serializer ys;
    ys.Finish(SerializeUnionMessage<UnionMessageSparseSerializer>(ys, 10));

    EXPECT_EQ(ys.Size(), 15ULL);

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

TEST(SparseDynamicMessage, UnionMessageExplicitPresence) {
    yaff::Serializer ys;
    ys.Finish(SerializeUnionMessage<UnionMessageSparseSerializer>(ys, 10, 0, 0, 0, -1));  // -1 is default value;

    EXPECT_EQ(ys.Size(), 23ULL);

    const auto& unionMessage = yaff::ReadRoot<UnionMessage>(ys.Data());
    EXPECT_EQ(unionMessage.GetSomeValue(), 10U);
    EXPECT_EQ(unionMessage.Union_case(), UnionMessage::UnionCase::kMediumField);
    EXPECT_EQ(unionMessage.GetMediumField(), -1);
    EXPECT_EQ(unionMessage.GetNested1().GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicMessage, UnionMessageNested) {
    yaff::Serializer ys;
    auto nested = SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, 10);
    ys.Finish(SerializeUnionMessage<UnionMessageSparseSerializer>(ys, 12, 0, 0, nested));

    EXPECT_EQ(ys.Size(), 33ULL);

    const auto& unionMessage = yaff::ReadRoot<UnionMessage>(ys.Data());
    EXPECT_EQ(unionMessage.GetSomeValue(), 12U);
    EXPECT_EQ(unionMessage.Union_case(), UnionMessage::UnionCase::kNested2);
    EXPECT_EQ(unionMessage.GetNested2().GetSignedField(), 10);
    EXPECT_EQ(unionMessage.GetNested1().GetSignedField(), 0);
}

TEST(SparseDynamicMessage, MixedOneof) {
    yaff::Serializer ys;
    ys.Finish(SerializeOneOfMix<OneOfMixSparseSerializer>(ys, 1, std::nullopt, 2, 4));

    EXPECT_EQ(ys.Size(), 38ULL);

    const auto& oneofMix = yaff::ReadRoot<OneOfMix>(ys.Data());
    EXPECT_EQ(oneofMix.Shared_case(), OneOfMix::SharedCase::kField4);
    EXPECT_EQ(oneofMix.GetField1(), 1ULL);
    EXPECT_EQ(oneofMix.GetField2(), 2ULL);
    EXPECT_EQ(oneofMix.HasField3(), false);
    EXPECT_EQ(oneofMix.GetField3(), 0ULL);
    EXPECT_EQ(oneofMix.HasField4(), true);
    EXPECT_EQ(oneofMix.GetField4(), 4ULL);
}

TEST(SparseDynamicMessage, GappedMessage) {
    yaff::Serializer ys;
    ys.Finish(SerializeGappedMessage(ys, 1, std::nullopt, 5));

    EXPECT_EQ(ys.Size(), 31ULL);

    const auto& gappedMessage = yaff::ReadRoot<GappedMessage>(ys.Data());
    EXPECT_EQ(gappedMessage.GetField1(), 1ULL);
    EXPECT_EQ(gappedMessage.GetField3(), 0ULL);
    EXPECT_EQ(gappedMessage.GetField5(), 5ULL);
}
