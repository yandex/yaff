#pragma once

#include "base.h"

namespace NYaFF::NReflect {

struct TMessageDescriptor;
struct TEnumDescriptor;

struct TTypeDescriptor {
    const EType Type = EType::TYPE_NONE;

    union {
        const TTypeDescriptor* Element = nullptr;
        const TMessageDescriptor* Message;
        const TEnumDescriptor* Enum;
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

struct TFieldDescriptor {
    const TFieldId Id = 0;
    const char* Name = nullptr;
    const char* ContainingOneof = nullptr;

    const TTypeDescriptor* Type = nullptr;

    const EPresence Presence = EPresence::PRESENCE_NONE;
    const bool Deprecated = false;

    const TFieldOffset FlatOffset = 0;
};

struct TMessageDescriptor {
    const char* Name = nullptr;
    const EMessageLayout Layout = EMessageLayout::MESSAGE_LAYOUT_UNKNOWN;

    const uint16_t FieldCount = 0;
    const TFieldDescriptor* Fields = nullptr;
};

struct TEnumValueDescriptor {
    const char* Name = nullptr;
    const int32_t Value = 0;
};

struct TEnumDescriptor {
    const char* Name = nullptr;

    const uint32_t ValueCount = 0;
    const TEnumValueDescriptor* Values = nullptr;
};

inline size_t InlineSize(EType type);
inline size_t InlineSize(const TTypeDescriptor& type);

inline size_t FixedMessageSize(const TMessageDescriptor& msg) {
    YAFF_REQUIRE(msg.Layout == EMessageLayout::MESSAGE_LAYOUT_FIXED);
    if (msg.FieldCount == 0) {
        return 0;
    }
    const auto& last = msg.Fields[msg.FieldCount - 1];
    return last.FlatOffset + InlineSize(*last.Type);
}

inline size_t InlineSize(EType type) {
    switch (type) {
        case EType::TYPE_NONE:
            return 0;
        case EType::TYPE_BOOL:
            return sizeof(bool);
        case EType::TYPE_INT32:
        case EType::TYPE_ENUM:
            return sizeof(int32_t);
        case EType::TYPE_UINT32:
            return sizeof(uint32_t);
        case EType::TYPE_INT64:
            return sizeof(int64_t);
        case EType::TYPE_UINT64:
            return sizeof(uint64_t);
        case EType::TYPE_FLOAT:
            return sizeof(float);
        case EType::TYPE_DOUBLE:
            return sizeof(double);
        case EType::TYPE_STRING:
        case EType::TYPE_VECTOR:
            return sizeof(TOffset);
        case EType::TYPE_MESSAGE:
            YAFF_THROW("incomplete message type");
    }
}

inline size_t InlineSize(const TTypeDescriptor& type) {
    if (type.Type == EType::TYPE_MESSAGE) {
        if (type.Inline) {
            YAFF_REQUIRE(type.Descriptor.Message);
            return FixedMessageSize(*type.Descriptor.Message);
        }
        return sizeof(TOffset);
    }
    return InlineSize(type.Type);
}

inline TFieldResolverFunc MakeFieldResolverFunc(const TMessageDescriptor* desc) {
    return [desc](const TFieldId id) {
        YAFF_REQUIRE(id > 0);
        return desc->Fields[id - 1].FlatOffset;
    };
}

}  // namespace NYaFF::NReflect
