#include "ir_processor.h"

#include <unordered_set>

namespace yaff::compilation {

class IRProcessor {
public:
    void Process(ir::IR& ir) &&;

private:
    static void Ensure(const bool cond, const std::string& message);

    void ProcessSchema(ir::IR& ir, ir::SchemaDef& schemaDef);
    void ProcessEnum(ir::IR& ir, ir::EnumDef& enumDef);
    void ProcessMessage(ir::IR& ir, ir::MessageDef& messageDef);

    std::unordered_set<const ir::MessageDef*> ProcessedMessages_;
};

void IRProcessor::Process(ir::IR& ir) && {
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

void IRProcessor::ProcessSchema(ir::IR&, ir::SchemaDef& schemaDef) {
    const bool someEnums = std::any_of(schemaDef.Enums.begin(), schemaDef.Enums.end(),
                                       [](const ir::EnumDef* enm) { return enm->Defined; });
    const bool someMessages = std::any_of(schemaDef.Messages.begin(), schemaDef.Messages.end(),
                                          [](const ir::MessageDef* tbl) { return tbl->Defined; });
    schemaDef.Defined = (someEnums || someMessages);
}

void IRProcessor::ProcessEnum(ir::IR&, ir::EnumDef& enumDef) {
    Ensure(enumDef.Schema, "enum can not be not connected to schema '" + enumDef.Name + "'");
    Ensure(enumDef.Defined == enumDef.Schema->Defined, "incomplete definition of enum '" + enumDef.Name +
                                                           "' in defined schema '" + enumDef.Schema->Namespace + "'");
    if (!enumDef.Defined) {
        return;
    }

    std::sort(enumDef.Values.begin(), enumDef.Values.end(),
              [](const auto& l, const auto& r) { return l.Value < r.Value; });
}

void IRProcessor::ProcessMessage(ir::IR& ir, ir::MessageDef& messageDef) {
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

    const bool gapped = ir::IsGapMessage(messageDef);
    Ensure(!gapped || (messageDef.Layout != MessageLayout::MESSAGE_LAYOUT_FLAT &&
                       messageDef.Layout != MessageLayout::MESSAGE_LAYOUT_FIXED),
           "message '" + messageDef.Name + "' can not contain any gaps because it has field size based layout");

    bool comparable = false;
    uint64_t activeIndex = 1;
    FieldOffset flatOffset = 0;
    for (auto& fieldDef : messageDef.Fields) {
        Ensure(!fieldDef.Name.empty() || fieldDef.Deprecated,
               "field with id '" + std::to_string(fieldDef.Id) + "' has empty name");

        if (fieldDef.Type->Modifiers.contains(ir::DEFAULT_MODIFIER_NAME)) {
            Ensure(ir::IsScalar(fieldDef.Type->Type) || fieldDef.Type->Type == Type::TYPE_STRING,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) + ") " +
                       "not allowed to have default value");
        }

        Ensure(!fieldDef.Type->Modifiers.contains(ir::INLINE_MODIFIER_NAME),
               "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                   ") is inlined, but inlined fields are not supported yet");

        if (fieldDef.Type->Type == Type::TYPE_ARRAY) {
            const auto* elementType = fieldDef.Type->ElementType;
            Ensure(!elementType || elementType->Type != Type::TYPE_ARRAY,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is 2d array, but 2d arrays are not supported");
            Ensure(fieldDef.Deprecated || fieldDef.Presence == Presence::PRESENCE_IMPLICIT,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is array, but has explicit presence");
        }

        if (fieldDef.Type->Type == Type::TYPE_MESSAGE) {
            Ensure(fieldDef.Deprecated || fieldDef.Presence == Presence::PRESENCE_EXPLICIT,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is message, but has implicit presence");
        }

        Ensure(fieldDef.ActiveIndex == 0,
               "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) + ") has non-zero active index");

        if (!fieldDef.Deprecated) {
            fieldDef.ActiveIndex = activeIndex++;
        }

        if (fieldDef.Type->Modifiers.contains(ir::KEY_MODIFIER_NAME)) {
            Ensure(!fieldDef.Deprecated,
                   "field (id: " + std::to_string(fieldDef.Id) + ") is deprecated and can not be key");
            Ensure(ir::IsScalar(fieldDef.Type->Type) || fieldDef.Type->Type == Type::TYPE_STRING,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is not scalar or string and can not be key");

            Ensure(!comparable, "message '" + messageDef.Name + "' contains multiple key fields");
            comparable = true;
        }

        if (!fieldDef.OneOf.empty()) {
            Ensure(messageDef.Layout != MessageLayout::MESSAGE_LAYOUT_FIXED,
                   "message '" + messageDef.Name + "' is fixed and can not have any oneof fields");
            Ensure(fieldDef.Type->Type != Type::TYPE_ARRAY, "field '" + fieldDef.Name +
                                                                "' (id: " + std::to_string(fieldDef.Id) +
                                                                ") is array and can not be oneof field");
            Ensure(fieldDef.Deprecated || fieldDef.Presence == Presence::PRESENCE_EXPLICIT,
                   "field '" + fieldDef.Name + "' (id: " + std::to_string(fieldDef.Id) +
                       ") is oneof field, but has implicit presence");

            auto [it, _] =
                messageDef.OneOfs.try_emplace(fieldDef.OneOf, ir::MessageDef::OneOfDef{.Name = fieldDef.OneOf});
            it->second.Fields.emplace_back(&fieldDef);
        }

        fieldDef.FlatOffset = flatOffset;
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

        if (ir::IsAssociative(*fieldDef.Type)) {
            message->AssociativePair = true;
            Ensure(message->Layout == MessageLayout::MESSAGE_LAYOUT_FIXED,
                   "message '" + message->Name + "' marked as associative pair, but is not fixed");
            Ensure(message->Fields.size() == 2,
                   "message '" + message->Name + "' marked as associative pair, but contains not 2 fields");

            const auto& key = message->Fields[0];
            Ensure(!key.Deprecated, "field (id: " + std::to_string(key.Id) + ") is deprecated and can not be map key");
            Ensure(ir::IsScalar(key.Type->Type) || key.Type->Type == Type::TYPE_STRING,
                   "field '" + key.Name + "' (id: " + std::to_string(key.Id) +
                       ") is not scalar or string and can not be map key");
        }
    }
}

void IRProcessor::Ensure(const bool cond, const std::string& message) {
    if (YAFF_UNLIKELY(!cond)) {
        throw std::runtime_error(message.c_str());
    }
}

void ProcessIR(ir::IR& ir) {
    IRProcessor{}.Process(ir);
}

}  // namespace yaff::compilation
