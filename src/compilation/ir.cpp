#include "ir.h"

namespace NYaFF::NCompile::NIR {

static std::string TypeString(const TType& type) {
    switch (type.Type) {
        case EType::TYPE_ENUM: {
            const std::string inner = (type.EnumDef ? type.EnumDef->ToString() : "Incomplete");
            return "Enum<" + inner + ">";
        }
        case EType::TYPE_VECTOR: {
            const std::string inner = (type.ElementType ? type.ElementType->ToString() : "Incomplete");
            return "Vector[" + inner + "]";
        }
        case EType::TYPE_TABLE: {
            const std::string inner = (type.TableDef ? type.TableDef->ToString() : "Incomplete");
            return "Table{" + inner + "}";
        }
        default:
            return TypeToString(type.Type);
    }
}

size_t FixedTableSize(const TTableDef& table) {
    YAFF_REQUIRE(IsFixedTable(table));
    if (table.Fields.empty()) {
        return 0;
    }
    const auto& last = table.Fields.back();
    return last.FlatOffset + last.Type->InlineSize();
}

size_t InlineSize(const EType type) {
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
        case EType::TYPE_VECTOR:
        case EType::TYPE_STRING:
            return sizeof(TOffset);
        case EType::TYPE_TABLE:
            YAFF_THROW("incomplete table type");
    }
    YAFF_THROW("unknown type");
}

size_t TType::InlineSize() const {
    if (Type == EType::TYPE_TABLE) {
        if (IsInline(*this)) {
            YAFF_REQUIRE(TableDef);
            return FixedTableSize(*TableDef);
        }
        return sizeof(TOffset);
    }
    return ::NYaFF::NCompile::NIR::InlineSize(Type);
}

std::string TType::ToString() const {
    std::string result = TypeString(*this) + "(";
    for (const auto& [mod, val] : Modifiers) {
        result += mod + "=" + val + ";";
    }
    return result + ")";
}

TDef::TDef(std::string name) : Name(std::move(name)) {
}

TEnumDef::TEnumDef(std::string name, const TSchemaDef* schema) : TDef(std::move(name)), Schema(schema) {
}

std::string TEnumDef::ToString() const {
    return Schema->Namespace + "::" + Name;
}

TTableDef::TTableDef(std::string name, const TSchemaDef* schema, ETableLayout layout)
    : TDef(std::move(name)), Schema(schema), Layout(layout) {
}

std::string TTableDef::ToString() const {
    return Schema->Namespace + "::" + Name;
}

TSchemaDef::TSchemaDef(std::string name) : TDef(std::move(name)) {
}

std::string TSchemaDef::ToString() const {
    return Name;
}

bool IsBasic(const EType type) {
    return type >= EType::TYPE_BOOL && type <= EType::TYPE_DOUBLE;
}

bool IsScalar(const EType type) {
    return type >= EType::TYPE_BOOL && type <= EType::TYPE_ENUM;
}

std::string TypeToString(const EType type) {
    switch (type) {
        case EType::TYPE_NONE:
            return "None";
        case EType::TYPE_BOOL:
            return "Bool";
        case EType::TYPE_INT32:
            return "Int32";
        case EType::TYPE_UINT32:
            return "Uint32";
        case EType::TYPE_INT64:
            return "Int64";
        case EType::TYPE_UINT64:
            return "Uint64";
        case EType::TYPE_FLOAT:
            return "Float";
        case EType::TYPE_DOUBLE:
            return "Double";
        case EType::TYPE_ENUM:
            return "Enum";
        case EType::TYPE_STRING:
            return "String";
        case EType::TYPE_VECTOR:
            return "Vector";
        case EType::TYPE_TABLE:
            return "Table";
    }
    YAFF_THROW("unknown type");
}

EType TypeFromString(const std::string& value) {
    if (value == "None") {
        return EType::TYPE_NONE;
    }
    if (value == "Bool") {
        return EType::TYPE_BOOL;
    }
    if (value == "Int32") {
        return EType::TYPE_INT32;
    }
    if (value == "Uint32") {
        return EType::TYPE_UINT32;
    }
    if (value == "Int64") {
        return EType::TYPE_INT64;
    }
    if (value == "Uint64") {
        return EType::TYPE_UINT64;
    }
    if (value == "Float") {
        return EType::TYPE_FLOAT;
    }
    if (value == "Double") {
        return EType::TYPE_DOUBLE;
    }
    if (value == "Enum") {
        return EType::TYPE_ENUM;
    }
    if (value == "String") {
        return EType::TYPE_STRING;
    }
    if (value == "Vector") {
        return EType::TYPE_VECTOR;
    }
    if (value == "Table") {
        return EType::TYPE_TABLE;
    }
    YAFF_THROW("unknown type");
}

std::string PresenceToString(const EPresence type) {
    switch (type) {
        case EPresence::PRESENCE_NONE:
            return "None";
        case EPresence::PRESENCE_IMPLICIT:
            return "Implicit";
        case EPresence::PRESENCE_EXPLICIT:
            return "Explicit";
    }
    YAFF_THROW("unknown presence type");
}

EPresence PresenceFromString(const std::string& value) {
    if (value == "None") {
        return EPresence::PRESENCE_NONE;
    }
    if (value == "Implicit") {
        return EPresence::PRESENCE_IMPLICIT;
    }
    if (value == "Explicit") {
        return EPresence::PRESENCE_EXPLICIT;
    }
    YAFF_THROW("unknown presence type");
}

const NIR::TTableDef* ExtractTableDef(const NIR::TType& type) {
    if (type.Type == EType::TYPE_VECTOR && type.ElementType) {
        return ExtractTableDef(*type.ElementType);
    }
    if (type.Type == EType::TYPE_TABLE) {
        return type.TableDef;
    }
    return nullptr;
}

bool IsFixedTable(const TTableDef& table) {
    return table.Layout == ETableLayout::TABLE_LAYOUT_FIXED;
}

bool IsDynamicTable(const TTableDef& table) {
    return table.Layout == ETableLayout::TABLE_LAYOUT_DYNAMIC;
}

bool IsStaticMetaTable(const TTableDef& table) {
    return table.Layout == ETableLayout::TABLE_LAYOUT_FIXED || table.Layout == ETableLayout::TABLE_LAYOUT_FLAT ||
           (table.Layout == ETableLayout::TABLE_LAYOUT_DYNAMIC && !NIR::IsGapTable(table));
}

bool IsAssociativePair(const TTableDef& table) {
    return table.AssociativePair;
}

bool IsGapTable(const TTableDef& table) {
    for (uint64_t i = 0; i < table.Fields.size(); ++i) {
        if (table.Fields[i].Id != i + 1) {
            return true;
        }
    }
    return false;
}

bool IsScalarVector(const TType& type) {
    return type.Type == EType::TYPE_VECTOR && type.ElementType && IsScalar(type.ElementType->Type);
}

bool IsInline(const TType& type) {
    return type.Modifiers.contains(NIR::INLINE_MODIFIER_NAME);
}

bool IsAssociative(const TType& type) {
    return type.Modifiers.contains(NIR::ASSOCIATIVE_MODIFIER_NAME);
}

bool IsSequentialTable(const TType& type) {
    return type.ElementType && type.ElementType->Type == EType::TYPE_TABLE &&
           type.Modifiers.contains(NIR::SEQUENTIAL_MODIFIER_NAME);
}

bool IsSortedSequentialTable(const TType& type) {
    return IsSequentialTable(type) && type.Modifiers.contains(NIR::SORT_ORDER_MODIFIER_NAME);
}

bool IsFixedTable(const TType& type) {
    return type.Type == EType::TYPE_TABLE && type.TableDef && IsFixedTable(*type.TableDef);
}

bool IsSharedOffsetField(const TTableDef::TFieldDef& field) {
    return !field.SharedOffsetVia.empty();
}

bool IsExplicitField(const TTableDef::TFieldDef& field) {
    return field.Presence == EPresence::PRESENCE_EXPLICIT;
}

}  // namespace NYaFF::NCompile::NIR
