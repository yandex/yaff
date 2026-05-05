#include "ir_processor.h"

#include <unordered_set>

namespace NYaFF::NCompile {

class TIRProcessor {
public:
    void Process(NIR::TIR& ir) &&;

private:
    static void Ensure(const bool cond, const std::string& message);

    void ProcessSchema(NIR::TIR& ir, NIR::TSchemaDef& schemaDef);
    void ProcessEnum(NIR::TIR& ir, NIR::TEnumDef& enumDef);
    void ProcessTable(NIR::TIR& ir, NIR::TTableDef& tableDef);

    std::unordered_set<const NIR::TTableDef*> ProcessedTables_;
};

void TIRProcessor::Process(NIR::TIR& ir) && {
    for (auto& [_, schemaDef] : ir.Schemas.Table) {
        ProcessSchema(ir, schemaDef);
    }

    for (auto& [_, enumDef] : ir.Enums.Table) {
        ProcessEnum(ir, enumDef);
    }

    for (auto& [_, tableDef] : ir.Tables.Table) {
        if (!ProcessedTables_.contains(&tableDef)) {
            ProcessTable(ir, tableDef);
        }
    }
}

void TIRProcessor::ProcessSchema(NIR::TIR&, NIR::TSchemaDef& schemaDef) {
    const bool someEnums = std::any_of(schemaDef.Enums.begin(), schemaDef.Enums.end(),
                                       [](const NIR::TEnumDef* enm) { return enm->Defined; });
    const bool someTables = std::any_of(schemaDef.Tables.begin(), schemaDef.Tables.end(),
                                        [](const NIR::TTableDef* tbl) { return tbl->Defined; });
    schemaDef.Defined = (someEnums || someTables);
}

void TIRProcessor::ProcessEnum(NIR::TIR&, NIR::TEnumDef& enumDef) {
    Ensure(enumDef.Schema, "enum can not be not connected to schema '" + enumDef.Name + "'");
    Ensure(enumDef.Defined == enumDef.Schema->Defined, "incomplete definition of enum '" + enumDef.Name +
                                                           "' in defined schema '" + enumDef.Schema->Namespace + "'");
    if (!enumDef.Defined) {
        return;
    }

    std::sort(enumDef.Values.begin(), enumDef.Values.end(),
              [](const auto& l, const auto& r) { return l.Value < r.Value; });
}

void TIRProcessor::ProcessTable(NIR::TIR& ir, NIR::TTableDef& tableDef) {
    if (!tableDef.Defined) {
        return;
    }
    const auto [_, emplaced] = ProcessedTables_.emplace(&tableDef);
    Ensure(emplaced, "duplicated process call for table '" + tableDef.Name + "'");

    Ensure(tableDef.Schema, "table can not be not connected to schema '" + tableDef.Name + "'");
    Ensure(tableDef.Defined == tableDef.Schema->Defined, "incomplete definition of table '" + tableDef.Name +
                                                             "' in defined schema '" + tableDef.Schema->Namespace +
                                                             "'");
    if (!tableDef.Defined) {
        return;
    }

    std::sort(tableDef.Fields.begin(), tableDef.Fields.end(), [](const auto& l, const auto& r) { return l.Id < r.Id; });

    const bool gapped = NIR::IsGapTable(tableDef);
    Ensure(!gapped || (tableDef.Layout != ETableLayout::TABLE_LAYOUT_FLAT &&
                       tableDef.Layout != ETableLayout::TABLE_LAYOUT_FIXED),
           "table '" + tableDef.Name + "' can not contain any gaps because it has field size based layout");

    bool comparable = false;
    uint64_t activeIndex = 1;
    TFieldOffset flatOffset = 0;
    for (auto& fieldDef : tableDef.Fields) {
        Ensure(!fieldDef.Name.empty() || fieldDef.Deprecated,
               "field with id '" + std::to_string(fieldDef.Id) + "' has empty name");

        if (fieldDef.Type->Modifiers.contains(NIR::DEFAULT_MODIFIER_NAME)) {
            Ensure(NIR::IsScalar(fieldDef.Type->Type) || fieldDef.Type->Type == EType::TYPE_STRING,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) + ") " +
                       "not allowed to have default value");
        }

        Ensure(!fieldDef.Type->Modifiers.contains(NIR::INLINE_MODIFIER_NAME),
               "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                   ") is inlined, but inlined fields are not supported yet");

        if (fieldDef.Type->Type == EType::TYPE_VECTOR) {
            const auto* elementType = fieldDef.Type->ElementType;
            Ensure(!elementType || elementType->Type != EType::TYPE_VECTOR,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is 2d vector, but 2d vectors are not supported");
            Ensure(fieldDef.Deprecated || fieldDef.Presence == EPresence::PRESENCE_IMPLICIT,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is vector, but has explicit presence");
        }

        if (fieldDef.Type->Type == EType::TYPE_TABLE) {
            Ensure(fieldDef.Deprecated || fieldDef.Presence == EPresence::PRESENCE_EXPLICIT,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is table, but has implicit presence");
        }

        Ensure(fieldDef.ActiveIndex == 0,
               "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) + ") has non-zero active index");

        if (!fieldDef.Deprecated) {
            fieldDef.ActiveIndex = activeIndex++;
        }

        if (fieldDef.Type->Modifiers.contains(NIR::KEY_MODIFIER_NAME)) {
            Ensure(!fieldDef.Deprecated,
                   "field (id: " + std::to_string(fieldDef.Id) + ") is deprecated and can not be key");
            Ensure(NIR::IsScalar(fieldDef.Type->Type) || fieldDef.Type->Type == EType::TYPE_STRING,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is not scalar or string and can not be key");

            Ensure(!comparable, "table '" + tableDef.Name + "' contains multiple key fields");
            comparable = true;
        }

        if (!fieldDef.SharedOffsetVia.empty()) {
            Ensure(tableDef.Layout != ETableLayout::TABLE_LAYOUT_FIXED,
                   "table '" + tableDef.Name + "' is fixed and can not have any oneof fields");
            Ensure(fieldDef.Type->Type != EType::TYPE_VECTOR, "field '" + fieldDef.Name +
                                                                  "' (id: " + std::to_string(fieldDef.Id) +
                                                                  ") is vector and can not be oneof field");
            Ensure(fieldDef.Deprecated || fieldDef.Presence == EPresence::PRESENCE_EXPLICIT,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is oneof field, but has implicit presence");

            auto [it, _] = tableDef.SharedOffsets.try_emplace(
                fieldDef.SharedOffsetVia, NIR::TTableDef::TSharedOffset{.Name = fieldDef.SharedOffsetVia});
            it->second.Fields.emplace_back(&fieldDef);

            fieldDef.SharedOffsetId = it->second.Fields.size();
        }

        fieldDef.FlatOffset = flatOffset * !gapped;
        flatOffset += fieldDef.Type->InlineSize();
    }

    for (auto& fieldDef : tableDef.Fields) {
        const auto* fieldTable = ExtractTableDef(*fieldDef.Type);
        if (!fieldTable) {
            continue;
        }

        auto* table = ir.Tables.Get(*fieldTable);
        if (!ProcessedTables_.contains(fieldTable)) {
            ProcessTable(ir, *table);
        }

        if (NIR::IsAssociative(*fieldDef.Type)) {
            table->AssociativePair = true;
            Ensure(table->Layout == ETableLayout::TABLE_LAYOUT_FIXED,
                   "table '" + table->Name + "' marked as associative pair, but is not fixed");
            Ensure(table->Fields.size() == 2,
                   "table '" + table->Name + "' marked as associative pair, but contains not 2 fields");

            const auto& key = table->Fields[0];
            Ensure(!key.Deprecated, "field (id: " + std::to_string(key.Id) + ") is deprecated and can not be map key");
            Ensure(NIR::IsScalar(key.Type->Type) || key.Type->Type == EType::TYPE_STRING,
                   "field '" + key.Name + "' (id: " + std::to_string(key.Id) +
                       ") is not scalar or string and can not be map key");
        }
    }
}

void TIRProcessor::Ensure(const bool cond, const std::string& message) {
    if (YAFF_UNLIKELY(!cond)) {
        throw std::runtime_error(message.c_str());
    }
}

void ProcessIR(NIR::TIR& ir) {
    TIRProcessor{}.Process(ir);
}

}  // namespace NYaFF::NCompile
