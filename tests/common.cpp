#include <gtest/gtest.h>
#include <yaff/message.h>
#include <yaff/serializer.h>

#include "protoyaff/proto2.yaff.h"

using namespace protoyaff::test;

TEST(DynMessage, EmptyRoot) {
    const auto& nestedMessage = yaff::ReadRoot<NestedMessage>(nullptr);
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x6789ULL);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0xAFU);
    EXPECT_TRUE(!nestedMessage.HasNested1() && !nestedMessage.HasNested2());
}

TEST(DynMessage, NestedMessageFlatRoot) {
    yaff::Serializer ys;
    auto nested1 = SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, -0x10, 0xFFFF, 0x3333);
    auto nested2 = SerializeSimpleMessage<SimpleMessageSparseSerializer>(ys, std::nullopt, 0xFFFF);
    auto root = SerializeNestedMessage<NestedMessageFlatSerializer>(ys, nested1, 0x1234, nested2);
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 54ULL);

    const auto& nestedMessage = yaff::ReadRoot<NestedMessage>(ys.Data());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x1234ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x3333ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSignedField(), -0x10);
    EXPECT_EQ(nestedMessage.GetNested2().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested2().GetLargeField(), 0x6789ULL);
}

TEST(DynMessage, NestedMessageSparseRoot) {
    yaff::Serializer ys;
    auto nested1 = SerializeSimpleMessage<SimpleMessageFlatSerializer>(ys, -0x10, 0xFFFF, 0x3333);
    auto root = SerializeNestedMessage<NestedMessageSparseSerializer>(ys, nested1);
    ys.Finish(root);

    EXPECT_EQ(ys.Size(), 34ULL);

    const auto& nestedMessage = yaff::ReadRoot<NestedMessage>(ys.Data());
    EXPECT_EQ(nestedMessage.GetScalarField(), 0x0ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSmallField(), 0xFFFFU);
    EXPECT_EQ(nestedMessage.GetNested1().GetLargeField(), 0x3333ULL);
    EXPECT_EQ(nestedMessage.GetNested1().GetSignedField(), -0x10);
}

TEST(DynMessage, NestedUnused) {
    yaff::Serializer ys;
    ys.Finish(SerializeNestedUnused_TMessage(ys, 123));

    const auto& nestedMessage = yaff::ReadRoot<NestedUnused_TMessage>(ys.Data());
    EXPECT_EQ(nestedMessage.GetSomeField(), 123);
}

TEST(Array, EmptyVec) {
    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(nullptr);
    EXPECT_EQ(arrayMessage.GetIntegerVec().Size(), 0U);
    EXPECT_EQ(arrayMessage.GetStringVec().Size(), 0U);
    EXPECT_EQ(arrayMessage.GetMessageVec().Size(), 0U);
}

TEST(Array, BadArrayOffset) {
    yaff::Serializer ys;
    std::vector<uint8_t> largeBytes(2147483646, 0);  // 2GB - 2B (still not enough because of uint32_t length);
    EXPECT_THROW(ys.SerializeArray(largeBytes), std::runtime_error);
}

TEST(Array, SimpleVec) {
    std::vector<uint64_t> vec{1, 2, 3, 4};

    yaff::Serializer ys;
    ys.Finish(SerializeArrayMessage<ArrayMessageFlatSerializer>(ys, ys.SerializeArray(vec)));

    EXPECT_EQ(ys.Size(), 47ULL);

    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& arrayFeild = arrayMessage.GetIntegerVec();
    EXPECT_EQ(arrayFeild.Size(), 4U);
    EXPECT_TRUE(std::equal(arrayFeild.begin(), arrayFeild.end(), vec.begin()));
    for (uint64_t i = 0; i < arrayFeild.Size(); ++i) {
        EXPECT_EQ(arrayFeild.Get(i), i + 1);
    }
}

TEST(Array, StringVec) {
    yaff::Serializer ys;

    std::vector<std::string> expected;
    std::vector<yaff::InternalOffset<yaff::String>> strings;
    for (uint64_t i = 10; i < 20; ++i) {
        std::string str(i, 'a');
        expected.emplace_back(str);
        strings.emplace_back(ys.SerializeString(str));
    }

    ys.Finish(SerializeArrayMessage<ArrayMessageFlatSerializer>(ys, 0, ys.SerializeArray(strings)));

    EXPECT_EQ(ys.Size(), 254ULL);
    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& stringArrayField = arrayMessage.GetStringVec();
    EXPECT_EQ(stringArrayField.Size(), expected.size());
    for (uint64_t i = 0; i < stringArrayField.Size(); ++i) {
        const auto& str = stringArrayField.Get(i);
        EXPECT_EQ(str, expected[i]);
        EXPECT_EQ(str.Size(), expected[i].size());
        EXPECT_TRUE(memcmp(str.Data(), expected[i].data(), str.Size()) == 0);
    }
}

TEST(Array, MessageVec) {
    yaff::Serializer ys;

    std::vector<yaff::InternalOffset<FlatMessage>> messages;
    for (uint64_t i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            messages.emplace_back(SerializeFlatMessage<FlatMessageFlatSerializer>(ys, std::nullopt, i));
        } else {
            messages.emplace_back(SerializeFlatMessage<FlatMessageSparseSerializer>(ys, std::nullopt, i));
        }
    }
    ys.Finish(SerializeArrayMessage<ArrayMessageFlatSerializer>(ys, 0, 0, ys.SerializeArray(messages)));

    EXPECT_EQ(ys.Size(), 2075ULL);  // Meta should be deduplicated for sparse messages;
    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& messageArrayField = arrayMessage.GetMessageVec();
    EXPECT_EQ(messageArrayField.Size(), 100U);
    for (uint64_t i = 0; i < messageArrayField.Size(); ++i) {
        EXPECT_EQ(messageArrayField.Get(i).GetSomeValue2(), i);
        EXPECT_EQ(messageArrayField.Get(i).GetSomeValue10(), 0x0ULL);
    }
}

TEST(Array, FixedMessageVec) {
    yaff::Serializer ys;
    const auto vec = ys.SerializeArray<yaff::InlineOffset<protoyaff::test::StaticFixedMessage>>(10, [&](size_t i) {
        const auto nested = SerializeSimpleMessage(ys, i + 1);
        return [=, &ys]() { return SerializeStaticFixedMessage(ys, i * i, nested); };
    });
    ys.Finish(SerializeArrayMessage(ys, 0, 0, 0, 0, vec));

    EXPECT_EQ(ys.Size(), 222ULL);

    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& fixedMessageArrayField = arrayMessage.GetFixedMessageVec();
    EXPECT_EQ(fixedMessageArrayField.Size(), 10U);
    for (uint64_t i = 0; i < fixedMessageArrayField.Size(); ++i) {
        EXPECT_EQ(fixedMessageArrayField.Get(i).GetScalar(), i * i);
        EXPECT_EQ(fixedMessageArrayField.Get(i).GetNested().GetSignedField(), static_cast<int32_t>(i + 1));
    }
}

TEST(Array, BoolVec) {
    std::vector<bool> flags;
    for (uint64_t i = 0; i < 10; ++i) {
        flags.emplace_back((i % 3) == 0);
    }

    yaff::Serializer ys;
    ys.Finish(SerializeArrayMessage<ArrayMessageFlatSerializer>(ys, 0, 0, 0, ys.SerializeArray(flags)));

    EXPECT_EQ(ys.Size(), 37ULL);
    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& boolArrayField = arrayMessage.GetBoolVec();
    EXPECT_EQ(boolArrayField.Size(), 10U);
    for (uint64_t i = 0; i < boolArrayField.Size(); ++i) {
        EXPECT_EQ(boolArrayField.Get(i), ((i % 3) == 0));
    }
}

TEST(Array, ArrayDedup) {
    yaff::Serializer ys;
    std::string expected1 = "aaaabbbb12";
    std::string expected2 = "bbbb4";
    std::vector<yaff::InternalOffset<yaff::String>> strings;
    for (uint64_t i = 0; i < 10; ++i) {
        if (i % 2 == 0) {
            strings.emplace_back(ys.SerializeString(expected1));
        } else {
            strings.emplace_back(ys.SerializeString(expected2));
        }
    }
}

TEST(Array, ArrayDedupByteSize) {
    // Based on SPI-136874. When deduplicating a array, it is necessary to compare memory regions bytewise,
    // without taking into account Size_ of the array, since it is in elements not in bytes.

    yaff::Serializer ys;

    std::string str = "a";
    std::vector<uint64_t> ints = {0xAB0BA61};

    // Byte vector of size 1;
    const auto strOffset = ys.SerializeString(str);
    // Uint64 array of size 1, where the first byte (little endian) matches the character 'a'.
    const auto intsOffset = ys.SerializeArray(ints);

    std::vector<yaff::InternalOffset<yaff::String>> strings{strOffset};
    ys.Finish(SerializeArrayMessage<ArrayMessageFlatSerializer>(ys, intsOffset, ys.SerializeArray(strings)));

    EXPECT_EQ(ys.Size(), 41ULL);

    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& stringArrayField = arrayMessage.GetStringVec();
    EXPECT_EQ(stringArrayField.Size(), 1U);
    EXPECT_EQ(stringArrayField.Get(0).Get(0), 'a');
    const auto& intArrayField = arrayMessage.GetIntegerVec();
    EXPECT_EQ(intArrayField.Size(), 1U);
    EXPECT_EQ(intArrayField.Get(0), 0xAB0BA61ULL);
}

TEST(Array, SimpleIterator) {
    yaff::Serializer ys;
    const auto array = ys.SerializeArray(std::vector<uint64_t>{1, 2, 3, 4});
    ys.Finish(SerializeArrayMessage<ArrayMessageFlatSerializer>(ys, array));

    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& arrayField = arrayMessage.GetIntegerVec();

    uint64_t expected = 1;
    for (const uint64_t v : arrayField) {
        EXPECT_EQ(v, expected);
        ++expected;
    }

    EXPECT_TRUE(std::find(arrayField.begin(), arrayField.end(), 3) != arrayField.end());
    EXPECT_TRUE(std::find(arrayField.begin(), arrayField.end(), 10) == arrayField.end());
}

TEST(Array, MessageIterator) {
    yaff::Serializer ys;

    std::vector<yaff::InternalOffset<FlatMessage>> messages;
    for (uint64_t i = 0; i < 100; ++i) {
        messages.emplace_back(SerializeFlatMessage<FlatMessageFlatSerializer>(ys, 0, i));
    }
    ys.Finish(SerializeArrayMessage<ArrayMessageFlatSerializer>(ys, 0, 0, ys.SerializeArray(messages)));

    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& messagesField = arrayMessage.GetMessageVec();

    uint64_t expected = 0;
    for (const auto& message : messagesField) {
        EXPECT_EQ(message.GetSomeValue2(), expected);
        ++expected;
    }

    const auto it = std::lower_bound(messagesField.begin(), messagesField.end(), 10ULL,
                                     [&](const auto& l, const auto& r) { return l.GetSomeValue2() < r; });
    EXPECT_EQ(it->GetSomeValue2(), 10ULL);
}

TEST(Array, NestedIterator) {
    yaff::Serializer ys;

    std::vector<std::string> expected;
    std::vector<yaff::InternalOffset<yaff::String>> strings;
    for (uint64_t i = 10; i < 20; ++i) {
        std::string str(i, 'a');
        expected.emplace_back(str);
        strings.emplace_back(ys.SerializeString(str));
    }
    ys.Finish(SerializeArrayMessage<ArrayMessageFlatSerializer>(ys, 0, ys.SerializeArray(strings)));

    const auto& arrayMessage = yaff::ReadRoot<ArrayMessage>(ys.Data());
    const auto& stringsField = arrayMessage.GetStringVec();
    for (size_t strIdx = 0; strIdx < stringsField.size(); ++strIdx) {
        const auto& str = stringsField[strIdx];
        for (size_t chIdx = 0; chIdx < str.size(); ++chIdx) {
            EXPECT_EQ(str[chIdx], expected[strIdx][chIdx]);
        }
    }
}
