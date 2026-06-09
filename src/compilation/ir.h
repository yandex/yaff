#pragma once

#include <yaff/base.h>

#include <map>
#include <set>
#include <string>

namespace yaff::compilation::ir {

inline constexpr const char* PROTO_FILE_ATTRIBUTE_NAME = "ProtoFile";
inline constexpr const char* PROTO_NAMESPACE_ATTRIBUTE_NAME = "ProtoNamespace";

inline constexpr const char* DEFAULT_MODIFIER_NAME = "Default";
inline constexpr const char* ASSOCIATIVE_MODIFIER_NAME = "Associative";
inline constexpr const char* INLINE_MODIFIER_NAME = "Inline";

// External experimental modifiers;
inline constexpr const char* KEY_MODIFIER_NAME = "Key";
inline constexpr const char* SEQUENTIAL_MODIFIER_NAME = "Sequential";
inline constexpr const char* DEFAULT_STREAM_TYPE_MODIFIER_NAME = "DefaultStreamType";
inline constexpr const char* SORT_ORDER_MODIFIER_NAME = "SortOrder";

template <typename T>
struct SymbolTable {
    std::pair<T*, bool> TryEmplace(T value) {
        std::string key = value.ToString();
        const auto [it, emplaced] = Symbols.try_emplace(std::move(key), std::move(value));
        return {&it->second, emplaced};
    }

    T* GetOrEmplace(T value) {
        const auto [ptr, _] = TryEmplace(std::move(value));
        return ptr;
    }

    T* Get(const T& value) {
        const auto it = Symbols.find(value.ToString());
        return (it != Symbols.end() ? &it->second : nullptr);
    }

    std::map<std::string, T> Symbols;
};

struct EnumDef;
struct MessageDef;
struct SchemaDef;

struct TypeDef {
    Type Type = Type::TYPE_NONE;
    const TypeDef* ElementType = nullptr;    // Not nullptr only for TYPE_ARRAY;
    const MessageDef* MessageDef = nullptr;  // Not nullptr only for TYPE_MESSAGE;
    const EnumDef* EnumDef = nullptr;        // Not nullptr only for TYPE_ENUM;

    std::map<std::string, std::string> Modifiers;

    size_t InlineSize() const;
    std::string ToString() const;
};

struct BaseDef {
    std::string Name;
    bool Defined = false;

    explicit BaseDef(std::string name);
};

struct EnumDef : public BaseDef {
    struct TEnumVal {
        std::string Name;
        int32_t Value;
    };

    const SchemaDef* Schema = nullptr;
    std::vector<TEnumVal> Values;

    EnumDef(std::string name, const SchemaDef* schema);
    std::string ToString() const;
};

struct MessageDef : public BaseDef {
    // FieldDef is not completely independent entity, since it exists only as part of MessageDef.
    struct FieldDef {
        uint64_t Id = 0;
        std::string Name;

        const TypeDef* Type = nullptr;

        Presence Presence = Presence::PRESENCE_NONE;
        bool Deprecated = false;

        std::string OneOf;

        // These fields are filled during post-processing of the IR.
        uint64_t ActiveIndex = 0;
        uint64_t FlatOffset = 0;
    };

    // This is not real definition. This structure is filled in during post-processing of the IR for optimizations.
    struct OneOfDef {
        std::string Name;
        std::vector<const FieldDef*> Fields;
    };

    const SchemaDef* Schema = nullptr;

    MessageLayout Layout = MessageLayout::MESSAGE_LAYOUT_UNKNOWN;
    std::vector<FieldDef> Fields;

    // This fields are filled during post-processing of the IR.
    // Exists on MessageDef only for optimization.
    bool AssociativePair = false;
    std::map<std::string, OneOfDef> OneOfs;

    MessageDef(std::string name, const SchemaDef* schema, MessageLayout layout = MessageLayout::MESSAGE_LAYOUT_UNKNOWN);
    std::string ToString() const;
};

struct SchemaDef : public BaseDef {
    struct SchemaOrder {
        bool operator()(const SchemaDef* a, const SchemaDef* b) const {
            return a->Name < b->Name;
        }
    };

    std::string Namespace;

    std::set<const SchemaDef*, SchemaOrder> Dependencies;
    std::map<std::string, std::string> Attributes;

    std::vector<const EnumDef*> Enums;
    std::vector<const MessageDef*> Messages;

    explicit SchemaDef(std::string name);
    std::string ToString() const;
};

struct IR {
    IR() = default;
    ~IR() = default;

    IR(IR&&) = default;
    IR& operator=(IR&&) = default;

    IR(const IR&) = delete;
    IR& operator=(const IR&) = delete;

    SymbolTable<TypeDef> Types;
    SymbolTable<EnumDef> Enums;
    SymbolTable<MessageDef> Messages;
    SymbolTable<SchemaDef> Schemas;
};

const ir::MessageDef* ExtractMessageDef(const ir::TypeDef& type);

bool IsBasic(const Type type);
bool IsScalar(const Type type);
size_t InlineSize(const Type type);

size_t MaxMessageSize(const MessageDef& msg);
size_t MaxFieldId(const MessageDef& msg);

bool IsFixedMessage(const MessageDef& msg);
bool IsDynamicMessage(const MessageDef& msg);
bool IsStaticMetaMessage(const MessageDef& msg);
bool IsAssociativePair(const MessageDef& msg);
bool IsGapMessage(const MessageDef& msg);

bool IsScalarArray(const TypeDef& type);
bool IsInline(const TypeDef& type);
bool IsAssociative(const TypeDef& type);
bool IsSequentialMessage(const TypeDef& type);
bool IsSortedSequentialMessage(const TypeDef& type);
bool IsFixedMessage(const TypeDef& type);
bool IsOneOfField(const MessageDef::FieldDef& field);
bool IsExplicitField(const MessageDef::FieldDef& field);

std::string TypeToString(const Type type);
Type TypeFromString(const std::string& value);

std::string PresenceToString(const Presence type);
Presence PresenceFromString(const std::string& value);

}  // namespace yaff::compilation::ir
