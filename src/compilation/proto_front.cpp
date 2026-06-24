#include "proto_front.h"

#include <google/protobuf/descriptor.pb.h>
#include <yaff/proto/options.pb.h>

namespace yaff::compilation {

template <typename T>
inline std::string AdaptString(T value) {
    return std::string{std::move(value)};
}

static bool IsMap(const google::protobuf::Descriptor& message) {
    return message.options().map_entry();
}

static MessageLayout TranslateMessageLayout(const yaff::proto::Message::Layout layout) {
    switch (layout) {
        case yaff::proto::Message::LAYOUT_UNKNOWN:
            return MessageLayout::MESSAGE_LAYOUT_UNKNOWN;
        case yaff::proto::Message::LAYOUT_FIXED:
            return MessageLayout::MESSAGE_LAYOUT_FIXED;
        case yaff::proto::Message::LAYOUT_FLAT:
            return MessageLayout::MESSAGE_LAYOUT_FLAT;
        case yaff::proto::Message::LAYOUT_SPARSE:
            return MessageLayout::MESSAGE_LAYOUT_SPARSE;
        case yaff::proto::Message::LAYOUT_DYNAMIC:
            return MessageLayout::MESSAGE_LAYOUT_DYNAMIC;
    }
}

static const yaff::proto::Field* GetFieldMeta(const google::protobuf::FieldOptions& fieldOptions,
                                              const std::string& sliceId = "") {
    const yaff::proto::Field* fallback = nullptr;
    const yaff::proto::Field* match = nullptr;
    for (int i = 0; i < fieldOptions.ExtensionSize(yaff::proto::field); ++i) {
        const auto& fieldMeta = fieldOptions.GetExtension(yaff::proto::field, i);
        const auto& sliceName = fieldMeta.slice_name();
        if (sliceName.empty()) {
            YAFF_REQUIRE(fallback == nullptr);
            fallback = &fieldMeta;
        }
        if (sliceName == sliceId) {
            YAFF_REQUIRE(match == nullptr);
            match = &fieldMeta;
        }
    }
    return (match ? match : fallback);
}

static const yaff::proto::Message* GetMessageMeta(const google::protobuf::MessageOptions& messageOptions,
                                                  const std::string& sliceId = "") {
    const yaff::proto::Message* fallback = nullptr;
    const yaff::proto::Message* match = nullptr;
    for (int i = 0; i < messageOptions.ExtensionSize(yaff::proto::message); ++i) {
        const auto& messageMeta = messageOptions.GetExtension(yaff::proto::message, i);
        const auto& sliceName = messageMeta.slice_name();
        if (sliceName.empty()) {
            YAFF_REQUIRE(fallback == nullptr);
            fallback = &messageMeta;
        }
        if (sliceName == sliceId) {
            YAFF_REQUIRE(match == nullptr);
            match = &messageMeta;
        }
    }
    return (match ? match : fallback);
}

static MessageLayout GetMessageLayout(const google::protobuf::Descriptor& message, const std::string& sliceId = "") {
    if (IsMap(message)) {
        return MessageLayout::MESSAGE_LAYOUT_FIXED;
    }
    if (const auto* messageMeta = GetMessageMeta(message.options(), sliceId)) {
        return TranslateMessageLayout(messageMeta->layout());
    }
    return MessageLayout::MESSAGE_LAYOUT_DYNAMIC;
}

static std::vector<ProtobufDeprecatedField> GetDeprecatedFields(const google::protobuf::Descriptor& message,
                                                                const std::string& sliceId = "") {
    if (const auto* messageMeta = GetMessageMeta(message.options(), sliceId)) {
        std::vector<ProtobufDeprecatedField> fields;
        fields.reserve(messageMeta->reserved_fields_size());
        for (const auto& deprecated : messageMeta->reserved_fields()) {
            const auto type = deprecated.has_type() ? deprecated.type() : 0;
            fields.emplace_back(ProtobufDeprecatedField{
                .Id = deprecated.id(),
                .Type = static_cast<google::protobuf::FieldDescriptor::Type>(type),
                .Label = static_cast<google::protobuf::FieldDescriptor::Label>(deprecated.label()),
            });
        }
        return fields;
    }
    return {};
}

static bool ShouldIgnore(const google::protobuf::Descriptor& message, const std::string& sliceId = "") {
    const auto& options = message.options();
    for (int i = 0; i < options.ExtensionSize(yaff::proto::message); ++i) {
        const auto& messageMeta = options.GetExtension(yaff::proto::message, i);
        if (messageMeta.slice_name().empty() || (!sliceId.empty() && messageMeta.slice_name() == sliceId)) {
            return messageMeta.ignore();
        }
    }
    return false;
}

static void FillAdditionalModifiers(const google::protobuf::FieldDescriptor& field, const std::string& sliceId,
                                    std::map<std::string, std::string>& modifiers,
                                    std::map<std::string, std::string>& elementModifiers) {
    if (field.is_map()) {
        modifiers.emplace(ir::ASSOCIATIVE_MODIFIER_NAME, "");
    }
    if (field.is_repeated() && field.message_type() &&
        GetMessageLayout(*field.message_type(), sliceId) == MessageLayout::MESSAGE_LAYOUT_FIXED) {
        elementModifiers.emplace(ir::INLINE_MODIFIER_NAME, "");
    }
}

std::optional<ProtobufField> DefaultProtobufReflectionTraits::GetYaffFields(
    const google::protobuf::FieldDescriptor& field) {
    const auto& options = field.options();
    if (options.deprecated()) {
        return std::nullopt;
    }
    const auto* fieldMeta = GetFieldMeta(options);

    ProtobufField protoField;
    protoField.Id = (fieldMeta && fieldMeta->id() > 0 ? fieldMeta->id() : field.number());
    FillAdditionalModifiers(field, "", protoField.Modifiers.Modifiers, protoField.Modifiers.ElementModifiers);

    return protoField;
}

std::optional<ProtobufMessage> DefaultProtobufReflectionTraits::GetYaffMessage(
    const google::protobuf::Descriptor& message) {
    if (ShouldIgnore(message)) {
        return std::nullopt;
    }
    std::vector<ProtobufDeprecatedField> deprecated = GetDeprecatedFields(message);
    for (int i = 0; i < message.field_count(); ++i) {
        const auto* field = message.field(i);
        if (field->options().deprecated()) {
            deprecated.emplace_back(ProtobufDeprecatedField{
                .Id = static_cast<uint64_t>(field->number()),
                .Type = field->type(),
                .Label = (field->is_repeated() ? google::protobuf::FieldDescriptor::LABEL_REPEATED
                                               : google::protobuf::FieldDescriptor::LABEL_OPTIONAL),
            });
        }
    }

    std::unordered_set<uint64_t> knownIds;
    knownIds.reserve(deprecated.size());
    for (const auto& field : deprecated) {
        knownIds.emplace(field.Id);
    }
    for (int i = 0; i < message.reserved_range_count(); ++i) {
        const auto* reservedRange = message.reserved_range(i);
        for (int id = reservedRange->start; id < reservedRange->end; ++id) {
            if (knownIds.contains(id)) {
                continue;
            }
            deprecated.emplace_back(ProtobufDeprecatedField{
                .Id = static_cast<uint64_t>(id),
                .Type = static_cast<google::protobuf::FieldDescriptor::Type>(0),
                .Label = static_cast<google::protobuf::FieldDescriptor::Label>(0),
            });
        }
    }
    return ProtobufMessage{.Layout = GetMessageLayout(message), .DeprecatedFields = std::move(deprecated)};
}

TaggedProtobufReflectionTraits::TaggedProtobufReflectionTraits(const std::string& sliceId) : SliceId_(sliceId) {
}

std::optional<ProtobufField> TaggedProtobufReflectionTraits::GetYaffFields(
    const google::protobuf::FieldDescriptor& field) {
    const auto& options = field.options();
    const auto* fieldMeta = GetFieldMeta(options, SliceId_);
    if (!fieldMeta) {
        return std::nullopt;
    }

    ProtobufField protoField;
    protoField.Id = (fieldMeta->id() > 0 ? fieldMeta->id() : field.number());
    FillAdditionalModifiers(field, SliceId_, protoField.Modifiers.Modifiers, protoField.Modifiers.ElementModifiers);

    return {std::move(protoField)};
}

std::optional<ProtobufMessage> TaggedProtobufReflectionTraits::GetYaffMessage(
    const google::protobuf::Descriptor& message) {
    if (ShouldIgnore(message, SliceId_)) {
        return std::nullopt;
    }
    const auto& options = message.options();
    for (int i = 0; i < options.ExtensionSize(yaff::proto::message); ++i) {
        const auto& messageMeta = options.GetExtension(yaff::proto::message, i);
        if (messageMeta.slice_name() == SliceId_) {
            return ProtobufMessage{
                .Layout = GetMessageLayout(message, SliceId_),
                .DeprecatedFields = GetDeprecatedFields(message, SliceId_),
            };
        }
    }
    return std::nullopt;
}

class TProtobufBuilder {
public:
    TProtobufBuilder(ProtobufReflectionTraits* traits, ProtobufReflectionFrontEndOptions opts)
        : Traits_(traits), Opts_(std::move(opts)) {
    }

    void AddDescriptor(const google::protobuf::Descriptor& message) {
        TraverseMessage(message);
    }

    void AddDescriptor(const google::protobuf::FileDescriptor& file) {
        TraverseFile(file);
    }

    ir::IR Finish() && {
        return std::move(Ir_);
    }

private:
    inline static const auto PROTO_TYPE_RESERVED = static_cast<google::protobuf::FieldDescriptor::Type>(0);

    struct TFieldInfo {
        ProtobufField TargetField;
        const google::protobuf::FieldDescriptor* SourceField = nullptr;
    };

    static std::vector<TFieldInfo> CollectFields(ProtobufReflectionTraits* traits,
                                                 const google::protobuf::Descriptor& message);
    static std::optional<ProtobufMessage> CollectMessage(ProtobufReflectionTraits* traits,
                                                         const google::protobuf::Descriptor& message);

    static std::string GetNamespaceName(const std::string& prefix, const std::string& pkg);
    static std::string GetTypeDefName(const std::string& pkg, const std::string& fullName);
    static Type GetBaseType(const google::protobuf::FieldDescriptor::Type type);

    static std::string GetDefaultFieldModifier(const google::protobuf::FieldDescriptor& field);
    static std::map<std::string, std::string> GetFieldModifiers(const google::protobuf::FieldDescriptor& field,
                                                                const ProtobufField::AdditionalModifiers& mods);

    const ir::MessageDef* TraverseMessage(const google::protobuf::Descriptor& message);
    void TraverseFile(const google::protobuf::FileDescriptor& message);

    ir::SchemaDef* RegisterFile(const google::protobuf::FileDescriptor& file);
    ir::EnumDef* RegisterEnum(const google::protobuf::EnumDescriptor& enm);
    ir::TypeDef* RegisterFieldType(const google::protobuf::FieldDescriptor& field,
                                   const ProtobufField::AdditionalModifiers& mods,
                                   const ir::MessageDef* messageDef = nullptr, const ir::EnumDef* enumDef = nullptr);
    ir::TypeDef* RegisterFieldTypeImpl(const google::protobuf::FieldDescriptor& field,
                                       const ProtobufField::AdditionalModifiers& mods,
                                       const ir::MessageDef* messageDef = nullptr,
                                       const ir::EnumDef* enumDef = nullptr);
    ir::TypeDef* RegisterDeprecatedFieldType(const ProtobufDeprecatedField& field);
    ir::TypeDef* RegisterReservedFieldType();
    void RegisterDependency(const ir::SchemaDef* from, ir::SchemaDef* to);

    std::vector<TFieldInfo> CollectFields(const google::protobuf::Descriptor& message);
    std::optional<ProtobufMessage> CollectMessage(const google::protobuf::Descriptor& message);

    bool IsTargetFile(const google::protobuf::FileDescriptor& file) const;

    ProtobufReflectionTraits* Traits_;
    ProtobufReflectionFrontEndOptions Opts_;
    ir::IR Ir_;
};

const ir::MessageDef* TProtobufBuilder::TraverseMessage(const google::protobuf::Descriptor& message) {
    std::vector<const ir::BaseDef*> nestedTypes;

    for (int i = 0; i < message.nested_type_count(); ++i) {
        if (const auto& nested = *message.nested_type(i); !IsMap(nested)) {
            if (const auto* next = TraverseMessage(nested); next) {
                nestedTypes.push_back(next);
            }
        }
    }
    for (int i = 0; i < message.enum_type_count(); ++i) {
        if (const auto* next = RegisterEnum(*message.enum_type(i)); next) {
            nestedTypes.push_back(next);
        }
    }

    const auto fields = CollectFields(message);
    if (Opts_.SkipEmptyMessages && fields.empty()) {
        return nullptr;
    }

    const auto mbMessage = CollectMessage(message);
    if (!mbMessage.has_value()) {
        return nullptr;
    }

    const auto* file = message.file();
    auto* schemaDef = RegisterFile(*file);

    const std::string messageName = GetTypeDefName(AdaptString(file->package()), AdaptString(message.full_name()));
    auto [messageDef, emplaced] =
        Ir_.Messages.TryEmplace(ir::MessageDef(messageName, schemaDef, std::move(nestedTypes)));
    if (!emplaced) {
        return messageDef;
    }
    schemaDef->Messages.emplace_back(messageDef);
    if (!IsTargetFile(*file)) {
        return messageDef;
    }

    uint64_t maxId = 0;
    std::unordered_set<uint64_t> knownIds;
    for (const auto& fieldInfo : fields) {
        const auto& target = fieldInfo.TargetField;
        const auto* field = fieldInfo.SourceField;

        // Register child types;
        const ir::EnumDef* childEnumDef = nullptr;
        const ir::MessageDef* childMessageDef = nullptr;
        const ir::SchemaDef* childSchemaDef = nullptr;
        if (field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
            childMessageDef = TraverseMessage(*field->message_type());
            if (!childMessageDef) {  // Type of field is ignored by frontend, so skip this field;
                continue;
            }
            childSchemaDef = childMessageDef->Schema;
        } else if (field->type() == google::protobuf::FieldDescriptor::TYPE_ENUM) {
            childEnumDef = RegisterEnum(*field->enum_type());
            childSchemaDef = childEnumDef->Schema;
        }

        if (childSchemaDef) {
            RegisterDependency(childSchemaDef, schemaDef);
        }

        const auto* fieldType = RegisterFieldType(*field, target.Modifiers, childMessageDef, childEnumDef);
        messageDef->Fields.emplace_back(ir::MessageDef::FieldDef{
            .Id = target.Id,
            .Name = AdaptString(field->name()),
            .Type = fieldType,
            .Presence = (field->has_presence() ? Presence::PRESENCE_EXPLICIT : Presence::PRESENCE_IMPLICIT),
            .OneOf = (field->real_containing_oneof() ? AdaptString(field->real_containing_oneof()->name()) : ""),
        });

        knownIds.emplace(target.Id);
        maxId = std::max<uint64_t>(maxId, target.Id);
    }

    for (const auto& field : mbMessage->DeprecatedFields) {
        // N.B.: reserved fields can be overridden using the (yaff.proto.id) markup,
        // so the existing field takes precedence over the reserved field.
        // If the reserved field has a specific type,
        // it will result in an invalid IR that will fail further validation.
        const bool skipReserved = (knownIds.contains(field.Id) || Opts_.SkipTypeReserved);
        if (field.Type == PROTO_TYPE_RESERVED && skipReserved) {
            continue;
        }
        messageDef->Fields.emplace_back(ir::MessageDef::FieldDef{
            .Id = field.Id,
            .Type = RegisterDeprecatedFieldType(field),
            .Deprecated = true,
        });
        knownIds.emplace(field.Id);
        maxId = std::max<uint64_t>(maxId, field.Id);
    }

    for (uint64_t id = 1; id < maxId * Opts_.FillGaps; ++id) {
        if (knownIds.contains(id)) {
            continue;
        }
        messageDef->Fields.emplace_back(ir::MessageDef::FieldDef{
            .Id = static_cast<uint64_t>(id),
            .Type = RegisterReservedFieldType(),
            .Deprecated = true,
        });
    }

    messageDef->Layout = mbMessage->Layout;
    messageDef->Defined = true;
    return messageDef;
}

void TProtobufBuilder::TraverseFile(const google::protobuf::FileDescriptor& file) {
    for (int i = 0; i < file.message_type_count(); ++i) {
        TraverseMessage(*file.message_type(i));
    }
    for (int i = 0; i < file.enum_type_count(); ++i) {
        RegisterEnum(*file.enum_type(i));
    }
}

ir::EnumDef* TProtobufBuilder::RegisterEnum(const google::protobuf::EnumDescriptor& enm) {
    const auto* file = enm.file();
    auto* schemaDef = RegisterFile(*file);

    const std::string enumName = GetTypeDefName(AdaptString(file->package()), AdaptString(enm.full_name()));
    auto [enumDef, emplaced] = Ir_.Enums.TryEmplace(ir::EnumDef(std::move(enumName), schemaDef));
    if (!emplaced) {
        return enumDef;
    }
    schemaDef->Enums.emplace_back(enumDef);
    if (!IsTargetFile(*file)) {
        return enumDef;
    }

    for (int i = 0; i < enm.value_count(); ++i) {
        const auto* v = enm.value(i);
        enumDef->Values.emplace_back(ir::EnumDef::TEnumVal{.Name = AdaptString(v->name()), .Value = v->number()});
    }

    enumDef->Defined = true;
    return enumDef;
}

ir::SchemaDef* TProtobufBuilder::RegisterFile(const google::protobuf::FileDescriptor& file) {
    const std::string name = AdaptString(file.name());
    const std::string fileName = name.substr(0, name.rfind('.'));

    auto [schemaDef, emplaced] = Ir_.Schemas.TryEmplace(ir::SchemaDef(fileName));
    if (!emplaced) {
        return schemaDef;
    }

    const std::string package = AdaptString(file.package());
    const std::string namespaceName = GetNamespaceName(Opts_.RootNamespace, package);
    schemaDef->Namespace = std::move(namespaceName);

    const std::string protoFileName = fileName + Opts_.GeneratedProtobufExt;
    schemaDef->Attributes.emplace(ir::PROTO_FILE_ATTRIBUTE_NAME, protoFileName);

    const std::string protoNamespaceName = GetNamespaceName("", package);
    schemaDef->Attributes.emplace(ir::PROTO_NAMESPACE_ATTRIBUTE_NAME, protoNamespaceName);

    return schemaDef;
}

ir::TypeDef* TProtobufBuilder::RegisterFieldType(const google::protobuf::FieldDescriptor& field,
                                                 const ProtobufField::AdditionalModifiers& mods,
                                                 const ir::MessageDef* messageDef, const ir::EnumDef* enumDef) {
    if (field.is_repeated()) {
        ProtobufField::AdditionalModifiers elemMods{.Modifiers = mods.ElementModifiers};
        return Ir_.Types.GetOrEmplace(
            ir::TypeDef{.Type = Type::TYPE_ARRAY,
                        .ElementType = RegisterFieldTypeImpl(field, elemMods, messageDef, enumDef),
                        .Modifiers = GetFieldModifiers(field, mods)});
    }
    return RegisterFieldTypeImpl(field, mods, messageDef, enumDef);
}

ir::TypeDef* TProtobufBuilder::RegisterFieldTypeImpl(const google::protobuf::FieldDescriptor& field,
                                                     const ProtobufField::AdditionalModifiers& mods,
                                                     const ir::MessageDef* messageDef, const ir::EnumDef* enumDef) {
    const auto fieldType = field.type();
    if (fieldType == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
        return Ir_.Types.GetOrEmplace(ir::TypeDef{
            .Type = Type::TYPE_MESSAGE, .MessageDef = messageDef, .Modifiers = GetFieldModifiers(field, mods)});
    }

    if (fieldType == google::protobuf::FieldDescriptor::TYPE_ENUM) {
        return Ir_.Types.GetOrEmplace(
            ir::TypeDef{.Type = Type::TYPE_ENUM, .EnumDef = enumDef, .Modifiers = GetFieldModifiers(field, mods)});
    }

    return Ir_.Types.GetOrEmplace(
        ir::TypeDef{.Type = GetBaseType(field.type()), .Modifiers = GetFieldModifiers(field, mods)});
}

ir::TypeDef* TProtobufBuilder::RegisterDeprecatedFieldType(const ProtobufDeprecatedField& field) {
    if (field.Label == google::protobuf::FieldDescriptor::LABEL_REPEATED) {
        return Ir_.Types.GetOrEmplace(ir::TypeDef{.Type = Type::TYPE_ARRAY});
    }

    if (field.Type == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
        return Ir_.Types.GetOrEmplace(ir::TypeDef{.Type = Type::TYPE_MESSAGE});
    }

    if (field.Type == google::protobuf::FieldDescriptor::TYPE_ENUM) {
        return Ir_.Types.GetOrEmplace(ir::TypeDef{.Type = Type::TYPE_INT32});
    }

    if (field.Type == PROTO_TYPE_RESERVED) {
        return Ir_.Types.GetOrEmplace(ir::TypeDef{.Type = Type::TYPE_NONE});
    }

    return Ir_.Types.GetOrEmplace(ir::TypeDef{.Type = GetBaseType(field.Type)});
}

ir::TypeDef* TProtobufBuilder::RegisterReservedFieldType() {
    return Ir_.Types.GetOrEmplace(ir::TypeDef{.Type = Type::TYPE_NONE});
}

void TProtobufBuilder::RegisterDependency(const ir::SchemaDef* from, ir::SchemaDef* to) {
    if (from == to) {  // All schemas are deduplicated in IR.
        return;
    }
    to->Dependencies.emplace(from);
}

std::vector<TProtobufBuilder::TFieldInfo> TProtobufBuilder::CollectFields(ProtobufReflectionTraits* traits,
                                                                          const google::protobuf::Descriptor& message) {
    std::vector<TFieldInfo> fields;
    fields.reserve(message.field_count());
    for (int i = 0; i < message.field_count(); ++i) {
        const auto* field = message.field(i);
        if (const auto mbField = traits->GetYaffFields(*field)) {
            fields.emplace_back(TFieldInfo{.TargetField = std::move(*mbField), .SourceField = field});
        }
    }

    return fields;
}

std::optional<ProtobufMessage> TProtobufBuilder::CollectMessage(ProtobufReflectionTraits* traits,
                                                                const google::protobuf::Descriptor& message) {
    return traits->GetYaffMessage(message);
}

std::vector<TProtobufBuilder::TFieldInfo> TProtobufBuilder::CollectFields(const google::protobuf::Descriptor& message) {
    if (IsMap(message)) {
        DefaultProtobufReflectionTraits traits;
        return CollectFields(&traits, message);
    }
    return CollectFields(Traits_, message);
}

std::optional<ProtobufMessage> TProtobufBuilder::CollectMessage(const google::protobuf::Descriptor& message) {
    if (IsMap(message)) {
        DefaultProtobufReflectionTraits traits;
        return CollectMessage(&traits, message);
    }
    return CollectMessage(Traits_, message);
}

bool TProtobufBuilder::IsTargetFile(const google::protobuf::FileDescriptor& file) const {
    return Opts_.TargetFiles.empty() || Opts_.TargetFiles.contains(AdaptString(file.name()));
}

std::string TProtobufBuilder::GetNamespaceName(const std::string& prefix, const std::string& pkg) {
    std::string ns = pkg;
    size_t pos = 0;
    while ((pos = ns.find('.', pos)) != std::string::npos) {
        ns.replace(pos, 1, "::");
        pos += 2;
    }
    return prefix + "::" + ns;
}

std::string TProtobufBuilder::GetTypeDefName(const std::string& pkg, const std::string& fullName) {
    std::string messageName = fullName;
    if (!pkg.empty() && fullName.find(pkg) == 0) {
        messageName = fullName.substr(pkg.size() + 1);  // + 1 for dot.
    }
    std::replace(messageName.begin(), messageName.end(), '.', '_');
    return messageName;
}

Type TProtobufBuilder::GetBaseType(const google::protobuf::FieldDescriptor::Type type) {
    switch (type) {
        case google::protobuf::FieldDescriptor::TYPE_BOOL:
            return Type::TYPE_BOOL;
        case google::protobuf::FieldDescriptor::TYPE_INT32:
        case google::protobuf::FieldDescriptor::TYPE_SINT32:
        case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
            return Type::TYPE_INT32;
        case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
            return Type::TYPE_UINT32;
        case google::protobuf::FieldDescriptor::TYPE_INT64:
        case google::protobuf::FieldDescriptor::TYPE_SINT64:
        case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
            return Type::TYPE_INT64;
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
        case google::protobuf::FieldDescriptor::TYPE_FIXED64:
            return Type::TYPE_UINT64;
        case google::protobuf::FieldDescriptor::TYPE_FLOAT:
            return Type::TYPE_FLOAT;
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
            return Type::TYPE_DOUBLE;
        case google::protobuf::FieldDescriptor::TYPE_STRING:
        case google::protobuf::FieldDescriptor::TYPE_BYTES:
            return Type::TYPE_STRING;
        default:
            YAFF_THROW("unknown base type");
    }
}

std::map<std::string, std::string> TProtobufBuilder::GetFieldModifiers(const google::protobuf::FieldDescriptor& field,
                                                                       const ProtobufField::AdditionalModifiers& mods) {
    std::map<std::string, std::string> modifiers = mods.Modifiers;
    if (field.has_default_value()) {
        modifiers.emplace(ir::DEFAULT_MODIFIER_NAME, GetDefaultFieldModifier(field));
    }
    return modifiers;
}

std::string TProtobufBuilder::GetDefaultFieldModifier(const google::protobuf::FieldDescriptor& field) {
    switch (field.type()) {
        case google::protobuf::FieldDescriptor::TYPE_BOOL:
            return std::to_string(field.default_value_bool());
        case google::protobuf::FieldDescriptor::TYPE_INT32:
        case google::protobuf::FieldDescriptor::TYPE_SINT32:
        case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
            return std::to_string(field.default_value_int32());
        case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        case google::protobuf::FieldDescriptor::TYPE_UINT32:
            return std::to_string(field.default_value_uint32());
        case google::protobuf::FieldDescriptor::TYPE_INT64:
        case google::protobuf::FieldDescriptor::TYPE_SINT64:
        case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
            return std::to_string(field.default_value_int64());
        case google::protobuf::FieldDescriptor::TYPE_UINT64:
        case google::protobuf::FieldDescriptor::TYPE_FIXED64:
            return std::to_string(field.default_value_uint64());
        // TODO: std::to_string rounds small double values, so these defaults may break.
        // This needs to be fixed by moving to the correct default descriptors.
        case google::protobuf::FieldDescriptor::TYPE_FLOAT:
            return std::to_string(field.default_value_float());
        case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
            return std::to_string(field.default_value_double());
        case google::protobuf::FieldDescriptor::TYPE_ENUM:
            return AdaptString(field.default_value_enum()->name());
        case google::protobuf::FieldDescriptor::TYPE_STRING:
        case google::protobuf::FieldDescriptor::TYPE_BYTES:
            return AdaptString(field.default_value_string());
        default:
            YAFF_THROW("unknown protobuf type");
    }
}

ProtobufReflectionFrontEnd::ProtobufReflectionFrontEnd(
    const std::vector<const google::protobuf::Descriptor*>& descriptors,
    std::unique_ptr<ProtobufReflectionTraits> traits, ProtobufReflectionFrontEndOptions opts)
    : Descriptors_(descriptors), FileDescriptor_(nullptr), Traits_(std::move(traits)), Opts_(std::move(opts)) {
}

ProtobufReflectionFrontEnd::ProtobufReflectionFrontEnd(const google::protobuf::FileDescriptor* descriptor,
                                                       std::unique_ptr<ProtobufReflectionTraits> traits,
                                                       ProtobufReflectionFrontEndOptions opts)
    : Descriptors_(), FileDescriptor_(descriptor), Traits_(std::move(traits)), Opts_(std::move(opts)) {
}

ir::IR ProtobufReflectionFrontEnd::Parse() {
    TProtobufBuilder builder{Traits_.get(), Opts_};
    if (FileDescriptor_) {
        builder.AddDescriptor(*FileDescriptor_);
    } else {
        for (const auto* descriptor : Descriptors_) {
            YAFF_REQUIRE(descriptor);
            builder.AddDescriptor(*descriptor);
        }
    }
    return std::move(builder).Finish();
}

}  // namespace yaff::compilation
