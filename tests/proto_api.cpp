#include <gtest/gtest.h>

#include <array>
#include <string_view>

#include "protoyaff/proto_api.pb.h"
#include "protoyaff/proto_api.yaff.h"

using namespace std::string_view_literals;

namespace {

test::UniversalMessage GenerateUniversalMessage() {
    test::UniversalMessage proto;
    proto.set_implicit_string_field("");
    proto.set_explicit_string_field_2("");
    proto.set_implicit_bytes_field("");
    proto.set_explicit_bytes_field_2("");
    proto.mutable_embedded_message_field_2();
    for (size_t i = 0; i < 10; ++i) {
        proto.add_repeated_numeric_field(i);
    }
    for (size_t i = 0; i < 5; ++i) {
        proto.add_repeated_string_field(std::to_string(i));
    }
    for (size_t i = 0; i < 15; ++i) {
        proto.add_repeated_bytes_field(std::to_string(i));
    }
    for (size_t i = 0; i < 9; ++i) {
        const auto v = (i % 2 ? test::Enumeration::ENUMERATION_VALUE : test::Enumeration::ENUMERATION_OTHER_VALUE);
        proto.add_repeated_enum_field(v);
    }
    for (size_t i = 0; i < 11; ++i) {
        proto.add_repeated_embedded_message_field();
    }
    proto.set_oneof_string_field("");
    return proto;
}

yaff::DetachedBuffer Convert(const test::UniversalMessage& proto) {
    yaff::Serializer ys;
    ys.Finish(protoyaff::test::SerializeUniversalMessage(ys, proto));
    return ys.Release();
}

test::UniversalMessage Restore(const protoyaff::test::UniversalMessage& yaff) {
    test::UniversalMessage proto;
    yaff.ParseTo(proto);
    return proto;
}

template <typename T>
void CheckImplicitStringFields(const T& obj) {
    EXPECT_EQ(obj.implicit_string_field(), "");
    EXPECT_EQ(obj.implicit_bytes_field(), "");
}

template <typename T>
void CheckExplicitStringFields(const T& obj) {
    EXPECT_FALSE(obj.has_explicit_string_field_1());
    EXPECT_TRUE(obj.has_explicit_string_field_2());
    EXPECT_FALSE(obj.has_explicit_bytes_field_1());
    EXPECT_TRUE(obj.has_explicit_bytes_field_2());

    EXPECT_EQ(obj.explicit_string_field_1(), "");
    EXPECT_EQ(obj.explicit_string_field_2(), std::string{""});
    EXPECT_EQ(obj.explicit_bytes_field_1(), std::string_view{""});
    EXPECT_EQ(obj.explicit_bytes_field_2(), ""sv);
}

template <typename T>
void CheckEmbeddedMessageFields(const T& obj) {
    EXPECT_FALSE(obj.has_embedded_message_field_1());
    EXPECT_TRUE(obj.has_embedded_message_field_2());
}

template <typename T>
void CheckRepeatedNumericFields(const T& obj) {
    EXPECT_EQ(obj.repeated_numeric_field_size(), 10);

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(obj.repeated_numeric_field(i), i);
    }
}

template <typename T>
void CheckRepeatedStringFields(const T& obj) {
    EXPECT_EQ(obj.repeated_string_field_size(), 5);
    EXPECT_EQ(obj.repeated_bytes_field_size(), 15);
}

template <typename T>
void CheckRepeatedEnumFields(const T& obj) {
    EXPECT_EQ(obj.repeated_enum_field_size(), 9);
}

template <typename T>
void CheckRepeatedEmbeddedMessageFields(const T& obj) {
    EXPECT_EQ(obj.repeated_embedded_message_field_size(), 11);
}

template <typename T>
void CheckOneofNumericFields(const T& obj) {
    EXPECT_FALSE(obj.has_oneof_numeric_field());
}

template <typename T>
void CheckOneofStringFields(const T& obj) {
    EXPECT_TRUE(obj.has_oneof_string_field());
    EXPECT_FALSE(obj.has_oneof_bytes_field());
}

template <typename T>
void CheckOneofEnumFields(const T& obj) {
    EXPECT_FALSE(obj.has_oneof_enum_field());
}

template <typename T>
void CheckOneofEmbeddedMessageFields(const T& obj) {
    EXPECT_FALSE(obj.has_oneof_embedded_message_field());
}

}  // namespace

TEST(ProtoAPI, TemplateTest) {
    const auto& originalProto = GenerateUniversalMessage();

    const auto buffer = Convert(originalProto);
    const auto& yaff = yaff::ReadRoot<protoyaff::test::UniversalMessage>(buffer.Data());

    const auto& restoredProto = Restore(yaff);

    CheckExplicitStringFields(originalProto);
    CheckExplicitStringFields(yaff);
    CheckExplicitStringFields(restoredProto);

    CheckImplicitStringFields(originalProto);
    CheckImplicitStringFields(yaff);
    CheckImplicitStringFields(restoredProto);

    CheckEmbeddedMessageFields(originalProto);
    CheckEmbeddedMessageFields(yaff);
    CheckEmbeddedMessageFields(restoredProto);

    CheckRepeatedNumericFields(originalProto);
    CheckRepeatedNumericFields(yaff);
    CheckRepeatedNumericFields(restoredProto);

    CheckRepeatedStringFields(originalProto);
    CheckRepeatedStringFields(yaff);
    CheckRepeatedStringFields(restoredProto);

    CheckRepeatedEnumFields(originalProto);
    CheckRepeatedEnumFields(yaff);
    CheckRepeatedEnumFields(restoredProto);

    CheckRepeatedEmbeddedMessageFields(originalProto);
    CheckRepeatedEmbeddedMessageFields(yaff);
    CheckRepeatedEmbeddedMessageFields(restoredProto);

    CheckOneofNumericFields(originalProto);
    CheckOneofNumericFields(yaff);
    CheckOneofNumericFields(restoredProto);

    CheckOneofStringFields(originalProto);
    CheckOneofStringFields(yaff);
    CheckOneofStringFields(restoredProto);

    CheckOneofEnumFields(originalProto);
    CheckOneofEnumFields(yaff);
    CheckOneofEnumFields(restoredProto);

    CheckOneofEmbeddedMessageFields(originalProto);
    CheckOneofEmbeddedMessageFields(yaff);
    CheckOneofEmbeddedMessageFields(restoredProto);
}

TEST(ProtoAPI, Enumerations) {
    const std::array<std::pair<protoyaff::test::Enumeration, test::Enumeration>, 4> testSet = {
        std::pair{protoyaff::test::Enumeration::ENUMERATION_UNSPECIFIED, test::Enumeration::ENUMERATION_UNSPECIFIED},
        std::pair{protoyaff::test::Enumeration::ENUMERATION_VALUE, test::Enumeration::ENUMERATION_VALUE},
        std::pair{protoyaff::test::Enumeration::ENUMERATION_OTHER_VALUE, test::Enumeration::ENUMERATION_OTHER_VALUE},
        std::pair{protoyaff::test::Enumeration::auto_, test::Enumeration::auto_}};

    for (auto [v1, v2] : testSet) {
        auto n1 = protoyaff::test::Enumeration_Name(v1);
        auto n2 = test::Enumeration_Name(v2);
        EXPECT_EQ(n1, n2);

        protoyaff::test::Enumeration r1;
        test::Enumeration r2;
        EXPECT_TRUE(protoyaff::test::Enumeration_Parse(n1, &r1));
        EXPECT_TRUE(test::Enumeration_Parse(n2, &r2));
        EXPECT_EQ(static_cast<int>(r1), static_cast<int>(r2));
    }

    EXPECT_EQ(static_cast<int>(protoyaff::test::Enumeration_MIN), static_cast<int>(test::Enumeration_MIN));
    EXPECT_EQ(static_cast<int>(protoyaff::test::Enumeration_MAX), static_cast<int>(test::Enumeration_MAX));
    EXPECT_EQ(static_cast<int>(protoyaff::test::Enumeration_ARRAYSIZE), static_cast<int>(test::Enumeration_ARRAYSIZE));

    EXPECT_EQ(protoyaff::test::Enumeration_IsValid(2), test::Enumeration_IsValid(2));
    EXPECT_EQ(protoyaff::test::Enumeration_IsValid(9000), test::Enumeration_IsValid(9000));
}
