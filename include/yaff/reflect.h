#pragma once

#include "base.h"

namespace yaff::reflect {

struct MessageDescriptor;
struct EnumDescriptor;

struct TypeDescriptor {
    const Type Type = Type::TYPE_NONE;

    union {
        const TypeDescriptor* Element = nullptr;
        const MessageDescriptor* Message;
        const EnumDescriptor* Enum;
    } Descriptor;

    union {
        const char* String = nullptr;
        const double Double;
        const float Float;
        const uint64_t Uint64;
        const int64_t Int64;
        const uint32_t Uint32;
        const int32_t Int32;
        const bool Bool;
    } Default;

    const bool Associative = false;
    const bool Inline = false;
};

struct FieldDescriptor {
    const FieldId Id = 0;
    const char* Name = nullptr;
    const char* ContainingOneof = nullptr;

    const TypeDescriptor* Type = nullptr;

    const Presence Presence = Presence::PRESENCE_NONE;
    const bool Deprecated = false;

    const FieldOffset FlatOffset = 0;
};

struct MessageDescriptor {
    const char* Name = nullptr;
    const MessageLayout Layout = MessageLayout::MESSAGE_LAYOUT_UNKNOWN;

    const uint16_t FieldCount = 0;
    const FieldDescriptor* Fields = nullptr;
};

struct EnumValueDescriptor {
    const char* Name = nullptr;
    const int32_t Value = 0;
};

struct EnumDescriptor {
    const char* Name = nullptr;

    const uint32_t ValueCount = 0;
    const EnumValueDescriptor* Values = nullptr;
};

inline size_t InlineSize(Type type);
inline size_t InlineSize(const TypeDescriptor& type);

inline size_t FixedMessageSize(const MessageDescriptor& msg) {
    YAFF_REQUIRE(msg.Layout == MessageLayout::MESSAGE_LAYOUT_FIXED);
    if (msg.FieldCount == 0) {
        return 0;
    }
    const auto& last = msg.Fields[msg.FieldCount - 1];
    return last.FlatOffset + InlineSize(*last.Type);
}

inline size_t InlineSize(Type type) {
    switch (type) {
        case Type::TYPE_NONE:
            return 0;
        case Type::TYPE_BOOL:
            return sizeof(bool);
        case Type::TYPE_INT32:
        case Type::TYPE_ENUM:
            return sizeof(int32_t);
        case Type::TYPE_UINT32:
            return sizeof(uint32_t);
        case Type::TYPE_INT64:
            return sizeof(int64_t);
        case Type::TYPE_UINT64:
            return sizeof(uint64_t);
        case Type::TYPE_FLOAT:
            return sizeof(float);
        case Type::TYPE_DOUBLE:
            return sizeof(double);
        case Type::TYPE_STRING:
        case Type::TYPE_VECTOR:
            return sizeof(Offset);
        case Type::TYPE_MESSAGE:
            YAFF_THROW("incomplete message type");
    }
}

inline size_t InlineSize(const TypeDescriptor& type) {
    if (type.Type == Type::TYPE_MESSAGE) {
        if (type.Inline) {
            YAFF_REQUIRE(type.Descriptor.Message);
            return FixedMessageSize(*type.Descriptor.Message);
        }
        return sizeof(Offset);
    }
    return InlineSize(type.Type);
}

inline FieldResolver MakeFieldResolverFunc(const MessageDescriptor* desc) {
    return [desc](const FieldId id) {
        YAFF_REQUIRE(id > 0);
        return desc->Fields[id - 1].FlatOffset;
    };
}

}  // namespace yaff::reflect
