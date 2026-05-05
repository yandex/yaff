#include <gtest/gtest.h>
#include <yaff/builder.h>
#include <yaff/table.h>

#include "protoyaff/tables2.yaff.h"

using namespace NProtoYaFF::NTestYaFF;

TEST(DynTable, EmptyRoot) {
    const auto& nestedTable = NYaFF::ReadRoot<TNestedTable>(nullptr);
    EXPECT_EQ(nestedTable.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetLargeField(), 0x6789ULL);
    EXPECT_EQ(nestedTable.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(!nestedTable.HasNested1() && !nestedTable.HasNested2());
}

TEST(DynTable, NestedTableFlatRoot) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, -0x10, 0xFFFF, 0x3333);
    auto nested2 = CreateTSimpleTable<TSimpleTableSparseBuilder>(yffb, std::nullopt, 0xFFFF);
    auto root = CreateTNestedTable<TNestedTableFlatBuilder>(yffb, nested1, 0x1234, nested2);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 52ULL);

    const auto& nestedTable = NYaFF::ReadRoot<TNestedTable>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedTable.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedTable.GetNested1().GetLargeField(), 0x3333ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSignedField(), -0x10);
    EXPECT_EQ(nestedTable.GetNested2().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedTable.GetNested2().GetLargeField(), 0x6789ULL);
}

TEST(DynTable, NestedTableSparseRoot) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleTable<TSimpleTableFlatBuilder>(yffb, -0x10, 0xFFFF, 0x3333);
    auto root = CreateTNestedTable<TNestedTableSparseBuilder>(yffb, nested1);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 33ULL);

    const auto& nestedTable = NYaFF::ReadRoot<TNestedTable>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedTable.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedTable.GetNested1().GetLargeField(), 0x3333ULL);
    EXPECT_EQ(nestedTable.GetNested1().GetSignedField(), -0x10);
}

TEST(DynTable, NestedUnused) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTNestedUnused_TTable(yffb, 123));

    const auto& nestedTable = NYaFF::ReadRoot<TNestedUnused_TTable>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedTable.GetSomeField(), 123);
}

TEST(Vector, EmptyVec) {
    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(nullptr);
    EXPECT_EQ(vectorTable.GetIntegerVec().Size(), 0U);
    EXPECT_EQ(vectorTable.GetStringVec().Size(), 0U);
    EXPECT_EQ(vectorTable.GetTableVec().Size(), 0U);
}

TEST(Vector, BadVectorOffset) {
    NYaFF::TBuilder yffb;
    std::vector<uint8_t> largeBytes(2147483646, 0);  // 2GB - 2B (still not enough because of uint32_t length);
    EXPECT_THROW(yffb.CreateVector(largeBytes), std::runtime_error);
}

TEST(Vector, SimpleVec) {
    std::vector<uint64_t> vec{1, 2, 3, 4};

    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTVectorTable<TVectorTableFlatBuilder>(yffb, yffb.CreateVector(vec)));

    EXPECT_EQ(yffb.GetSize(), 46ULL);

    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& vectorField = vectorTable.GetIntegerVec();
    EXPECT_EQ(vectorField.Size(), 4U);
    EXPECT_TRUE(std::equal(vectorField.begin(), vectorField.end(), vec.begin()));
    for (uint64_t i = 0; i < vectorField.Size(); ++i) {
        EXPECT_EQ(vectorField.Get(i), i + 1);
    }
}

TEST(Vector, StringVec) {
    NYaFF::TBuilder yffb;

    std::vector<std::string> expected;
    std::vector<NYaFF::TInternalOffset<NYaFF::TString>> strings;
    for (uint64_t i = 10; i < 20; ++i) {
        std::string str(i, 'a');
        expected.emplace_back(str);
        strings.emplace_back(yffb.CreateString(str));
    }

    yffb.Finish(CreateTVectorTable<TVectorTableFlatBuilder>(yffb, 0, yffb.CreateVector(strings)));

    EXPECT_EQ(yffb.GetSize(), 253ULL);
    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& stringVectorField = vectorTable.GetStringVec();
    EXPECT_EQ(stringVectorField.Size(), expected.size());
    for (uint64_t i = 0; i < stringVectorField.Size(); ++i) {
        const auto& str = stringVectorField.Get(i);
        EXPECT_EQ(str, expected[i]);
        EXPECT_EQ(str.Size(), expected[i].size());
        EXPECT_TRUE(memcmp(str.Data(), expected[i].data(), str.Size()) == 0);
    }
}

TEST(Vector, TableVec) {
    NYaFF::TBuilder yffb;

    std::vector<NYaFF::TInternalOffset<TFlatTable>> tables;
    for (uint64_t i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            tables.emplace_back(CreateTFlatTable<TFlatTableFlatBuilder>(yffb, std::nullopt, i));
        } else {
            tables.emplace_back(CreateTFlatTable<TFlatTableSparseBuilder>(yffb, std::nullopt, i));
        }
    }
    yffb.Finish(CreateTVectorTable<TVectorTableFlatBuilder>(yffb, 0, 0, yffb.CreateVector(tables)));

    EXPECT_EQ(yffb.GetSize(), 2025ULL);  // Meta should be deduplicated for sparse tables;
    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& tableVectorField = vectorTable.GetTableVec();
    EXPECT_EQ(tableVectorField.Size(), 100U);
    for (uint64_t i = 0; i < tableVectorField.Size(); ++i) {
        EXPECT_EQ(tableVectorField.Get(i).GetSomeValue2(), i);
        EXPECT_EQ(tableVectorField.Get(i).GetSomeValue10(), 0x0ULL);
    }
}

TEST(Vector, FixedTableVec) {
    NYaFF::TBuilder yffb;
    const auto vec =
        yffb.CreateVector<NYaFF::TInlineOffset<NProtoYaFF::NTestYaFF::TStaticFixedTable>>(10, [&](size_t i) {
            const auto nested = CreateTSimpleTable(yffb, i + 1);
            return [=, &yffb]() { return CreateTStaticFixedTable(yffb, i * i, nested); };
        });
    yffb.Finish(CreateTVectorTable(yffb, 0, 0, 0, 0, vec));

    EXPECT_EQ(yffb.GetSize(), 210ULL);

    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& fixedTableVectorField = vectorTable.GetFixedTableVec();
    EXPECT_EQ(fixedTableVectorField.Size(), 10U);
    for (uint64_t i = 0; i < fixedTableVectorField.Size(); ++i) {
        EXPECT_EQ(fixedTableVectorField.Get(i).GetScalar(), i * i);
        EXPECT_EQ(fixedTableVectorField.Get(i).GetNested().GetSignedField(), static_cast<int32_t>(i + 1));
    }
}

TEST(Vector, BoolVec) {
    std::vector<bool> flags;
    for (uint64_t i = 0; i < 10; ++i) {
        flags.emplace_back((i % 3) == 0);
    }

    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTVectorTable<TVectorTableFlatBuilder>(yffb, 0, 0, 0, yffb.CreateVector(flags)));

    EXPECT_EQ(yffb.GetSize(), 36ULL);
    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& boolVectorField = vectorTable.GetBoolVec();
    EXPECT_EQ(boolVectorField.Size(), 10U);
    for (uint64_t i = 0; i < boolVectorField.Size(); ++i) {
        EXPECT_EQ(boolVectorField.Get(i), ((i % 3) == 0));
    }
}

TEST(Vector, VectorDedup) {
    NYaFF::TBuilder yffb;
    std::string expected1 = "aaaabbbb12";
    std::string expected2 = "bbbb4";
    std::vector<NYaFF::TInternalOffset<NYaFF::TString>> strings;
    for (uint64_t i = 0; i < 10; ++i) {
        if (i % 2 == 0) {
            strings.emplace_back(yffb.CreateString(expected1));
        } else {
            strings.emplace_back(yffb.CreateString(expected2));
        }
    }
}

TEST(Vector, VectorDedupByteSize) {
    // Based on SPI-136874. When deduplicating a vector, it is necessary to compare memory regions bytewise,
    // without taking into account Size_ of the vector, since it is in elements not in bytes.

    NYaFF::TBuilder yffb;

    std::string str = "a";
    std::vector<uint64_t> ints = {0xAB0BA61};

    const auto strOffset = yffb.CreateString(str);  // Byte vector of size 1;
    const auto intsOffset = yffb.CreateVector(
        ints);  // Uint64 vector of size 1, where the first byte (little endian) matches the character 'a'.

    std::vector<NYaFF::TInternalOffset<NYaFF::TString>> strings{strOffset};
    yffb.Finish(CreateTVectorTable<TVectorTableFlatBuilder>(yffb, intsOffset, yffb.CreateVector(strings)));

    EXPECT_EQ(yffb.GetSize(), 40ULL);

    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& stringVectorField = vectorTable.GetStringVec();
    EXPECT_EQ(stringVectorField.Size(), 1U);
    EXPECT_EQ(stringVectorField.Get(0).Get(0), 'a');
    const auto& intVectorField = vectorTable.GetIntegerVec();
    EXPECT_EQ(intVectorField.Size(), 1U);
    EXPECT_EQ(intVectorField.Get(0), 0xAB0BA61ULL);
}

TEST(Vector, SimpleIterator) {
    NYaFF::TBuilder yffb;
    yffb.Finish(
        CreateTVectorTable<TVectorTableFlatBuilder>(yffb, yffb.CreateVector(std::vector<uint64_t>{1, 2, 3, 4})));

    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& vectorField = vectorTable.GetIntegerVec();

    uint64_t expected = 1;
    for (const uint64_t v : vectorField) {
        EXPECT_EQ(v, expected);
        ++expected;
    }

    EXPECT_TRUE(std::find(vectorField.begin(), vectorField.end(), 3) != vectorField.end());
    EXPECT_TRUE(std::find(vectorField.begin(), vectorField.end(), 10) == vectorField.end());
}

TEST(Vector, TableIterator) {
    NYaFF::TBuilder yffb;

    std::vector<NYaFF::TInternalOffset<TFlatTable>> tables;
    for (uint64_t i = 0; i < 100; ++i) {
        tables.emplace_back(CreateTFlatTable<TFlatTableFlatBuilder>(yffb, 0, i));
    }
    yffb.Finish(CreateTVectorTable<TVectorTableFlatBuilder>(yffb, 0, 0, yffb.CreateVector(tables)));

    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& tablesField = vectorTable.GetTableVec();

    uint64_t expected = 0;
    for (const auto& table : tablesField) {
        EXPECT_EQ(table.GetSomeValue2(), expected);
        ++expected;
    }

    const auto it = std::lower_bound(tablesField.begin(), tablesField.end(), 10ULL,
                                     [&](const auto& l, const auto& r) { return l.GetSomeValue2() < r; });
    EXPECT_EQ(it->GetSomeValue2(), 10ULL);
}

TEST(Vector, NestedIterator) {
    NYaFF::TBuilder yffb;

    std::vector<std::string> expected;
    std::vector<NYaFF::TInternalOffset<NYaFF::TString>> strings;
    for (uint64_t i = 10; i < 20; ++i) {
        std::string str(i, 'a');
        expected.emplace_back(str);
        strings.emplace_back(yffb.CreateString(str));
    }
    yffb.Finish(CreateTVectorTable<TVectorTableFlatBuilder>(yffb, 0, yffb.CreateVector(strings)));

    const auto& vectorTable = NYaFF::ReadRoot<TVectorTable>(yffb.GetBufferPointer());
    const auto& stringsField = vectorTable.GetStringVec();
    for (size_t strIdx = 0; strIdx < stringsField.size(); ++strIdx) {
        const auto& str = stringsField[strIdx];
        for (size_t chIdx = 0; chIdx < str.size(); ++chIdx) {
            EXPECT_EQ(str[chIdx], expected[strIdx][chIdx]);
        }
    }
}

// TEST(Map, SimpleMap) {
//     NYaFF::TBuilder yffb;
//     const auto mapStruct = TMapTableStruct{
//         .SimpleMap =
//             {
//                 {3, 300},
//                 {0, 0},
//                 {2, 200},
//                 {4, 400},
//                 {1, 100},
//             },
//     };
//     yffb.Finish(TMapTable::CreateFrom(yffb, std::move(mapStruct)));

//     // Map pairs are inlined and use only 8 bytes per <int32_t, int32_t> pair without overhead;
//     EXPECT_EQ(yffb.GetSize(), 54ULL);

//     const auto& mapTable = NYaFF::ReadRoot<TMapTable>(yffb.GetBufferPointer());
//     const auto& simpleMap = mapTable.GetSimpleMap();

//     EXPECT_EQ(simpleMap.Size(), 5U);
//     for (size_t i = 0; i < simpleMap.size(); ++i) {
//         const auto& entity = simpleMap[i];
//         EXPECT_EQ(static_cast<int32_t>(i), entity.key());
//         EXPECT_EQ(static_cast<int32_t>(i * 100), entity.value());

//         const auto it = simpleMap.find(static_cast<int32_t>(i));
//         EXPECT_EQ(it->key(), static_cast<int32_t>(i));
//         EXPECT_EQ(it->value(), static_cast<int32_t>(i * 100));

//         const auto [b, e] = simpleMap.equal_range(static_cast<int32_t>(i));
//         EXPECT_EQ(e - b, 1);
//         EXPECT_EQ(b->key(), static_cast<int32_t>(i));
//         EXPECT_TRUE(e == simpleMap.end() || e->key() == static_cast<int32_t>(i + 1));

//         EXPECT_EQ(simpleMap.count(static_cast<int32_t>(i)), 1ULL);
//         EXPECT_TRUE(simpleMap.contains(static_cast<int32_t>(i)));
//     }

//     const auto tooSmall = simpleMap.find(-1);
//     EXPECT_EQ(tooSmall, simpleMap.end());
//     EXPECT_EQ(simpleMap.count(-1), 0ULL);
//     EXPECT_TRUE(!simpleMap.contains(-1));

//     const auto tooBig = simpleMap.find(100);
//     EXPECT_EQ(tooBig, simpleMap.end());
//     EXPECT_EQ(simpleMap.count(100), 0ULL);
//     EXPECT_TRUE(!simpleMap.contains(100));

//     const auto [tooBugB, tooBigE] = simpleMap.equal_range(1000);
//     EXPECT_EQ(tooBugB, simpleMap.end());
//     EXPECT_EQ(tooBigE, simpleMap.end());
// }

// TEST(Map, CompositeMap) {
//     NYaFF::TBuilder yffb;
//     std::unordered_map<std::string, std::unique_ptr<TMapTable_TCompositeValueStruct>> compositeMapStruct;
//     compositeMapStruct.try_emplace("bbb", std::make_unique<TMapTable_TCompositeValueStruct>(
//                                               TMapTable_TCompositeValueStruct{.SomeField = 2, .AnotherField = 200}));
//     compositeMapStruct.try_emplace("a", std::make_unique<TMapTable_TCompositeValueStruct>(
//                                             TMapTable_TCompositeValueStruct{.SomeField = 0, .AnotherField = 0}));
//     compositeMapStruct.try_emplace("aa", std::make_unique<TMapTable_TCompositeValueStruct>(
//                                              TMapTable_TCompositeValueStruct{.SomeField = 1, .AnotherField = 100}));
//     compositeMapStruct.try_emplace("yaff", std::make_unique<TMapTable_TCompositeValueStruct>(
//                                                TMapTable_TCompositeValueStruct{.SomeField = 3, .AnotherField =
//                                                300}));
//     const auto& expectedKeys = NYaFF::SortedKeys(compositeMapStruct);

//     const auto mapStruct = TMapTableStruct{.CompositeMap = std::move(compositeMapStruct)};
//     yffb.Finish(TMapTable::CreateFrom(yffb, std::move(mapStruct)));

//     EXPECT_EQ(yffb.GetSize(), 124ULL);

//     const auto& mapTable = NYaFF::ReadRoot<TMapTable>(yffb.GetBufferPointer());
//     const auto& compositeMap = mapTable.GetCompositeMap();

//     EXPECT_EQ(compositeMap.Size(), 4U);
//     for (size_t i = 0; i < compositeMap.size(); ++i) {
//         const auto& entity = compositeMap[i];
//         EXPECT_EQ(entity.key(), expectedKeys[i]);
//         EXPECT_EQ(entity.value().GetSomeField(), static_cast<int32_t>(i));
//         EXPECT_EQ(entity.value().GetAnotherField(), static_cast<int64_t>(i * 100));

//         const auto it = compositeMap.find(expectedKeys[i]);
//         EXPECT_EQ(it->key(), expectedKeys[i]);
//         EXPECT_EQ(it->value().GetSomeField(), static_cast<int32_t>(i));

//         EXPECT_EQ(compositeMap.count(expectedKeys[i]), 1ULL);
//         EXPECT_TRUE(compositeMap.contains(expectedKeys[i]));
//     }

//     const auto empty = compositeMap.find("");
//     EXPECT_EQ(empty, compositeMap.end());
//     EXPECT_EQ(compositeMap.count(""), 0ULL);
//     EXPECT_TRUE(!compositeMap.contains(""));

//     const auto flatbuffers = compositeMap.find("flatbuffers");
//     EXPECT_EQ(flatbuffers, compositeMap.end());
//     EXPECT_EQ(compositeMap.count("flatbuffers"), 0ULL);
//     EXPECT_TRUE(!compositeMap.contains("flatbuffers"));
// }
