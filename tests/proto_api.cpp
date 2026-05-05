#include <gtest/gtest.h>

#include <array>
#include <string_view>

#include "protoyaff/proto_api.pb.h"
#include "protoyaff/proto_api.yaff.h"

using namespace std::string_view_literals;

namespace {

NTestYaFF::TUniversalMessage GenerateUniversalMessage() {
    NTestYaFF::TUniversalMessage proto;
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
        const auto v =
            (i % 2 ? NTestYaFF::EEnumeration::ENUMERATION_VALUE : NTestYaFF::EEnumeration::ENUMERATION_OTHER_VALUE);
        proto.add_repeated_enum_field(v);
    }
    for (size_t i = 0; i < 11; ++i) {
        proto.add_repeated_embedded_message_field();
    }
    proto.set_oneof_string_field("");
    return proto;
}

NYaFF::TDetachedBuffer Convert(const NTestYaFF::TUniversalMessage& proto) {
    NYaFF::TBuilder yffb;
    yffb.Finish(NProtoYaFF::NTestYaFF::CreateTUniversalMessage(yffb, proto));
    return yffb.Release();
}

NTestYaFF::TUniversalMessage Restore(const NProtoYaFF::NTestYaFF::TUniversalMessage& yaff) {
    NTestYaFF::TUniversalMessage proto;
    yaff.TransformTo(proto);
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
    const auto& yaff = NYaFF::ReadRoot<NProtoYaFF::NTestYaFF::TUniversalMessage>(buffer.Data());

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
    const std::array<std::pair<NProtoYaFF::NTestYaFF::EEnumeration, NTestYaFF::EEnumeration>, 4> testSet = {
        std::pair{NProtoYaFF::NTestYaFF::EEnumeration::ENUMERATION_UNSPECIFIED,
                  NTestYaFF::EEnumeration::ENUMERATION_UNSPECIFIED},
        std::pair{NProtoYaFF::NTestYaFF::EEnumeration::ENUMERATION_VALUE, NTestYaFF::EEnumeration::ENUMERATION_VALUE},
        std::pair{NProtoYaFF::NTestYaFF::EEnumeration::ENUMERATION_OTHER_VALUE,
                  NTestYaFF::EEnumeration::ENUMERATION_OTHER_VALUE},
        std::pair{NProtoYaFF::NTestYaFF::EEnumeration::auto_, NTestYaFF::EEnumeration::auto_}};

    for (auto [v1, v2] : testSet) {
        auto n1 = NProtoYaFF::NTestYaFF::EEnumeration_Name(v1);
        auto n2 = NTestYaFF::EEnumeration_Name(v2);
        EXPECT_EQ(n1, n2);

        NProtoYaFF::NTestYaFF::EEnumeration r1;
        NTestYaFF::EEnumeration r2;
        EXPECT_TRUE(NProtoYaFF::NTestYaFF::EEnumeration_Parse(n1, &r1));
        EXPECT_TRUE(NTestYaFF::EEnumeration_Parse(n2, &r2));
        EXPECT_EQ(static_cast<int>(r1), static_cast<int>(r2));
    }

    EXPECT_EQ(static_cast<int>(NProtoYaFF::NTestYaFF::EEnumeration_MIN), static_cast<int>(NTestYaFF::EEnumeration_MIN));
    EXPECT_EQ(static_cast<int>(NProtoYaFF::NTestYaFF::EEnumeration_MAX), static_cast<int>(NTestYaFF::EEnumeration_MAX));
    EXPECT_EQ(static_cast<int>(NProtoYaFF::NTestYaFF::EEnumeration_ARRAYSIZE),
              static_cast<int>(NTestYaFF::EEnumeration_ARRAYSIZE));

    EXPECT_EQ(NProtoYaFF::NTestYaFF::EEnumeration_IsValid(2), NTestYaFF::EEnumeration_IsValid(2));
    EXPECT_EQ(NProtoYaFF::NTestYaFF::EEnumeration_IsValid(9000), NTestYaFF::EEnumeration_IsValid(9000));
}
