#include "ir_processor.h"

#include <sstream>
#include <unordered_set>

namespace yaff::compilation {

class IRProcessor {
public:
    void Process(ir::IR& ir) &&;

private:
    template <typename... MessagePartT>
    static void Ensure(const bool cond, const MessagePartT&... message);

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
    Ensure(enumDef.Schema, "enum '", enumDef.Name, "' must belong to a schema");
    Ensure(enumDef.Defined == enumDef.Schema->Defined, "enum '", enumDef.Name,
           "' definition status does not match that of schema '", enumDef.Schema->Namespace, "'");
    if (!enumDef.Defined) {
        return;
    }

    std::stable_sort(enumDef.Values.begin(), enumDef.Values.end(),
                     [](const auto& l, const auto& r) { return l.Value < r.Value; });
}

void IRProcessor::ProcessMessage(ir::IR& ir, ir::MessageDef& messageDef) {
    if (!messageDef.Defined) {
        return;
    }
    const auto [_, emplaced] = ProcessedMessages_.emplace(&messageDef);
    Ensure(emplaced, "message '", messageDef.Name, "' was processed more than once");

    Ensure(messageDef.Schema, "message '", messageDef.Name, "' must belong to a schema");
    Ensure(messageDef.Defined == messageDef.Schema->Defined, "message '", messageDef.Name,
           "' definition status does not match that of schema '", messageDef.Schema->Namespace, "'");
    if (!messageDef.Defined) {
        return;
    }

    std::sort(messageDef.Fields.begin(), messageDef.Fields.end(),
              [](const auto& l, const auto& r) { return l.Id < r.Id; });

    const bool gapped = ir::IsGapMessage(messageDef);
    Ensure(!gapped || (messageDef.Layout != MessageLayout::MESSAGE_LAYOUT_FLAT &&
                       messageDef.Layout != MessageLayout::MESSAGE_LAYOUT_FIXED),
           "message '", messageDef.Name, "' with a field-size-based layout cannot contain gaps");

    bool comparable = false;
    uint64_t activeIndex = 1;
    FieldOffset flatOffset = 0;
    for (auto& fieldDef : messageDef.Fields) {
        Ensure(!fieldDef.Name.empty() || fieldDef.Deprecated, "field with ID '", fieldDef.Id, "' has an empty name");

        if (fieldDef.Type->Modifiers.contains(ir::DEFAULT_MODIFIER_NAME)) {
            Ensure(ir::IsScalar(fieldDef.Type->Type) || fieldDef.Type->Type == Type::TYPE_STRING, "field '",
                   fieldDef.Name, "' (ID: ", fieldDef.Id, ") cannot have a default value");
        }

        Ensure(!fieldDef.Type->Modifiers.contains(ir::INLINE_MODIFIER_NAME), "field '", fieldDef.Name,
               "' (ID: ", fieldDef.Id, ") is inline, but inline fields are not supported yet");

        if (fieldDef.Type->Type == Type::TYPE_ARRAY) {
            const auto* elementType = fieldDef.Type->ElementType;
            Ensure(!elementType || elementType->Type != Type::TYPE_ARRAY, "field '", fieldDef.Name,
                   "' (ID: ", fieldDef.Id, ") is a 2D array, but 2D arrays are not supported");
            Ensure(fieldDef.Deprecated || fieldDef.Presence == Presence::PRESENCE_IMPLICIT, "array field '",
                   fieldDef.Name, "' (ID: ", fieldDef.Id, ") must have implicit presence");
        }

        if (fieldDef.Type->Type == Type::TYPE_MESSAGE) {
            Ensure(fieldDef.Deprecated || fieldDef.Presence == Presence::PRESENCE_EXPLICIT, "message field '",
                   fieldDef.Name, "' (ID: ", fieldDef.Id, ") must have explicit presence");
        }

        Ensure(fieldDef.ActiveIndex == 0, "field '", fieldDef.Name, "' (ID: ", fieldDef.Id,
               ") has a non-zero active index");

        if (!fieldDef.Deprecated) {
            fieldDef.ActiveIndex = activeIndex++;
        }

        if (fieldDef.Type->Modifiers.contains(ir::KEY_MODIFIER_NAME)) {
            Ensure(!fieldDef.Deprecated, "deprecated field (ID: ", fieldDef.Id, ") cannot be a key");
            Ensure(ir::IsScalar(fieldDef.Type->Type) || fieldDef.Type->Type == Type::TYPE_STRING, "key field '",
                   fieldDef.Name, "' (ID: ", fieldDef.Id, ") must be scalar or string");

            Ensure(!comparable, "message '", messageDef.Name, "' cannot contain multiple key fields");
            comparable = true;
        }

        if (!fieldDef.OneOf.empty()) {
            Ensure(messageDef.Layout != MessageLayout::MESSAGE_LAYOUT_FIXED, "message '", messageDef.Name,
                   "' with a fixed layout cannot contain oneof fields");
            Ensure(fieldDef.Type->Type != Type::TYPE_ARRAY, "field '", fieldDef.Name, "' (ID: ", fieldDef.Id,
                   ") is an array and cannot be a oneof member");
            Ensure(fieldDef.Deprecated || fieldDef.Presence == Presence::PRESENCE_EXPLICIT, "oneof field '",
                   fieldDef.Name, "' (ID: ", fieldDef.Id, ") must have explicit presence");

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
            Ensure(message->Layout == MessageLayout::MESSAGE_LAYOUT_FIXED, "message '", message->Name,
                   "' marked as an associative pair must have a fixed layout");
            Ensure(message->Fields.size() == 2, "message '", message->Name,
                   "' marked as an associative pair must contain exactly two fields");

            const auto& key = message->Fields[0];
            Ensure(!key.Deprecated, "deprecated field '", key.Name, "' (ID: ", key.Id, ") cannot be a map key");
            Ensure(ir::IsScalar(key.Type->Type) || key.Type->Type == Type::TYPE_STRING, "map key field '", key.Name,
                   "' (ID: ", key.Id, ") must be scalar or string");
        }
    }
}

template <typename... MessageArgT>
void IRProcessor::Ensure(const bool cond, const MessageArgT&... message) {
    if (YAFF_UNLIKELY(!cond)) {
        std::ostringstream oss;
        ((oss << message), ...);
        throw std::runtime_error(oss.str());
    }
}

void ProcessIR(ir::IR& ir) {
    IRProcessor{}.Process(ir);
}

}  // namespace yaff::compilation
