#include <gtest/gtest.h>
#include <yaff/builder.h>
#include <yaff/message.h>

#include "protoyaff/proto2.yaff.h"

TEST(FixedMessage, SimpleMessage) {
    NYaFF::TBuilder yffb;
    yffb.Finish(NProtoYaFF::NTestYaFF::CreateTSimpleFixedMessage(yffb, 0xE, -0xD, 0xC, 0xB, 0xA));

    EXPECT_EQ(yffb.GetSize(), 36ULL);  // only fields and root offset;

    const auto* buf = yffb.GetBufferPointer();
    const auto& simpleMessage = NYaFF::ReadRoot<NProtoYaFF::NTestYaFF::TSimpleFixedMessage>(buf);

    EXPECT_EQ(simpleMessage.GetScalar1(), 0xEULL);
    EXPECT_EQ(simpleMessage.GetScalar2(), -0xD);
    EXPECT_EQ(simpleMessage.GetScalar3(), 0xCU);
    EXPECT_EQ(simpleMessage.GetScalar4(), 0xBU);
    EXPECT_EQ(simpleMessage.GetScalar5(), 0xAULL);
}

TEST(FixedMessage, DefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(NProtoYaFF::NTestYaFF::CreateTSimpleFixedMessage(yffb));

    EXPECT_EQ(yffb.GetSize(), 36ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& simpleMessage = NYaFF::ReadRoot<NProtoYaFF::NTestYaFF::TSimpleFixedMessage>(buf);

    EXPECT_EQ(simpleMessage.GetScalar5(), 0xAULL);
}

TEST(FixedMessage, NestedValues) {
    NYaFF::TBuilder yffb;
    const auto nested = NProtoYaFF::NTestYaFF::CreateTSimpleMessage<NProtoYaFF::NTestYaFF::TSimpleMessageFlatBuilder>(
        yffb, -0x10, 0xFFFF, 0x3333);
    yffb.Finish(NProtoYaFF::NTestYaFF::CreateTStaticFixedMessage(yffb, 0xFF, nested));

    EXPECT_EQ(yffb.GetSize(), 35ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<NProtoYaFF::NTestYaFF::TStaticFixedMessage>(buf);

    const auto& simpleMessage = root.GetNested();
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
    EXPECT_EQ(root.GetScalar(), 0xFFULL);
}

struct TFixedMeta {
    static constexpr size_t LIMIT = 16;
    static constexpr std::array<NYaFF::TFieldOffset, 3> FLAT_OFFSETS = {0, 8, 16};
};

struct TFlatMeta {
    static constexpr std::array<NYaFF::TFieldOffset, 2> FLAT_OFFSETS = {0, 4};
    static constexpr std::array<NYaFF::TFieldId, 0> DELETED_IDS = {};
    static constexpr std::array<bool, 1> STATIC_FLAGS = {1};
};

TEST(FixedMessage, FixedValue) {
    NYaFF::TBuilder yffb;

    yffb.StartFixedMessage<TFixedMeta>();
    yffb.AddField<uint64_t>(2, 0xAA, 0x0);
    yffb.AddField<uint64_t>(1, 0xFF, 0x0);
    const auto nested = yffb.FinishFixedMessage();

    yffb.StartFlatMessage<TFlatMeta>();
    yffb.AddField(1, NYaFF::TInternalOffset<void>(nested));
    yffb.Finish(NYaFF::TInternalOffset<void>(yffb.FinishFlatMessage()));

    EXPECT_EQ(yffb.GetSize(), 26ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<NYaFF::TDynamicMessage<TFlatMeta>>(buf);

    const auto& simpleMessage =
        *root.ReadLayout<NYaFF::TFixedMessage<TFixedMeta>>(1, &NYaFF::TFixedMessage<TFixedMeta>::Default());
    EXPECT_EQ(simpleMessage.ReadValue<uint64_t>(1, 0x0), 0xFFULL);
    EXPECT_EQ(simpleMessage.ReadValue<uint64_t>(2, 0x0), 0xAAU);
}

TEST(FixedMessage, VectorOfInlineFixed) {
    struct TSimpleMessage {};
    NYaFF::TBuilder yffb;

    const auto vectorOffset = yffb.CreateVector<NYaFF::TInlineOffset<TSimpleMessage>>([&](size_t i) {
        yffb.StartFixedMessage<TFixedMeta>();
        yffb.AddField<uint64_t>(2, i, 0x0);
        yffb.AddField<uint64_t>(1, 2 * i, 0x0);
        return std::make_pair(NYaFF::TInlineOffset<TSimpleMessage>(yffb.FinishFixedMessage()), i + 1 < 10);
    });

    yffb.StartFlatMessage<TFlatMeta>();
    yffb.AddField(1, vectorOffset);
    yffb.Finish(NYaFF::TInternalOffset<void>(yffb.FinishFlatMessage()));

    EXPECT_EQ(yffb.GetSize(), 174ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<NYaFF::TDynamicMessage<TFlatMeta>>(buf);

    const auto& vector = *root.ReadLayout<NYaFF::TVector<NYaFF::TInlineOffset<NYaFF::TFixedMessage<TFixedMeta>>>>(
        1, &NYaFF::TVector<NYaFF::TInlineOffset<NYaFF::TFixedMessage<TFixedMeta>>>::Default());
    const size_t size = vector.Size();
    EXPECT_EQ(size, 10U);
    for (size_t i = 0; i < size; ++i) {
        const auto& simpleMessage = vector.Get(i);
        EXPECT_EQ(simpleMessage.ReadValue<uint64_t>(1, 0x0), 2 * (size - i - 1));
        EXPECT_EQ(simpleMessage.ReadValue<uint64_t>(2, 0x0), size - i - 1);
    }
}
