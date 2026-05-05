#include <gtest/gtest.h>
#include <yaff/builder.h>
#include <yaff/table.h>

#include "protoyaff/tables2.yaff.h"
#include "protoyaff/tables3.yaff.h"

using namespace NProtoYaFF::NTestYaFF;

TEST(SparseTable, NestedTable) {
    NYaFF::TBuilder yffb;
    const auto nested = CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, -0x10, 0xFFFF, 0x3333);
    yffb.Finish(CreateTStaticSparseTable(yffb, 0xFF, nested));

    EXPECT_EQ(yffb.GetSize(), 49ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<TStaticSparseTable>(buf);

    const auto& simpleTable = root.GetNested();
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
    EXPECT_EQ(root.GetScalar(), 0xFFULL);
}

TEST(SparseTable, ImplicitTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTImplicitTable<TImplicitTableSparseBuilder>(yffb, 0, 1, 0));

    EXPECT_EQ(yffb.GetSize(), 20ULL);

    const auto* buf = yffb.GetBufferPointer();
    const auto& root = NYaFF::ReadRoot<TImplicitTable>(buf);

    EXPECT_EQ(root.GetStringField(), "");
    EXPECT_EQ(root.GetSignedField(), 1LL);
    EXPECT_EQ(root.GetUnsignedField(), 0ULL);
}

TEST(SparseDynamicTable, SimpleTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, -0x10, 0xFFFF, 0x3333));

    EXPECT_EQ(yffb.GetSize(), 29ULL);

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xFFFFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicTable, BadId) {
    NYaFF::TBuilder yffb;
    yffb.StartSparseTable();
    EXPECT_THROW(yffb.AddField<int32_t>(0, -0x10, 0), std::runtime_error);
}

TEST(SparseDynamicTable, BadFillOrder) {
    NYaFF::TBuilder yffb;
    yffb.StartSparseTable();
    yffb.AddField<uint16_t>(TSimpleTable::ID_SIGNEDFIELD, 0xFFFF, 0xAF);
    EXPECT_THROW(yffb.AddField<uint64_t>(TSimpleTable::ID_LARGEFIELD, 0x3333, 0x6789), std::runtime_error);
}

TEST(SparseDynamicTable, BadTableOffset) {
    NYaFF::TBuilder yffb;
    std::vector<uint8_t> largeBytes(2147483638, 0);  // 2GB - 10B;
    const auto vectorOffset = yffb.CreateVector(largeBytes);

    yffb.StartSparseTable();
    yffb.AddField(1, vectorOffset);
    EXPECT_THROW(yffb.FinishSparseTable(), std::runtime_error);
}

TEST(SparseDynamicTable, SkipFields) {
    NYaFF::TBuilder yffb;
    TSimpleTableSparseBuilder builder(yffb);
    builder.add_largefield(0x3333);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 21ULL);  // Sparse table does not store skipped fields on wire.

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), 0x0);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicTable, SkipFieldsTail) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, 0x17));

    EXPECT_EQ(yffb.GetSize(), 15ULL);  // Meta tail size optimization;

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), 0x17);
}

TEST(SparseDynamicTable, DefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, -0x10, std::nullopt, 0x3333));

    EXPECT_EQ(yffb.GetSize(), 25ULL);  // Message is smaller, because empty value does not stored on wire;

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x3333ULL);
}

TEST(SparseDynamicTable, TailDefaultValues) {
    NYaFF::TBuilder yffb;
    TSimpleTableSparseBuilder builder(yffb);
    builder.add_signedfield(-0x10);
    auto root = std::move(builder).Finish();
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 15ULL);

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), -0x10);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicTable, AllDefaultValues) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb));

    EXPECT_EQ(yffb.GetSize(), 6ULL);  // root offset (4) + typed limit (2)

    const auto& simpleTable = NYaFF::ReadRoot<TSimpleTable>(yffb.GetBufferPointer());
    EXPECT_EQ(simpleTable.GetSignedField(), 0x0);
    EXPECT_EQ(simpleTable.GetSmallField(), 0xAFU);
    EXPECT_EQ(simpleTable.GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicTable, NestedTables) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, std::nullopt, 0xFFFF);
    auto nested2 = CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, 0x17);
    auto root = CreateTNestedTable<TNestedTableSparseBuilder>(yffb, nested1, 0x1234, nested2);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 52ULL);

    const auto& nestedTable = NYaFF::ReadRoot<TNestedTable>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedTable.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedTable.GetNested2().GetSignedField(), 0x17);
    EXPECT_EQ(nestedTable.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(nestedTable.HasNested1() && nestedTable.HasNested2());
}

TEST(SparseDynamicTable, NestedTableEmpty) {
    NYaFF::TBuilder yffb;
    auto nested = CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, std::nullopt, std::nullopt, 0x5678);
    auto root = CreateTNestedTable<TNestedTableSparseBuilder>(yffb, nested);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 32ULL);

    const auto& nestedTable = NYaFF::ReadRoot<TNestedTable>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedTable.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetLargeField(), 0x5678ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSignedField(), 0x0);
    EXPECT_TRUE(nestedTable.HasNested1() && !nestedTable.HasNested2());
}

TEST(SparseDynamicTable, MetaDeduplication) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, std::nullopt, 0x20);
    auto nested2 = CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, std::nullopt, 0x10);
    auto root = CreateTNestedTable<TNestedTableSparseBuilder>(yffb, nested1, std::nullopt, nested2);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 43ULL);  // because nested2 meta is deduplicated;

    const auto& nestedTable = NYaFF::ReadRoot<TNestedTable>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedTable.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSmallField(), 0x20U);
    EXPECT_EQ(nestedTable.GetNested2().GetSmallField(), 0x10U);
}

TEST(SparseDynamicTable, FloatTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTFloatTable<TFloatTableSparseBuilder>(yffb, 1.1234f, std::nullopt));

    EXPECT_EQ(yffb.GetSize(), 15ULL);

    const auto& floatTable = NYaFF::ReadRoot<TFloatTable>(yffb.GetBufferPointer());
    EXPECT_EQ(floatTable.GetFloatField(), 1.1234f);
    EXPECT_EQ(floatTable.GetDoubleField(), 1e-6);
}

TEST(SparseDynamicTable, UnionTableEmpty) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTUnionTable<TUnionTableSparseBuilder>(yffb, 10));

    EXPECT_EQ(yffb.GetSize(), 15ULL);

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

TEST(SparseDynamicTable, UnionTableExplicitPresence) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTUnionTable<TUnionTableSparseBuilder>(yffb, 10, 0, 0, 0, -1));  // -1 is default value;

    EXPECT_EQ(yffb.GetSize(), 23ULL);

    const auto& unionTable = NYaFF::ReadRoot<TUnionTable>(yffb.GetBufferPointer());
    EXPECT_EQ(unionTable.GetSomeValue(), 10U);
    EXPECT_EQ(unionTable.Union_case(), TUnionTable::UnionCase::kMediumField);
    EXPECT_EQ(unionTable.GetMediumField(), -1);
    EXPECT_EQ(unionTable.GetNested1().GetLargeField(), 0x6789ULL);
}

TEST(SparseDynamicTable, UnionTableNested) {
    NYaFF::TBuilder yffb;
    auto nested = CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, 10);
    yffb.Finish(CreateTUnionTable<TUnionTableSparseBuilder>(yffb, 12, 0, 0, nested));

    EXPECT_EQ(yffb.GetSize(), 33ULL);

    const auto& unionTable = NYaFF::ReadRoot<TUnionTable>(yffb.GetBufferPointer());
    EXPECT_EQ(unionTable.GetSomeValue(), 12U);
    EXPECT_EQ(unionTable.Union_case(), TUnionTable::UnionCase::kNested2);
    EXPECT_EQ(unionTable.GetNested2().GetSignedField(), 10);
    EXPECT_EQ(unionTable.GetNested1().GetSignedField(), 0);
}

TEST(SparseDynamicTable, MixedOneof) {
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

TEST(SparseDynamicTable, GappedTable) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTGappedTable(yffb, 1, std::nullopt, 5));

    EXPECT_EQ(yffb.GetSize(), 31ULL);

    const auto& gappedTable = NYaFF::ReadRoot<TGappedTable>(yffb.GetBufferPointer());
    EXPECT_EQ(gappedTable.GetField1(), 1ULL);
    EXPECT_EQ(gappedTable.GetField3(), 0ULL);
    EXPECT_EQ(gappedTable.GetField5(), 5ULL);
}
