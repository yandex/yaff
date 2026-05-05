#include <gtest/gtest.h>
#include <yaff/builder.h>
#include <yaff/message.h>

#include "protoyaff/proto2.yaff.h"
#include "protoyaff/proto3.yaff.h"

using namespace NProtoYaFF::NTestYaFF;

TEST(SparseMessage, NestedMessage) {
    NYaFF::TBuilder yffb;
    const auto nested = CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, -0x10, 0xFFFF, 0x3333);
    yffb.Finish(CreateTStaticSparseMessage(yffb, 0xFF, nested));

    EXPECT_EQ(yffb.GetSize(), 49ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<TStaticSparseMessage>(buf);

    const auto& simpleMessage = root.GetNested();
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
    EXPECT_EQ(root.GetScalar(), 0xFFULL);
}

TEST(SparseMessage, ImplicitMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTImplicitMessage<TImplicitMessageSparseBuilder>(yffb, 0, 1, 0));

    EXPECT_EQ(yffb.GetSize(), 20ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<TImplicitMessage>(buf);

    EXPECT_EQ(root.GetStringField(), "");
    EXPECT_EQ(root.GetSignedField(), 1LL);
    EXPECT_EQ(root.GetUnsignedField(), 0ULL);
}

TEST(SparseDynamicMessage, SimpleMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, -0x10, 0xFFFF, 0x3333));

    EXPECT_EQ(yffb.GetSize(), 29ULL);

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicMessage, BadId) {
    NYaFF::TBuilder yffb;
    yffb.StartSparseMessage();
    EXPECT_THROW(yffb.AddField<int32_t>(0, -0x10, 0), std::runtime_error);
}

TEST(SparseDynamicMessage, BadFillOrder) {
    NYaFF::TBuilder yffb;
    yffb.StartSparseMessage();
    yffb.AddField<uint16_t>(TSimpleMessage::ID_SIGNEDFIELD, 0xFFFF, 0xAF);
    EXPECT_THROW(yffb.AddField<uint64_t>(TSimpleMessage::ID_LARGEFIELD, 0x3333, 0x6789), std::runtime_error);
}

TEST(SparseDynamicMessage, BadMessageOffset) {
    NYaFF::TBuilder yffb;
    std::vector<uint8_t> largeBytes(2147483638, 0);  // 2GB - 10B;
    const auto vectorOffset = yffb.CreateVector(largeBytes);

    yffb.StartSparseMessage();
    yffb.AddField(1, vectorOffset);
    EXPECT_THROW(yffb.FinishSparseMessage(), std::runtime_error);
}

TEST(SparseDynamicMessage, SkipFields) {
    NYaFF::TBuilder yffb;
    TSimpleMessageSparseBuilder builder(yffb);
    builder.add_largefield(0x3333);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 21ULL);  // Sparse Message does not store skipped fields on wire.

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x0);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicMessage, SkipFieldsTail) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, 0x17));

    EXPECT_EQ(yffb.GetSize(), 15ULL);  // Meta tail size optimization;

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x17);
}

TEST(SparseDynamicMessage, DefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, -0x10, std::nullopt, 0x3333));

    EXPECT_EQ(yffb.GetSize(), 25ULL);  // Message is smaller, because empty value does not stored on wire;

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicMessage, TailDefaultValues) {
    NYaFF::TBuilder yffb;
    TSimpleMessageSparseBuilder builder(yffb);
    builder.add_signedfield(-0x10);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 15ULL);

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicMessage, AllDefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb));

    EXPECT_EQ(yffb.GetSize(), 6ULL);  // root offset (4) + typed limit (2)

    const auto& simpleMessage = NYaFF::ReadRoot<TSimpleMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleMessage.GetSignedField(), 0x0);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicMessage, NestedMessages) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, std::nullopt, 0xFFFF);
    auto nested2 = CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, 0x17);
    auto root = CreateTNestedMessage<TNestedMessageSparseBuilder>(yffb, nested1, 0x1234, nested2);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 52ULL);

    const auto& nestedMessage = NYaFF::ReadRoot<TNestedMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested2().GetSignedField(), 0x17);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(nestedMessage.HasNested1() && nestedMessage.HasNested2());
}

TEST(SparseDynamicMessage, NestedMessageEmpty) {
    NYaFF::TBuilder yffb;
    auto nested = CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, std::nullopt, std::nullopt, 0x5678);
    auto root = CreateTNestedMessage<TNestedMessageSparseBuilder>(yffb, nested);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 32ULL);

    const auto& nestedMessage = NYaFF::ReadRoot<TNestedMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x5678ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSignedField(), 0x0);
    EXPECT_TRUE(nestedMessage.HasNested1() && !nestedMessage.HasNested2());
}

TEST(SparseDynamicMessage, MetaDeduplication) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, std::nullopt, 0x20);
    auto nested2 = CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, std::nullopt, 0x10);
    auto root = CreateTNestedMessage<TNestedMessageSparseBuilder>(yffb, nested1, std::nullopt, nested2);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 43ULL);  // because nested2 meta is deduplicated;

    const auto& nestedMessage = NYaFF::ReadRoot<TNestedMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0x20U);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0x10U);
}

TEST(SparseDynamicMessage, FloatMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTFloatMessage<TFloatMessageSparseBuilder>(yffb, 1.1234f, std::nullopt));

    EXPECT_EQ(yffb.GetSize(), 15ULL);

    const auto& floatMessage = NYaFF::ReadRoot<TFloatMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(floatMessage.GetFloatField(), 1.1234f);
    EXPECT_EQ(floatMessage.GetDoubleField(), 1e-6);
}

TEST(SparseDynamicMessage, UnionMessageEmpty) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTUnionMessage<TUnionMessageSparseBuilder>(yffb, 10));

    EXPECT_EQ(yffb.GetSize(), 15ULL);

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

TEST(SparseDynamicMessage, UnionMessageExplicitPresence) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTUnionMessage<TUnionMessageSparseBuilder>(yffb, 10, 0, 0, 0, -1));  // -1 is default value;

    EXPECT_EQ(yffb.GetSize(), 23ULL);

    const auto& unionMessage = NYaFF::ReadRoot<TUnionMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(unionMessage.GetSomeValue(), 10U);
    EXPECT_EQ(unionMessage.Union_case(), TUnionMessage::UnionCase::kMediumField);
    EXPECT_EQ(unionMessage.GetMediumField(), -1);
    EXPECT_EQ(unionMessage.GetNested1().GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicMessage, UnionMessageNested) {
    NYaFF::TBuilder yffb;
    auto nested = CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, 10);
    yffb.Finish(CreateTUnionMessage<TUnionMessageSparseBuilder>(yffb, 12, 0, 0, nested));

    EXPECT_EQ(yffb.GetSize(), 33ULL);

    const auto& unionMessage = NYaFF::ReadRoot<TUnionMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(unionMessage.GetSomeValue(), 12U);
    EXPECT_EQ(unionMessage.Union_case(), TUnionMessage::UnionCase::kNested2);
    EXPECT_EQ(unionMessage.GetNested2().GetSignedField(), 10);
    EXPECT_EQ(unionMessage.GetNested1().GetSignedField(), 0);
}

TEST(SparseDynamicMessage, MixedOneof) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTOneOfMix<TOneOfMixSparseBuilder>(yffb, 1, std::nullopt, 2, 4));

    EXPECT_EQ(yffb.GetSize(), 38ULL);

    const auto& oneofMix = NYaFF::ReadRoot<TOneOfMix>(yffb.GetBufferPointer());
    EXPECT_EQ(oneofMix.Shared_case(), TOneOfMix::SharedCase::kField4);
    EXPECT_EQ(oneofMix.GetField1(), 1ULL);
    EXPECT_EQ(oneofMix.GetField2(), 2ULL);
    EXPECT_EQ(oneofMix.HasField3(), false);
    EXPECT_EQ(oneofMix.GetField3(), 0ULL);
    EXPECT_EQ(oneofMix.HasField4(), true);
    EXPECT_EQ(oneofMix.GetField4(), 4ULL);
}

TEST(SparseDynamicMessage, GappedMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTGappedMessage(yffb, 1, std::nullopt, 5));

    EXPECT_EQ(yffb.GetSize(), 31ULL);

    const auto& gappedMessage = NYaFF::ReadRoot<TGappedMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(gappedMessage.GetField1(), 1ULL);
    EXPECT_EQ(gappedMessage.GetField3(), 0ULL);
    EXPECT_EQ(gappedMessage.GetField5(), 5ULL);
}
