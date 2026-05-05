#pragma once

#include <yaff/base.h>

#include <map>
#include <set>
#include <string>

namespace NYaFF::NCompile::NIR {

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
struct TSymbolTable {
    std::pair<T*, bool> TryEmplace(T value) {
        std::string key = value.ToString();
        const auto [it, emplaced] = Table.try_emplace(std::move(key), std::move(value));
        return {&it->second, emplaced};
    }

    T* GetOrEmplace(T value) {
        const auto [ptr, _] = TryEmplace(std::move(value));
        return ptr;
    }

    T* Get(const T& value) {
        const auto it = Table.find(value.ToString());
        return (it != Table.end() ? &it->second : nullptr);
    }

    std::map<std::string, T> Table;
};

struct TEnumDef;
struct TTableDef;
struct TSchemaDef;

struct TType {
    EType Type = EType::TYPE_NONE;
    const TType* ElementType = nullptr;   // Not nullptr only for TYPE_VECTOR;
    const TTableDef* TableDef = nullptr;  // Not nullptr only for TYPE_TABLE;
    const TEnumDef* EnumDef = nullptr;    // Not nullptr only for TYPE_ENUM;

    std::map<std::string, std::string> Modifiers;

    size_t InlineSize() const;
    std::string ToString() const;
};

struct TDef {
    std::string Name;
    bool Defined = false;

    explicit TDef(std::string name);
};

struct TEnumDef : public TDef {
    struct TEnumVal {
        std::string Name;
        int32_t Value;
    };

    const TSchemaDef* Schema = nullptr;
    std::vector<TEnumVal> Values;

    TEnumDef(std::string name, const TSchemaDef* schema);
    std::string ToString() const;
};

struct TTableDef : public TDef {
    // TFieldDef is not completely independent entity, since it exists only as part of TableDef.
    struct TFieldDef {
        uint64_t Id = 0;
        std::string Name;

        const TType* Type = nullptr;

        EPresence Presence = EPresence::PRESENCE_NONE;
        bool Deprecated = false;

        std::string SharedOffsetVia;

        // These fields are filled during post-processing of the IR.
        uint64_t ActiveIndex = 0;
        uint64_t FlatOffset = 0;
        uint64_t SharedOffsetId = 0;
    };

    // This is not real definition. This structure is filled in during post-processing of the IR for optimizations.
    struct TSharedOffset {
        std::string Name;
        std::vector<const TFieldDef*> Fields;
    };

    const TSchemaDef* Schema = nullptr;

    ETableLayout Layout = ETableLayout::TABLE_LAYOUT_UNKNOWN;
    std::vector<TFieldDef> Fields;

    // This fields are filled during post-processing of the IR.
    // Exists on TableDef only for optimization.
    bool AssociativePair = false;
    std::map<std::string, TSharedOffset> SharedOffsets;

    TTableDef(std::string name, const TSchemaDef* schema, ETableLayout layout = ETableLayout::TABLE_LAYOUT_UNKNOWN);
    std::string ToString() const;
};

struct TSchemaDef : public TDef {
    struct TSchemaOrder {
        bool operator()(const TSchemaDef* a, const TSchemaDef* b) const {
            return a->Name < b->Name;
        }
    };

    std::string Namespace;

    std::set<const TSchemaDef*, TSchemaOrder> Dependencies;
    std::map<std::string, std::string> Attributes;

    std::vector<const TEnumDef*> Enums;
    std::vector<const TTableDef*> Tables;

    explicit TSchemaDef(std::string name);
    std::string ToString() const;
};

struct TIR {
    TIR() = default;
    ~TIR() = default;

    TIR(TIR&&) = default;
    TIR& operator=(TIR&&) = default;

    TIR(const TIR&) = delete;
    TIR& operator=(const TIR&) = delete;

    TSymbolTable<TType> Types;
    TSymbolTable<TEnumDef> Enums;
    TSymbolTable<TTableDef> Tables;
    TSymbolTable<TSchemaDef> Schemas;
};

const NIR::TTableDef* ExtractTableDef(const NIR::TType& type);

bool IsBasic(const EType type);
bool IsScalar(const EType type);
size_t InlineSize(const EType type);
size_t FixedTableSize(const TTableDef& table);

bool IsFixedTable(const TTableDef& table);
bool IsDynamicTable(const TTableDef& table);
bool IsStaticMetaTable(const TTableDef& table);
bool IsAssociativePair(const TTableDef& table);
bool IsGapTable(const TTableDef& table);

bool IsScalarVector(const TType& type);
bool IsInline(const TType& type);
bool IsAssociative(const TType& type);
bool IsSequentialTable(const TType& type);
bool IsSortedSequentialTable(const TType& type);
bool IsFixedTable(const TType& type);
bool IsSharedOffsetField(const TTableDef::TFieldDef& field);
bool IsExplicitField(const TTableDef::TFieldDef& field);

std::string TypeToString(const EType type);
EType TypeFromString(const std::string& value);

std::string PresenceToString(const EPresence type);
EPresence PresenceFromString(const std::string& value);

}  // namespace NYaFF::NCompile::NIR
