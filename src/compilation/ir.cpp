#include "ir.h"

namespace yaff::compilation::ir {

static std::string TypeString(const TypeDef& type) {
    switch (type.Type) {
        case Type::TYPE_ENUM: {
            const std::string inner = (type.EnumDef ? type.EnumDef->ToString() : "Incomplete");
            return "Enum<" + inner + ">";
        }
        case Type::TYPE_ARRAY: {
            const std::string inner = (type.ElementType ? type.ElementType->ToString() : "Incomplete");
            return "Array[" + inner + "]";
        }
        case Type::TYPE_MESSAGE: {
            const std::string inner = (type.MessageDef ? type.MessageDef->ToString() : "Incomplete");
            return "Message{" + inner + "}";
        }
        default:
            return TypeToString(type.Type);
    }
}

size_t MaxMessageSize(const MessageDef& msg) {
    if (msg.Fields.empty()) {
        return 0;
    }
    const auto& last = msg.Fields.back();
    return last.FlatOffset + last.Type->InlineSize();
}
size_t MaxFieldId(const MessageDef& msg) {
    if (msg.Fields.empty()) {
        return 0;
    }
    const auto& last = msg.Fields.back();
    return last.Id;
}

size_t InlineSize(const Type type) {
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
        case Type::TYPE_ARRAY:
        case Type::TYPE_STRING:
            return sizeof(Offset);
        case Type::TYPE_MESSAGE:
            YAFF_THROW("incomplete message type");
    }
    YAFF_THROW("unknown type");
}

size_t TypeDef::InlineSize() const {
    if (Type == Type::TYPE_MESSAGE) {
        if (IsInline(*this)) {
            YAFF_REQUIRE(MessageDef);
            return MaxMessageSize(*MessageDef);
        }
        return sizeof(Offset);
    }
    return ::yaff::compilation::ir::InlineSize(Type);
}

std::string TypeDef::ToString() const {
    std::string result = TypeString(*this) + "(";
    for (const auto& [mod, val] : Modifiers) {
        result += mod + "=" + val + ";";
    }
    return result + ")";
}

BaseDef::BaseDef(std::string name) : Name(std::move(name)) {
}

EnumDef::EnumDef(std::string name, const SchemaDef* schema) : BaseDef(std::move(name)), Schema(schema) {
}

std::string EnumDef::ToString() const {
    return Schema->Namespace + "::" + Name;
}

MessageDef::MessageDef(std::string name, const SchemaDef* schema, std::vector<const BaseDef*> nestedTypes,
                       const MessageLayout layout)
    : BaseDef(std::move(name)), Schema(schema), NestedTypes(std::move(nestedTypes)), Layout(layout) {
}

std::string MessageDef::ToString() const {
    return Schema->Namespace + "::" + Name;
}

SchemaDef::SchemaDef(std::string name) : BaseDef(std::move(name)) {
}

std::string SchemaDef::ToString() const {
    return Name;
}

bool IsBasic(const Type type) {
    return type >= Type::TYPE_BOOL && type <= Type::TYPE_DOUBLE;
}

bool IsScalar(const Type type) {
    return type >= Type::TYPE_BOOL && type <= Type::TYPE_ENUM;
}

std::string TypeToString(const Type type) {
    switch (type) {
        case Type::TYPE_NONE:
            return "None";
        case Type::TYPE_BOOL:
            return "Bool";
        case Type::TYPE_INT32:
            return "Int32";
        case Type::TYPE_UINT32:
            return "Uint32";
        case Type::TYPE_INT64:
            return "Int64";
        case Type::TYPE_UINT64:
            return "Uint64";
        case Type::TYPE_FLOAT:
            return "Float";
        case Type::TYPE_DOUBLE:
            return "Double";
        case Type::TYPE_ENUM:
            return "Enum";
        case Type::TYPE_STRING:
            return "String";
        case Type::TYPE_ARRAY:
            return "Array";
        case Type::TYPE_MESSAGE:
            return "Message";
    }
    YAFF_THROW("unknown type");
}

Type TypeFromString(const std::string& value) {
    if (value == "None") {
        return Type::TYPE_NONE;
    }
    if (value == "Bool") {
        return Type::TYPE_BOOL;
    }
    if (value == "Int32") {
        return Type::TYPE_INT32;
    }
    if (value == "Uint32") {
        return Type::TYPE_UINT32;
    }
    if (value == "Int64") {
        return Type::TYPE_INT64;
    }
    if (value == "Uint64") {
        return Type::TYPE_UINT64;
    }
    if (value == "Float") {
        return Type::TYPE_FLOAT;
    }
    if (value == "Double") {
        return Type::TYPE_DOUBLE;
    }
    if (value == "Enum") {
        return Type::TYPE_ENUM;
    }
    if (value == "String") {
        return Type::TYPE_STRING;
    }
    if (value == "Array") {
        return Type::TYPE_ARRAY;
    }
    if (value == "Message") {
        return Type::TYPE_MESSAGE;
    }
    YAFF_THROW("unknown type");
}

std::string PresenceToString(const Presence type) {
    switch (type) {
        case Presence::PRESENCE_NONE:
            return "None";
        case Presence::PRESENCE_IMPLICIT:
            return "Implicit";
        case Presence::PRESENCE_EXPLICIT:
            return "Explicit";
    }
    YAFF_THROW("unknown presence type");
}

Presence PresenceFromString(const std::string& value) {
    if (value == "None") {
        return Presence::PRESENCE_NONE;
    }
    if (value == "Implicit") {
        return Presence::PRESENCE_IMPLICIT;
    }
    if (value == "Explicit") {
        return Presence::PRESENCE_EXPLICIT;
    }
    YAFF_THROW("unknown presence type");
}

const ir::MessageDef* ExtractMessageDef(const ir::TypeDef& type) {
    if (type.Type == Type::TYPE_ARRAY && type.ElementType) {
        return ExtractMessageDef(*type.ElementType);
    }
    if (type.Type == Type::TYPE_MESSAGE) {
        return type.MessageDef;
    }
    return nullptr;
}

bool IsFixedMessage(const MessageDef& msg) {
    return msg.Layout == MessageLayout::MESSAGE_LAYOUT_FIXED;
}

bool IsDynamicMessage(const MessageDef& msg) {
    return msg.Layout == MessageLayout::MESSAGE_LAYOUT_DYNAMIC;
}

bool IsStaticMetaMessage(const MessageDef& msg) {
    return msg.Layout != MessageLayout::MESSAGE_LAYOUT_SPARSE;
}

bool IsAssociativePair(const MessageDef& msg) {
    return msg.AssociativePair;
}

bool IsGapMessage(const MessageDef& msg) {
    for (uint64_t i = 0; i < msg.Fields.size(); ++i) {
        if (msg.Fields[i].Id != i + 1) {
            return true;
        }
    }
    return false;
}

bool IsScalarArray(const TypeDef& type) {
    return type.Type == Type::TYPE_ARRAY && type.ElementType && IsScalar(type.ElementType->Type);
}

bool IsInline(const TypeDef& type) {
    return type.Modifiers.contains(ir::INLINE_MODIFIER_NAME);
}

bool IsAssociative(const TypeDef& type) {
    return type.Modifiers.contains(ir::ASSOCIATIVE_MODIFIER_NAME);
}

bool IsSequentialMessage(const TypeDef& type) {
    return type.ElementType && type.ElementType->Type == Type::TYPE_MESSAGE &&
           type.Modifiers.contains(ir::SEQUENTIAL_MODIFIER_NAME);
}

bool IsSortedSequentialMessage(const TypeDef& type) {
    return IsSequentialMessage(type) && type.Modifiers.contains(ir::SORT_ORDER_MODIFIER_NAME);
}

bool IsFixedMessage(const TypeDef& type) {
    return type.Type == Type::TYPE_MESSAGE && type.MessageDef && IsFixedMessage(*type.MessageDef);
}

bool IsOneOfField(const MessageDef::FieldDef& field) {
    return !field.OneOf.empty();
}

bool IsExplicitField(const MessageDef::FieldDef& field) {
    return field.Presence == Presence::PRESENCE_EXPLICIT;
}

}  // namespace yaff::compilation::ir
