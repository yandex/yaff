#include <gtest/gtest.h>
#include <yaff/builder.h>
#include <yaff/message.h>

#include "protoyaff/proto2.yaff.h"

using namespace NProtoYaFF::NTestYaFF;

TEST(DynMessage, EmptyRoot) {
    const auto& nestedMessage = NYaFF::ReadRoot<TNestedMessage>(nullptr);
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x6789ULL);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(!nestedMessage.HasNested1() && !nestedMessage.HasNested2());
}

TEST(DynMessage, NestedMessageFlatRoot) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, -0x10, 0xFFFF, 0x3333);
    auto nested2 = CreateTSimpleMessage<TSimpleMessageSparseBuilder>(yffb, std::nullopt, 0xFFFF);
    auto root = CreateTNestedMessage<TNestedMessageFlatBuilder>(yffb, nested1, 0x1234, nested2);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 54ULL);

    const auto& nestedMessage = NYaFF::ReadRoot<TNestedMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x3333ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSignedField(), -0x10);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested2().GetLargeField(), 0x6789ULL);
}

TEST(DynMessage, NestedMessageSparseRoot) {
    NYaFF::TBuilder yffb;
    auto nested1 = CreateTSimpleMessage<TSimpleMessageFlatBuilder>(yffb, -0x10, 0xFFFF, 0x3333);
    auto root = CreateTNestedMessage<TNestedMessageSparseBuilder>(yffb, nested1);
    yffb.Finish(root);

    EXPECT_EQ(yffb.GetSize(), 34ULL);

    const auto& nestedMessage = NYaFF::ReadRoot<TNestedMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x3333ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSignedField(), -0x10);
}

TEST(DynMessage, NestedUnused) {
    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTNestedUnused_TMessage(yffb, 123));

    const auto& nestedMessage = NYaFF::ReadRoot<TNestedUnused_TMessage>(yffb.GetBufferPointer());
    EXPECT_EQ(nestedMessage.GetSomeField(), 123);
}

TEST(Vector, EmptyVec) {
    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(nullptr);
    EXPECT_EQ(vectorMessage.GetIntegerVec().Size(), 0U);
    EXPECT_EQ(vectorMessage.GetStringVec().Size(), 0U);
    EXPECT_EQ(vectorMessage.GetMessageVec().Size(), 0U);
}

TEST(Vector, BadVectorOffset) {
    NYaFF::TBuilder yffb;
    std::vector<uint8_t> largeBytes(2147483646, 0);  // 2GB - 2B (still not enough because of uint32_t length);
    EXPECT_THROW(yffb.CreateVector(largeBytes), std::runtime_error);
}

TEST(Vector, SimpleVec) {
    std::vector<uint64_t> vec{1, 2, 3, 4};

    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTVectorMessage<TVectorMessageFlatBuilder>(yffb, yffb.CreateVector(vec)));

    EXPECT_EQ(yffb.GetSize(), 47ULL);

    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& vectorField = vectorMessage.GetIntegerVec();
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

    yffb.Finish(CreateTVectorMessage<TVectorMessageFlatBuilder>(yffb, 0, yffb.CreateVector(strings)));

    EXPECT_EQ(yffb.GetSize(), 254ULL);
    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& stringVectorField = vectorMessage.GetStringVec();
    EXPECT_EQ(stringVectorField.Size(), expected.size());
    for (uint64_t i = 0; i < stringVectorField.Size(); ++i) {
        const auto& str = stringVectorField.Get(i);
        EXPECT_EQ(str, expected[i]);
        EXPECT_EQ(str.Size(), expected[i].size());
        EXPECT_TRUE(memcmp(str.Data(), expected[i].data(), str.Size()) == 0);
    }
}

TEST(Vector, MessageVec) {
    NYaFF::TBuilder yffb;

    std::vector<NYaFF::TInternalOffset<TFlatMessage>> messages;
    for (uint64_t i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            messages.emplace_back(CreateTFlatMessage<TFlatMessageFlatBuilder>(yffb, std::nullopt, i));
        } else {
            messages.emplace_back(CreateTFlatMessage<TFlatMessageSparseBuilder>(yffb, std::nullopt, i));
        }
    }
    yffb.Finish(CreateTVectorMessage<TVectorMessageFlatBuilder>(yffb, 0, 0, yffb.CreateVector(messages)));

    EXPECT_EQ(yffb.GetSize(), 2075ULL);  // Meta should be deduplicated for sparse messages;
    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& messageVectorField = vectorMessage.GetMessageVec();
    EXPECT_EQ(messageVectorField.Size(), 100U);
    for (uint64_t i = 0; i < messageVectorField.Size(); ++i) {
        EXPECT_EQ(messageVectorField.Get(i).GetSomeValue2(), i);
        EXPECT_EQ(messageVectorField.Get(i).GetSomeValue10(), 0x0ULL);
    }
}

TEST(Vector, FixedMessageVec) {
    NYaFF::TBuilder yffb;
    const auto vec =
        yffb.CreateVector<NYaFF::TInlineOffset<NProtoYaFF::NTestYaFF::TStaticFixedMessage>>(10, [&](size_t i) {
            const auto nested = CreateTSimpleMessage(yffb, i + 1);
            return [=, &yffb]() { return CreateTStaticFixedMessage(yffb, i * i, nested); };
        });
    yffb.Finish(CreateTVectorMessage(yffb, 0, 0, 0, 0, vec));

    EXPECT_EQ(yffb.GetSize(), 222ULL);

    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& fixedMessageVectorField = vectorMessage.GetFixedMessageVec();
    EXPECT_EQ(fixedMessageVectorField.Size(), 10U);
    for (uint64_t i = 0; i < fixedMessageVectorField.Size(); ++i) {
        EXPECT_EQ(fixedMessageVectorField.Get(i).GetScalar(), i * i);
        EXPECT_EQ(fixedMessageVectorField.Get(i).GetNested().GetSignedField(), static_cast<int32_t>(i + 1));
    }
}

TEST(Vector, BoolVec) {
    std::vector<bool> flags;
    for (uint64_t i = 0; i < 10; ++i) {
        flags.emplace_back((i % 3) == 0);
    }

    NYaFF::TBuilder yffb;
    yffb.Finish(CreateTVectorMessage<TVectorMessageFlatBuilder>(yffb, 0, 0, 0, yffb.CreateVector(flags)));

    EXPECT_EQ(yffb.GetSize(), 37ULL);
    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& boolVectorField = vectorMessage.GetBoolVec();
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
    yffb.Finish(CreateTVectorMessage<TVectorMessageFlatBuilder>(yffb, intsOffset, yffb.CreateVector(strings)));

    EXPECT_EQ(yffb.GetSize(), 41ULL);

    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& stringVectorField = vectorMessage.GetStringVec();
    EXPECT_EQ(stringVectorField.Size(), 1U);
    EXPECT_EQ(stringVectorField.Get(0).Get(0), 'a');
    const auto& intVectorField = vectorMessage.GetIntegerVec();
    EXPECT_EQ(intVectorField.Size(), 1U);
    EXPECT_EQ(intVectorField.Get(0), 0xAB0BA61ULL);
}

TEST(Vector, SimpleIterator) {
    NYaFF::TBuilder yffb;
    yffb.Finish(
        CreateTVectorMessage<TVectorMessageFlatBuilder>(yffb, yffb.CreateVector(std::vector<uint64_t>{1, 2, 3, 4})));

    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& vectorField = vectorMessage.GetIntegerVec();

    uint64_t expected = 1;
    for (const uint64_t v : vectorField) {
        EXPECT_EQ(v, expected);
        ++expected;
    }

    EXPECT_TRUE(std::find(vectorField.begin(), vectorField.end(), 3) != vectorField.end());
    EXPECT_TRUE(std::find(vectorField.begin(), vectorField.end(), 10) == vectorField.end());
}

TEST(Vector, MessageIterator) {
    NYaFF::TBuilder yffb;

    std::vector<NYaFF::TInternalOffset<TFlatMessage>> messages;
    for (uint64_t i = 0; i < 100; ++i) {
        messages.emplace_back(CreateTFlatMessage<TFlatMessageFlatBuilder>(yffb, 0, i));
    }
    yffb.Finish(CreateTVectorMessage<TVectorMessageFlatBuilder>(yffb, 0, 0, yffb.CreateVector(messages)));

    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& messagesField = vectorMessage.GetMessageVec();

    uint64_t expected = 0;
    for (const auto& message : messagesField) {
        EXPECT_EQ(message.GetSomeValue2(), expected);
        ++expected;
    }

    const auto it = std::lower_bound(messagesField.begin(), messagesField.end(), 10ULL,
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
    yffb.Finish(CreateTVectorMessage<TVectorMessageFlatBuilder>(yffb, 0, yffb.CreateVector(strings)));

    const auto& vectorMessage = NYaFF::ReadRoot<TVectorMessage>(yffb.GetBufferPointer());
    const auto& stringsField = vectorMessage.GetStringVec();
    for (size_t strIdx = 0; strIdx < stringsField.size(); ++strIdx) {
        const auto& str = stringsField[strIdx];
        for (size_t chIdx = 0; chIdx < str.size(); ++chIdx) {
            EXPECT_EQ(str[chIdx], expected[strIdx][chIdx]);
        }
    }
}
