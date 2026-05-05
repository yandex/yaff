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
    void ProcessMessage(NIR::TIR& ir, NIR::TMessageDef& messageDef);

    std::unordered_set<const NIR::TMessageDef*> ProcessedMessages_;
};

void TIRProcessor::Process(NIR::TIR& ir) && {
    for (auto& [_, schemaDef] : ir.Schemas.Symbols) {
        ProcessSchema(ir, schemaDef);
    }

    for (auto& [_, enumDef] : ir.Enums.Symbols) {
        ProcessEnum(ir, enumDef);
    }

    for (auto& [_, messageDef] : ir.Messages.Symbols) {
        if (!ProcessedMessages_.contains(&messageDef)) {
            ProcessMessage(ir, messageDef);
        }
    }
}

void TIRProcessor::ProcessSchema(NIR::TIR&, NIR::TSchemaDef& schemaDef) {
    const bool someEnums = std::any_of(schemaDef.Enums.begin(), schemaDef.Enums.end(),
                                       [](const NIR::TEnumDef* enm) { return enm->Defined; });
    const bool someMessages = std::any_of(schemaDef.Messages.begin(), schemaDef.Messages.end(),
                                          [](const NIR::TMessageDef* tbl) { return tbl->Defined; });
    schemaDef.Defined = (someEnums || someMessages);
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

void TIRProcessor::ProcessMessage(NIR::TIR& ir, NIR::TMessageDef& messageDef) {
    if (!messageDef.Defined) {
        return;
    }
    const auto [_, emplaced] = ProcessedMessages_.emplace(&messageDef);
    Ensure(emplaced, "duplicated process call for message '" + messageDef.Name + "'");

    Ensure(messageDef.Schema, "message can not be not connected to schema '" + messageDef.Name + "'");
    Ensure(messageDef.Defined == messageDef.Schema->Defined,
           "incomplete definition of message '" + messageDef.Name + "'");
    if (!messageDef.Defined) {
        return;
    }

    std::sort(messageDef.Fields.begin(), messageDef.Fields.end(),
              [](const auto& l, const auto& r) { return l.Id < r.Id; });

    const bool gapped = NIR::IsGapMessage(messageDef);
    Ensure(!gapped || (messageDef.Layout != EMessageLayout::MESSAGE_LAYOUT_FLAT &&
                       messageDef.Layout != EMessageLayout::MESSAGE_LAYOUT_FIXED),
           "message '" + messageDef.Name + "' can not contain any gaps because it has field size based layout");

    bool comparable = false;
    uint64_t activeIndex = 1;
    TFieldOffset flatOffset = 0;
    for (auto& fieldDef : messageDef.Fields) {
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

        if (fieldDef.Type->Type == EType::TYPE_MESSAGE) {
            Ensure(fieldDef.Deprecated || fieldDef.Presence == EPresence::PRESENCE_EXPLICIT,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is message, but has implicit presence");
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

            Ensure(!comparable, "message '" + messageDef.Name + "' contains multiple key fields");
            comparable = true;
        }

        if (!fieldDef.SharedOffsetVia.empty()) {
            Ensure(messageDef.Layout != EMessageLayout::MESSAGE_LAYOUT_FIXED,
                   "message '" + messageDef.Name + "' is fixed and can not have any oneof fields");
            Ensure(fieldDef.Type->Type != EType::TYPE_VECTOR, "field '" + fieldDef.Name +
                                                                  "' (id: " + std::to_string(fieldDef.Id) +
                                                                  ") is vector and can not be oneof field");
            Ensure(fieldDef.Deprecated || fieldDef.Presence == EPresence::PRESENCE_EXPLICIT,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is oneof field, but has implicit presence");

            auto [it, _] = messageDef.SharedOffsets.try_emplace(
                fieldDef.SharedOffsetVia, NIR::TMessageDef::TSharedOffset{.Name = fieldDef.SharedOffsetVia});
            it->second.Fields.emplace_back(&fieldDef);

            fieldDef.SharedOffsetId = it->second.Fields.size();
        }

        fieldDef.FlatOffset = flatOffset * !gapped;
        flatOffset += fieldDef.Type->InlineSize();
    }

    for (auto& fieldDef : messageDef.Fields) {
        const auto* fieldMessage = ExtractMessageDef(*fieldDef.Type);
        if (!fieldMessage) {
            continue;
        }

        auto* message = ir.Messages.Get(*fieldMessage);
        if (!ProcessedMessages_.contains(fieldMessage)) {
            ProcessMessage(ir, *message);
        }

        if (NIR::IsAssociative(*fieldDef.Type)) {
            message->AssociativePair = true;
            Ensure(message->Layout == EMessageLayout::MESSAGE_LAYOUT_FIXED,
                   "message '" + message->Name + "' marked as associative pair, but is not fixed");
            Ensure(message->Fields.size() == 2,
                   "message '" + message->Name + "' marked as associative pair, but contains not 2 fields");

            const auto& key = message->Fields[0];
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
