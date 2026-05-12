#include <gtest/gtest.h>
#include <yaff/message.h>
#include <yaff/serializer.h>

#include "protoyaff/proto2.yaff.h"

TEST(FixedMessage, SimpleMessage) {
    yaff::Serializer ys;
    ys.Finish(protoyaff::test::SerializeSimpleFixedMessage(ys, 0xE, -0xD, 0xC, 0xB, 0xA));

    EXPECT_EQ(ys.Size(), 36ULL);  // only fields and root offset;

    const auto* buf = ys.Data();
    const auto& simpleMessage = yaff::ReadRoot<protoyaff::test::SimpleFixedMessage>(buf);

    EXPECT_EQ(simpleMessage.GetScalar1(), 0xEULL);
    EXPECT_EQ(simpleMessage.GetScalar2(), -0xD);
    EXPECT_EQ(simpleMessage.GetScalar3(), 0xCU);
    EXPECT_EQ(simpleMessage.GetScalar4(), 0xBU);
    EXPECT_EQ(simpleMessage.GetScalar5(), 0xAULL);
}

TEST(FixedMessage, DefaultValues) {
    yaff::Serializer ys;
    ys.Finish(protoyaff::test::SerializeSimpleFixedMessage(ys));

    EXPECT_EQ(ys.Size(), 36ULL);

    const auto* buf = ys.Data();
    const auto& simpleMessage = yaff::ReadRoot<protoyaff::test::SimpleFixedMessage>(buf);

    EXPECT_EQ(simpleMessage.GetScalar5(), 0xAULL);
}

TEST(FixedMessage, NestedValues) {
    yaff::Serializer ys;
    const auto nested = protoyaff::test::SerializeSimpleMessage<protoyaff::test::SimpleMessageFlatSerializer>(
        ys, -0x10, 0xFFFF, 0x3333);
    ys.Finish(protoyaff::test::SerializeStaticFixedMessage(ys, 0xFF, nested));

    EXPECT_EQ(ys.Size(), 35ULL);

    const auto* buf = ys.Data();
    const auto& root = yaff::ReadRoot<protoyaff::test::StaticFixedMessage>(buf);

    const auto& simpleMessage = root.GetNested();
    EXPECT_EQ(simpleMessage.GetSignedField(), -0x10);
    EXPECT_EQ(simpleMessage.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleMessage.GetLargeField(), 0x3333ULL);
    EXPECT_EQ(root.GetScalar(), 0xFFULL);
}

struct TFixedMeta {
    static constexpr size_t LIMIT = 16;
    static constexpr std::array<yaff::FieldOffset, 3> FLAT_OFFSETS = {0, 8, 16};
};

struct TFlatMeta {
    static constexpr std::array<yaff::FieldOffset, 2> FLAT_OFFSETS = {0, 4};
    static constexpr std::array<yaff::FieldId, 0> DELETED_IDS = {};
    static constexpr std::array<bool, 1> STATIC_FLAGS = {1};
};

TEST(FixedMessage, FixedValue) {
    yaff::Serializer ys;

    ys.StartFixedMessage<TFixedMeta>();
    ys.AddField<uint64_t>(2, 0xAA, 0x0);
    ys.AddField<uint64_t>(1, 0xFF, 0x0);
    const auto nested = ys.FinishFixedMessage();

    ys.StartFlatMessage<TFlatMeta>();
    ys.AddField(1, yaff::InternalOffset<void>(nested));
    ys.Finish(yaff::InternalOffset<void>(ys.FinishFlatMessage()));

    EXPECT_EQ(ys.Size(), 26ULL);

    const auto* buf = ys.Data();
    const auto& root = yaff::ReadRoot<yaff::DynamicMessage<TFlatMeta>>(buf);

    const auto& simpleMessage =
        *root.ReadLayout<yaff::FixedMessage<TFixedMeta>>(1, &yaff::FixedMessage<TFixedMeta>::Default());
    EXPECT_EQ(simpleMessage.ReadValue<uint64_t>(1, 0x0), 0xFFULL);
    EXPECT_EQ(simpleMessage.ReadValue<uint64_t>(2, 0x0), 0xAAU);
}

TEST(FixedMessage, ArrayOfInlineFixed) {
    struct SimpleMessage {};
    yaff::Serializer ys;

    const auto vectorOffset = ys.SerializeArray<yaff::InlineOffset<SimpleMessage>>([&](size_t i) {
        ys.StartFixedMessage<TFixedMeta>();
        ys.AddField<uint64_t>(2, i, 0x0);
        ys.AddField<uint64_t>(1, 2 * i, 0x0);
        return std::make_pair(yaff::InlineOffset<SimpleMessage>(ys.FinishFixedMessage()), i + 1 < 10);
    });

    ys.StartFlatMessage<TFlatMeta>();
    ys.AddField(1, vectorOffset);
    ys.Finish(yaff::InternalOffset<void>(ys.FinishFlatMessage()));

    EXPECT_EQ(ys.Size(), 174ULL);

    const auto* buf = ys.Data();
    const auto& root = yaff::ReadRoot<yaff::DynamicMessage<TFlatMeta>>(buf);

    const auto& vector = *root.ReadLayout<yaff::Array<yaff::InlineOffset<yaff::FixedMessage<TFixedMeta>>>>(
        1, &yaff::Array<yaff::InlineOffset<yaff::FixedMessage<TFixedMeta>>>::Default());
    const size_t size = vector.Size();
    EXPECT_EQ(size, 10U);
    for (size_t i = 0; i < size; ++i) {
        const auto& simpleMessage = vector.Get(i);
        EXPECT_EQ(simpleMessage.ReadValue<uint64_t>(1, 0x0), 2 * (size - i - 1));
        EXPECT_EQ(simpleMessage.ReadValue<uint64_t>(2, 0x0), size - i - 1);
    }
}
