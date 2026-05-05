#include <gtest/gtest.h>
#include <yaff/builder.h>
#include <yaff/table.h>

#include "protoyaff/tables2.yaff.h"
#include "protoyaff/tables3.yaff.h"

using namespace NProtoYaFF::NTestYaFF;

TEST(FlatTable, NestedTable) {
    NYaFF::TBuilder yffb;
    const auto nested = CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, -0x10, 0xFFFF, 0x3333);
    yffb.Finish(CreateTStaticFlatTable(yffb, 0xFF, std::nullopt, nested));

    EXPECT_EQ(yffb.GetSize(), 40ULL);

    const auto* buf = yffb.GetBufferPointer();

    const auto& root = NYaFF::ReadRoot<TStaticFlatTable>(buf);

    const auto& simpleTable = root.GetNested();
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
    EXPECT_EQ(root.GetScalar(), 0xFFULL);
    EXPECT_EQ(root.GetOtherScalar(), 0x5U);

    EXPECT_TRUE(root.HasScalar());
    EXPECT_FALSE(root.HasOtherScalar());
    EXPECT_TRUE(root.HasNested());
}

TEST(FlatTable, ImplicitTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTImplicitTable(yffb, 0, 1, 0));

    EXPECT_EQ(yffb.GetSize(), 18ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<TImplicitTable>(buf);

    EXPECT_EQ(root.GetStringField(), "");
    EXPECT_EQ(root.GetSignedField(), 1LL);
    EXPECT_EQ(root.GetUnsignedField(), 0ULL);
}

TEST(FlatDynamicTable, ImplicitTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, -0x10, 0xFFFF, 0x3333));

    EXPECT_EQ(yffb.GetSize(), 22ULL);

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicTable, ExplicitTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, -0x10, 0xAF,
                                                            0x3333));  // Sets second field to default explicitly;

    EXPECT_EQ(yffb.GetSize(), 23ULL);  // Allocates 1 more byte for presence mask;

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicTable, BadFillOrder) {
    NYaFF::TBuilder yffb;
    yffb.StartFlatTable<TSimpleTable::TMetaType>();
    yffb.AddField<int32_t>(TSimpleTable::ID_SIGNEDFIELD, -0x10, 0);
    EXPECT_THROW(yffb.AddField<uint64_t>(TSimpleTable::ID_LARGEFIELD, 0x3333, 0x6789), std::runtime_error);
}

struct TBadTableMeta {
    static NYaFF::TFieldOffset ResolveField(const NYaFF::TFieldId id) {
        return (id - 1) * 8;
    }
};

TEST(FlatDynamicTable, BadTableSize) {
    NYaFF::TBuilder yffb;
    yffb.StartFlatTable<TBadTableMeta>();
    for (int i = 0; i < 8192; ++i) {
        yffb.AddField<uint64_t>(8192 - i, i, 0xFFFFFFFF);
    }
    EXPECT_THROW(yffb.FinishFlatTable(), std::runtime_error);
}

TEST(FlatDynamicTable, SkipFields) {
    NYaFF::TBuilder yffb;
    TSimpleTableFlatBuilder builder(yffb);
    builder.add_largefield(0x3333);
    builder.add_signedfield(-0x10);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 22ULL);

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicTable, SkipFieldsFront) {
    NYaFF::TBuilder yffb;
    TSimpleTableFlatBuilder builder(yffb);
    builder.add_largefield(0x3333);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 22ULL);

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), 0x0);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
}

TEST(FlatDynamicTable, SkipFieldsTail) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, 0x17));

    EXPECT_EQ(yffb.GetSize(), 10ULL);  // Tail size optimization;

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), 0x17);
}

TEST(FlatDynamicTable, DefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, -0x10, std::nullopt,
                                                            0x1234));  // Sets empty value for small field

    EXPECT_EQ(yffb.GetSize(), 22ULL);

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x1234ULL);
}

TEST(FlatDynamicTable, TailDefaultValues) {
    NYaFF::TBuilder yffb;
    TSimpleTableFlatBuilder builder(yffb);
    builder.add_signedfield(-0x10);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 10ULL);  // Does not store empty values in the end of object;

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicTable, AllDefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb));

    EXPECT_EQ(yffb.GetSize(), 6ULL);  // root offset + flat table typed size;

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), 0x0);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicTable, NestedTables) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, 0, 0xFFFF);
    auto nested2 = CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, 0x17);
    auto root = CreateTNestedTable<TNestedTableFlatBuilder>(yffb, nested1, 0x1234, nested2);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 39ULL);

    const auto& nestedTable = NYaFF::ReadRoot<TNestedTable>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedTable.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedTable.GetNested2().GetSignedField(), 0x17);
    EXPECT_EQ(nestedTable.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(nestedTable.HasNested1() && nestedTable.HasNested2());
}

TEST(FlatDynamicTable, NestedTableEmpty) {
    NYaFF::TBuilder yffb;
    auto nested = CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, 0, 0xAF, 0x5678);
    auto root = CreateTNestedTable<TNestedTableFlatBuilder>(yffb, nested);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 29ULL);

    const auto& nestedTable = NYaFF::ReadRoot<TNestedTable>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedTable.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetLargeField(), 0x5678ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSignedField(), 0x0);
    EXPECT_TRUE(nestedTable.HasNested1() && !nestedTable.HasNested2());
}

TEST(FlatDynamicTable, FloatTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTFloatTable<TFloatTableFlatBuilder>(yffb, 1.1234f, std::nullopt));

    EXPECT_EQ(yffb.GetSize(), 10ULL);  // double empty value is not stored;

    const auto& floatTable = NYaFF::ReadRoot<TFloatTable>(yffb.GetBufferPointer());
    EXPECT_EQ(floatTable.GetFloatField(), 1.1234f);
    EXPECT_EQ(floatTable.GetDoubleField(), 1e-6);
}

TEST(FlatDynamicTable, UnionTableEmpty) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTUnionTable<TUnionTableFlatBuilder>(yffb, 10));
    EXPECT_EQ(yffb.GetSize(), 10ULL);  // shared values is not stored;

    const auto& unionTable = NYaFF::ReadRoot<TUnionTable>(yffb.GetBufferPointer());
    EXPECT_EQ(unionTable.GetSomeValue(), 10U);
    EXPECT_EQ(unionTable.Union_case(), TUnionTable::UnionCase::UNION_NOT_SET);

    EXPECT_EQ(unionTable.HasNested1(), false);
    EXPECT_EQ(unionTable.HasStringVec(), false);
    EXPECT_EQ(unionTable.HasNested2(), false);
    EXPECT_EQ(unionTable.HasMediumField(), false);
    EXPECT_EQ(unionTable.HasLargeField(), false);

    EXPECT_EQ(unionTable.GetNested1().GetLargeField(), 0x6789ULL);
    EXPECT_EQ(unionTable.GetStringVec().Size(), 0U);
    EXPECT_EQ(unionTable.GetMediumField(), -1);
}

TEST(FlatDynamicTable, UnionTableExplicitPresence) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTUnionTable<TUnionTableFlatBuilder>(yffb, 10, 0, 0, 0, -1));  // -1 is default value;

    EXPECT_EQ(yffb.GetSize(), 27ULL);

    const auto& unionTable = NYaFF::ReadRoot<TUnionTable>(yffb.GetBufferPointer());
    EXPECT_EQ(unionTable.GetSomeValue(), 10U);
    EXPECT_EQ(unionTable.Union_case(), TUnionTable::UnionCase::kMediumField);
    EXPECT_EQ(unionTable.GetMediumField(), -1);
    EXPECT_EQ(unionTable.GetNested1().GetLargeField(), 0x6789ULL);
}

TEST(FlatDynamicTable, UnionTableNested) {
    NYaFF::TBuilder yffb;
    auto nested = CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, 10);
    yffb.Finish(CreateTUnionTable<TUnionTableFlatBuilder>(yffb, 12, 0, 0, nested));

    EXPECT_EQ(yffb.GetSize(), 28ULL);

    const auto& unionTable = NYaFF::ReadRoot<TUnionTable>(yffb.GetBufferPointer());
    EXPECT_EQ(unionTable.GetSomeValue(), 12U);
    EXPECT_EQ(unionTable.Union_case(), TUnionTable::UnionCase::kNested2);
    EXPECT_EQ(unionTable.GetNested2().GetSignedField(), 10);
    EXPECT_EQ(unionTable.GetNested1().GetSignedField(), 0);
}

TEST(FlatDynamicTable, UnionTableString) {
    std::string expected = "aaaa";

    NYaFF::TBuilder yffb;
    auto vec = yffb.CreateString(expected);
    yffb.Finish(CreateTUnionTable<TUnionTableFlatBuilder>(yffb, 14, 0, vec));

    EXPECT_EQ(yffb.GetSize(), 27ULL);

    const auto& unionTable = NYaFF::ReadRoot<TUnionTable>(yffb.GetBufferPointer());
    EXPECT_EQ(unionTable.GetSomeValue(), 14U);
    EXPECT_EQ(unionTable.Union_case(), TUnionTable::UnionCase::kStringVec);

    std::string str(unionTable.GetStringVec());
    EXPECT_EQ(str, expected);
}

TEST(FlatDynamicTable, MixedOneof) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTOneOfMix<TOneOfMixFlatBuilder>(yffb, 1, std::nullopt, 2, 4));

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
