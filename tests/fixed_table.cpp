#include <gtest/gtest.h>
#include <yaff/builder.h>
#include <yaff/table.h>

#include "protoyaff/tables2.yaff.h"

TEST(FixedTable, SimpleTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(NProtoYaFF::NTestYaFF::CreateTSimpleFixedTable(yffb, 0xE, -0xD, 0xC, 0xB, 0xA));

    EXPECT_EQ(yffb.GetSize(), 36ULL);  // only fields and root offset;

    const auto* buf = yffb.GetBufferPointer();
    const auto& simpleTable = NYaFF::ReadRoot<NProtoYaFF::NTestYaFF::TSimpleFixedTable>(buf);

    EXPECT_EQ(simpleTable.GetScalar1(), 0xEULL);
    EXPECT_EQ(simpleTable.GetScalar2(), -0xD);
    EXPECT_EQ(simpleTable.GetScalar3(), 0xCU);
    EXPECT_EQ(simpleTable.GetScalar4(), 0xBU);
    EXPECT_EQ(simpleTable.GetScalar5(), 0xAULL);
}

TEST(FixedTable, DefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(NProtoYaFF::NTestYaFF::CreateTSimpleFixedTable(yffb));

    EXPECT_EQ(yffb.GetSize(), 36ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& simpleTable = NYaFF::ReadRoot<NProtoYaFF::NTestYaFF::TSimpleFixedTable>(buf);

    EXPECT_EQ(simpleTable.GetScalar5(), 0xAULL);
}

TEST(FixedTable, NestedValues) {
    NYaFF::TBuilder yffb;
    const auto nested = NProtoYaFF::NTestYaFF::CreateTSimpleTable<NProtoYaFF::NTestYaFF::TSimpleTableFlatBuilder>(
        yffb, -0x10, 0xFFFF, 0x3333);
    yffb.Finish(NProtoYaFF::NTestYaFF::CreateTStaticFixedTable(yffb, 0xFF, nested));

    EXPECT_EQ(yffb.GetSize(), 34ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<NProtoYaFF::NTestYaFF::TStaticFixedTable>(buf);

    const auto& simpleTable = root.GetNested();
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
    EXPECT_EQ(root.GetScalar(), 0xFFULL);
}

struct TFixedMeta {
    static constexpr size_t LIMIT = 16;
    static constexpr std::array<NYaFF::TFieldOffset, 2> FLAT_OFFSETS = {0, 8};

    static NYaFF::TFieldOffset ResolveField(const NYaFF::TFieldId id) {
        return FLAT_OFFSETS[id - 1];
    }
};

struct TFlatMeta {
    static NYaFF::TFieldOffset ResolveField(const NYaFF::TFieldId) {
        return 0;
    }
};

TEST(FixedTable, FixedValue) {
    NYaFF::TBuilder yffb;

    yffb.StartFixedTable<TFixedMeta>();
    yffb.AddField<uint64_t>(2, 0xAA, 0x0);
    yffb.AddField<uint64_t>(1, 0xFF, 0x0);
    const auto nested = yffb.FinishFixedTable();

    yffb.StartFlatTable<TFlatMeta>();
    yffb.AddField(1, NYaFF::TInternalOffset<void>(nested));
    yffb.Finish(NYaFF::TInternalOffset<void>(yffb.FinishFlatTable()));

    EXPECT_EQ(yffb.GetSize(), 26ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<NYaFF::TDynamicTable<TFlatMeta>>(buf);

    const auto& simpleTable =
        *root.ReadLayout<NYaFF::TFixedTable<TFixedMeta>>(1, &NYaFF::TFixedTable<TFixedMeta>::Default());
    EXPECT_EQ(simpleTable.ReadValue<uint64_t>(1, 0x0), 0xFFULL);
    EXPECT_EQ(simpleTable.ReadValue<uint64_t>(2, 0x0), 0xAAU);
}

TEST(FixedTable, VectorOfInlineFixed) {
    struct TSimpleTable {};
    NYaFF::TBuilder yffb;

    const auto vectorOffset = yffb.CreateVector<NYaFF::TInlineOffset<TSimpleTable>>([&](size_t i) {
        yffb.StartFixedTable<TFixedMeta>();
        yffb.AddField<uint64_t>(2, i, 0x0);
        yffb.AddField<uint64_t>(1, 2 * i, 0x0);
        return std::make_pair(NYaFF::TInlineOffset<TSimpleTable>(yffb.FinishFixedTable()), i + 1 < 10);
    });

    yffb.StartFlatTable<TFlatMeta>();
    yffb.AddField(1, vectorOffset);
    yffb.Finish(NYaFF::TInternalOffset<void>(yffb.FinishFlatTable()));

    EXPECT_EQ(yffb.GetSize(), 174ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<NYaFF::TDynamicTable<TFlatMeta>>(buf);

    const auto& vector = *root.ReadLayout<NYaFF::TVector<NYaFF::TInlineOffset<NYaFF::TFixedTable<TFixedMeta>>>>(
        1, &NYaFF::TVector<NYaFF::TInlineOffset<NYaFF::TFixedTable<TFixedMeta>>>::Default());
    const size_t size = vector.Size();
    EXPECT_EQ(size, 10U);
    for (size_t i = 0; i < size; ++i) {
        const auto& simpleTable = vector.Get(i);
        EXPECT_EQ(simpleTable.ReadValue<uint64_t>(1, 0x0), 2 * (size - i - 1));
        EXPECT_EQ(simpleTable.ReadValue<uint64_t>(2, 0x0), size - i - 1);
    }
}
