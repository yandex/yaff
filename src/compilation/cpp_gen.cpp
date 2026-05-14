#include "cpp_gen.h"

#include <yaff/base.h>
#include <yaff/util.h>

#include <functional>
#include <string>

#include "compilation/ir.h"
#include "ir_reflect.h"
#include "util.h"

using namespace std::placeholders;

namespace yaff::compilation {

static bool IsSizeableField(const ir::MessageDef::FieldDef& field) {
    return field.Type->Type == Type::TYPE_ARRAY && !ir::IsAssociative(*field.Type) &&
           !ir::IsSequentialMessage(*field.Type);
}

template <typename D>
static bool HasRealFields(const D& def) {
    return std::any_of(def.Fields.begin(), def.Fields.end(),
                       [](const auto& field) { return !std::invoke(&ir::MessageDef::FieldDef::Deprecated, field); });
}

template <typename D>
static bool HasExplicitFields(const D& def) {
    return std::any_of(def.Fields.begin(), def.Fields.end(), [](const auto& field) {
        return std::invoke(&ir::MessageDef::FieldDef::Presence, field) == Presence::PRESENCE_EXPLICIT;
    });
}

template <typename D, typename F>
static void ForFields(const D& def, F&& cb) {
    const auto& fields = def.Fields;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        cb(*it);
    }
}

template <typename D, typename F>
static void ForReversedFields(const D& def, F&& cb) {
    const auto& fields = def.Fields;
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        cb(*it);
    }
}

template <typename D, typename F>
static void ForRealFields(const D& def, F&& cb) {
    ForFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        if (!std::invoke(&ir::MessageDef::FieldDef::Deprecated, fieldDef)) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForReversedRealFields(const D& def, F&& cb) {
    ForReversedFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        if (!std::invoke(&ir::MessageDef::FieldDef::Deprecated, fieldDef)) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForRealScalarFields(const D& def, F&& cb) {
    ForRealFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        const auto* type = std::invoke(&ir::MessageDef::FieldDef::Type, fieldDef);
        if (ir::IsScalar(type->Type)) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForRealStructureFields(const D& def, F&& cb) {
    ForRealFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        const auto* type = std::invoke(&ir::MessageDef::FieldDef::Type, fieldDef);
        if (!ir::IsScalar(type->Type)) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForRealStringFields(const D& def, F&& cb) {
    ForRealFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        const auto* type = std::invoke(&ir::MessageDef::FieldDef::Type, fieldDef);
        if (type->Type == Type::TYPE_STRING) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForOneOfs(const D& def, F&& cb) {
    const auto& oneOfs = def.OneOfs;
    for (auto it = oneOfs.begin(); it != oneOfs.end(); ++it) {
        cb(it->second);
    }
}

template <typename D, typename F>
static void ForRealOneOfs(const D& def, F&& cb) {
    ForOneOfs(def, [cb = std::move(cb)](const auto& oneOf) {
        if (HasRealFields(oneOf)) {
            cb(oneOf);
        }
    });
}

template <typename D, typename F>
static void ForValues(const D& def, F&& cb) {
    const auto& values = def.Values;
    for (auto it = values.begin(); it != values.end(); ++it) {
        cb(*it);
    }
}

class CppGenerator::Impl {
public:
    Impl(std::ostream& out, CppGenerationOptions opts = {});

    void Generate(const ir::IR& ir);

private:
    enum class MessageType : int32_t {
        MESSAGE_TYPE_FIXED = 0,
        MESSAGE_TYPE_FLAT = 1,
        MESSAGE_TYPE_SPARSE = 2,
    };

    struct TypeIndex {
        using Dict = std::unordered_map<const ir::TypeDef*, size_t>;
        using List = std::vector<Dict::value_type*>;

        Dict D;
        List L;

        void Add(const ir::TypeDef* t) {
            auto [it, emplaced] = D.try_emplace(t);
            if (emplaced) {
                it->second = L.size();
                L.emplace_back(&*it);
            }
        }

        size_t At(const ir::TypeDef* t) const {
            const size_t* idx = FindKey(D, t);
            YAFF_REQUIRE(idx);
            return *idx;
        }

        template <typename F>
        void ForEach(F&& cb) const {
            for (const auto* it : L) {
                cb(*it->first);
            }
        }
    };

    struct MessageMeta {
        std::vector<FieldOffset> FlatOffsets;
        std::vector<FieldId> DeletedIds;
        std::vector<bool> StaticFlags;
    };

    using MessageLayoutGenerator = std::function<void(MessageType, const ir::MessageDef&)>;

    static MessageMeta GenerateMessageMeta(const ir::MessageDef& msgDef);

    // External experimental generators;
    static std::string GenerateMessageColumnName(const ir::MessageDef& msgDef);

    static std::string GenerateMessageType(const MessageType type);
    static std::string GenerateBasicType(const Type type);
    static std::string GenerateTypeProtobuf(const ir::TypeDef& type);
    static std::string GenerateTypeInternal(const ir::TypeDef& type);
    static std::string GenerateTypeExternal(const ir::TypeDef& type);
    static std::string GenerateGetterReturnType(const ir::TypeDef& type);

    static std::string GenerateBasicDefault(const Type type);
    static std::string GenerateStringDefault(const ir::TypeDef& type);
    static std::string GenerateDefaultValueInternal(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateDefaultValueExternal(const ir::MessageDef::FieldDef& fieldDef);

    static std::string GenerateTypeCastInternal(const ir::TypeDef& type, const std::string& call);
    static std::string GenerateTypeCastExternal(const ir::TypeDef& type, const std::string& call);

    static std::string GenerateBasicTypeDescriptor(const Type type);
    static std::string GenerateCompositeTypeDescriptor(const ir::TypeDef& type, const TypeIndex& index);
    static std::string GeneratePresenceDescriptor(const Presence presence);
    static std::string GenerateMessageLayoutDescriptor(const MessageLayout layout);
    static std::string GenerateDefaultDescriptor(const ir::TypeDef& type);
    static std::string GenerateDescriptorFuncName(const std::string& name);
    static std::string GenerateEnumDescriptorFuncDeclaration(const std::string& name);
    static std::string GenerateMessageDescriptorFuncDeclaration(const std::string& name);

    static std::string GenerateDefaultFuncName(const std::string& name);
    static std::string GenerateDefaultFuncDeclaration(const std::string& ret, const std::string& name);

    static std::string GenerateProtobufValue(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateProtobufPairValue(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateProtobufPairFill(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateProtobufStructFill(const std::string& mutableCall, const std::string& clearCall,
                                                  const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateProtobufScalarFill(const std::string& mutableCall, const std::string& clearCall,
                                                  const ir::MessageDef::FieldDef& fieldDef);

    static std::string GenerateExternalPairName(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateExternalSerialize(const std::string& getCall, const std::string& getSuffix,
                                                 const ir::TypeDef& type);
    static std::string GenerateDeferredSerializeFuncName(const ir::MessageDef& msgDef);
    static std::string GenerateSerializeFuncName(const ir::MessageDef& msgDef);
    static std::string GenerateTransformFuncName(const ir::MessageDef& msgDef);

    static std::string GenerateDeferredSerializeProtobufFuncDeclaration(const ir::MessageDef& msgDef);
    static std::string GenerateSerializeProtobufFuncDeclaration(const ir::MessageDef& msgDef);
    static std::string GenerateParseToProtobufFuncDeclaration(const ir::MessageDef& msgDef);

    static std::string GenerateMessageSerializeFuncArgument(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateMessageBasicSerializeFuncCall(const ir::MessageDef& msgDef,
                                                             const std::string& templateArg = "");
    static std::string GenerateMessageBasicSerializeFuncDeferredCall(const ir::MessageDef& msgDef,
                                                                     const std::string& templateArg = "");
    static std::string GenerateMessageBasicSerializeFuncDefault(const ir::MessageDef& msgDef);

    static std::string GenerateEscapedName(const std::string& input);
    static std::string GenerateFieldName(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateDefaultName(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateIdName(const ir::MessageDef::FieldDef& fieldDef);
    static std::string GenerateOneOfCaseEnumName(const std::string& name);
    static std::string GenerateOneOfDefaultCaseEnumName(const std::string& name);
    static std::string GenerateOneOfFieldName(const std::string& name, const ir::MessageDef::FieldDef* fieldDef);
    static std::string GenerateMessageSerializerName(MessageType msgType, const ir::MessageDef& msgDef);
    static std::string GenerateMessageStaticMetaName(const ir::MessageDef& msgDef);
    static std::string GenerateMessageProtobufName(const ir::MessageDef& msgDef);
    static std::string GenerateMessageProtobufPair(const ir::MessageDef& msgDef);
    static std::string GenerateMessageProtobufMap(const ir::MessageDef& msgDef);
    static std::string GenerateMessageProtobufType(const ir::MessageDef& msgDef);
    static std::string GenerateMessageBaseClass(const ir::MessageDef& msgDef);
    static std::string GenerateEnumProtobufName(const ir::EnumDef& enumDef);
    static std::string GenerateWithNamespaceName(const std::string& ns, const std::string& name);

    void GenerateHeader();
    void GenerateIncludes(const ir::SchemaDef& schema);
    void GenerateSchema(const ir::SchemaDef& ir);

    void GenerateEnum(const ir::EnumDef& enumDef);
    void GenerateEnumPre(const ir::EnumDef& enumDef);
    void GenerateEnumPost(const ir::EnumDef& enumDef);

    void GenerateMessage(const ir::MessageDef& msgDef);
    void GenerateMessagePre(const ir::MessageDef& msgDef);
    void GenerateMessagePost(const ir::MessageDef& msgDef);

    void GenerateMessageFieldDescriptors(const ir::MessageDef& msgDef);
    void GenerateMessageDescriptor(const ir::MessageDef& msgDef);
    void GenerateMessageAliasDescriptor(const ir::MessageDef& msgDef);
    void GenerateTypeDescriptor(const ir::TypeDef& type);

    void GenerateEnumValueDescriptors(const ir::EnumDef& enumDef);
    void GenerateEnumDescriptor(const ir::EnumDef& enumDef);
    void GenerateEnumStruct(const ir::EnumDef& enumDef);
    void GenerateEnumConst(const ir::EnumDef& enumDef);
    void GenerateEnumIsValidFunc(const ir::EnumDef& enumDef);
    void GenerateEnumNameFunc(const ir::EnumDef& enumDef);

    void GenerateMessageParseToProtobuf(const ir::MessageDef& msgDef);
    void GenerateMessageProtobufSerializer(const ir::MessageDef& msgDef, const bool deferred = false);
    void GenerateMessageAliasSerializeFunc(const ir::MessageDef& msgDef);
    void GenerateMessageAliasDeferredSerializeFunc(const ir::MessageDef& msgDef);
    void GenerateMessageAliasTransformFunc(const ir::MessageDef& msgDef);
    void GenerateMessageBasicSerializeFunc(const ir::MessageDef& msgDef);
    void GenerateMessageDynamicSerializeFunc(const ir::MessageDef& msgDef);
    void GenerateMessageSerializer(MessageType msgType, const ir::MessageDef& msgDef);
    void GenerateMessageIdsEnum(const ir::MessageDef& msgDef);
    void GenerateMessageStaticMeta(const ir::MessageDef& msgDef);
    void GenerateMessageAliasDefaultFunc(const ir::MessageDef& msgDef);
    void GenerateMessageDefaultFunc(const ir::MessageDef& msgDef);
    void GenerateMessageFieldGet(const ir::MessageDef::FieldDef& fieldDef);
    void GenerateMessageFieldGetIdx(const ir::MessageDef::FieldDef& fieldDef);
    void GenerateMessageFieldCheck(const ir::MessageDef::FieldDef& fieldDef);
    void GenerateMessageFieldSize(const ir::MessageDef::FieldDef& fieldDef);
    void GenerateMessageFieldCompare(const std::string& msgName, const ir::MessageDef::FieldDef& fieldDef);
    void GenerateMessageFieldAdd(const std::string& msgName, const ir::MessageDef::FieldDef& fieldDef);
    void GenerateMessageOneOfCase(const ir::MessageDef::OneOfDef& oneOf);
    void GenerateByMessageLayout(const ir::MessageDef& msgDef, MessageLayoutGenerator gen);

    CppGenerationOptions Opts_;
    CodeWriter Writer_;
};

CppGenerator::Impl::Impl(std::ostream& out, CppGenerationOptions opts) : Opts_(std::move(opts)), Writer_(out, "\t") {
}

void CppGenerator::Impl::Generate(const ir::IR& ir) {
    auto schemaOrder = GetSchemaDependencyOrder(ir);
    for (const auto* schema : schemaOrder) {
        if (schema->Defined) {
            GenerateSchema(*schema);
        }
    }
}

void CppGenerator::Impl::GenerateSchema(const ir::SchemaDef& schema) {
    GenerateHeader();
    GenerateIncludes(schema);

    const bool includeProto = (Opts_.GenerateProtobufApi && !Opts_.SuppressProtobufIncludes);
    if (includeProto) {
        const std::string* protoPath = FindKey(schema.Attributes, ir::PROTO_FILE_ATTRIBUTE_NAME);
        YAFF_REQUIRE(protoPath);
        Writer_ |= "#include \"" + *protoPath + "\"\n";
    }

    if (!schema.Namespace.empty()) {
        Writer_ |= "namespace " + schema.Namespace + " {\n";
    }

    const auto& enums = schema.Enums;
    const auto& messages = GetMessageDependencyOrder(schema);
    for (const auto* enumDef : enums) {
        GenerateEnumPre(*enumDef);
    }

    for (const auto* msgDef : messages) {
        GenerateMessagePre(*msgDef);
    }

    for (const auto* enumDef : enums) {
        GenerateEnum(*enumDef);
    }

    for (const auto* msgDef : messages) {
        GenerateMessage(*msgDef);
    }

    for (const auto* enumDef : enums) {
        GenerateEnumPost(*enumDef);
    }

    for (const auto* msgDef : messages) {
        GenerateMessagePost(*msgDef);
    }

    if (!schema.Namespace.empty()) {
        Writer_ |= "} // namespace " + schema.Namespace + "\n";
    }
}

void CppGenerator::Impl::GenerateHeader() {
    Writer_ |= "// Automatically generated by the YaFF compiler, do not edit it manually\n";
    Writer_ |= "#pragma once\n";

    Writer_ |= "#include <yaff/yaff.h>";
    if (Opts_.GenerateReflectionApi) {
        Writer_ |= "#include <yaff/reflect.h>";
    }
    Writer_ |= "";

    for (const auto& incl : Opts_.ExternalIncludes) {
        Writer_ |= "#include \"" + incl + "\"";
    }
    if (!Opts_.ExternalIncludes.empty()) {
        Writer_ |= "";
    }

    Writer_ |= "#if YAFF_VERSION != " YAFF_STRINGIFY(YAFF_VERSION);
    Writer_ |= "#error \"YaFF version mismatch: this generated file requires another version of support library.\"";
    Writer_ |= "#error \"Regenerate this file with the matching YaFF compiler";
    Writer_ |= "#error \"or link against a compatible YaFF support library.\"";
    Writer_ |= "#endif\n";
}

void CppGenerator::Impl::GenerateIncludes(const ir::SchemaDef& schema) {
    for (const auto* depend : schema.Dependencies) {
        if (!depend->Defined) {
            Writer_ |= "#include \"" + depend->Name + Opts_.IncludesSuffix + ".yaff.h\"";
        }
    }

    if (!schema.Dependencies.empty()) {
        Writer_ |= "";
    }
}

void CppGenerator::Impl::GenerateEnum(const ir::EnumDef& enumDef) {
    GenerateEnumStruct(enumDef);
    GenerateEnumConst(enumDef);
    GenerateEnumIsValidFunc(enumDef);
    GenerateEnumNameFunc(enumDef);
}

void CppGenerator::Impl::GenerateEnumStruct(const ir::EnumDef& enumDef) {
    Writer_ |= "enum class " + enumDef.Name + " : int32_t {";
    ForValues(enumDef, [&](const auto& enumVal) {
        Writer_ >= GenerateEscapedName(enumVal.Name) + " = " + std::to_string(enumVal.Value) + ",";
    });
    Writer_ |= "};\n";
}

void CppGenerator::Impl::GenerateEnumConst(const ir::EnumDef& enumDef) {
    Writer_ |= "inline constexpr " + enumDef.Name + " " + enumDef.Name + "_MAX = static_cast<" + enumDef.Name + ">(" +
               std::to_string(enumDef.Values.back().Value) + ");";
    Writer_ |= "inline constexpr " + enumDef.Name + " " + enumDef.Name + "_MIN = static_cast<" + enumDef.Name + ">(" +
               std::to_string(enumDef.Values.front().Value) + ");";
    Writer_ |= "inline constexpr int " + enumDef.Name +
               "_ARRAYSIZE = " + std::to_string(enumDef.Values.back().Value + 1) + ";\n";
}

void CppGenerator::Impl::GenerateEnumIsValidFunc(const ir::EnumDef& enumDef) {
    Writer_ |= "inline constexpr bool " + enumDef.Name + "_IsValid(const int value) {";
    Writer_.IncrementIdentLevel();
    Writer_ |= "switch (value) {";
    ForValues(enumDef, [&](const auto& enumVal) { Writer_ |= "case " + std::to_string(enumVal.Value) + ":"; });
    Writer_ >= "return true;";
    Writer_ |= "default:";
    Writer_ >= "return false;";
    Writer_ |= "}";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateEnumNameFunc(const ir::EnumDef& enumDef) {
    Writer_ |= "inline constexpr std::string_view " + enumDef.Name + "_Name(" + enumDef.Name + " value) {";
    Writer_.IncrementIdentLevel();
    Writer_ |= "switch (value) {";
    ForValues(enumDef, [&](const auto& enumVal) {
        Writer_ |= "case " + enumDef.Name + "::" + GenerateEscapedName(enumVal.Name) + ":";
        Writer_ >= "return \"" + enumVal.Name + "\";";
    });
    Writer_ |= "default:";
    Writer_ >= "return \"\";";
    Writer_ |= "}";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";

    Writer_ |= "inline constexpr bool " + enumDef.Name + "_Parse(std::string_view name, " + enumDef.Name + "* value) {";
    Writer_.IncrementIdentLevel();
    ForValues(enumDef, [&](const auto& enumVal) {
        Writer_ |= "if (name == \"" + enumVal.Name + "\") {";
        Writer_ >= "*value = " + enumDef.Name + "::" + GenerateEscapedName(enumVal.Name) + ";";
        Writer_ >= "return true;";
        Writer_ |= "}";
    });
    Writer_ |= "return false;";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateEnumPre(const ir::EnumDef& enumDef) {
    Writer_ |= "enum class " + enumDef.Name + " : int32_t;";

    if (Opts_.GenerateReflectionApi) {
        Writer_ |= GenerateEnumDescriptorFuncDeclaration(enumDef.Name) + ";";
    }

    Writer_ |= "";
}

void CppGenerator::Impl::GenerateEnumPost(const ir::EnumDef& enumDef) {
    if (Opts_.GenerateReflectionApi) {
        GenerateEnumDescriptor(enumDef);
    }
}

void CppGenerator::Impl::GenerateMessage(const ir::MessageDef& msgDef) {
    if (ir::IsStaticMetaMessage(msgDef)) {
        GenerateMessageStaticMeta(msgDef);
    }

    const std::string baseClass = GenerateMessageBaseClass(msgDef);
    Writer_ |= "YAFF_LAYOUT_BEGIN(" + msgDef.Name + ") final : private " + baseClass + " {";

    Writer_.TrackText();
    Writer_ >=
        "using MetaType = " + (ir::IsStaticMetaMessage(msgDef) ? GenerateMessageStaticMetaName(msgDef) : "void") + ";";
    if (Opts_.GenerateProtobufApi) {
        Writer_ >= "using ProtobufType = " + GenerateMessageProtobufType(msgDef) + ";";
    }
    if (Writer_.TextWritten()) {
        Writer_ |= "";
    }

    if (!msgDef.Fields.empty()) {
        auto messageStructGuard = Writer_.IdentGuard();

        ForRealStringFields(msgDef, [&](const auto& fieldDef) {
            if (fieldDef.Type->Modifiers.contains(ir::DEFAULT_MODIFIER_NAME)) {
                Writer_ |= "inline static const auto& " + GenerateDefaultName(fieldDef) +
                           " = *::yaff::ReadLayout<::yaff::String>(" + GenerateStringDefault(*fieldDef.Type) + ");";
            }
        });

        GenerateMessageIdsEnum(msgDef);
        ForOneOfs(msgDef, [&](const auto& oneOf) {
            Writer_ |= "enum " + GenerateOneOfCaseEnumName(oneOf.Name) + " : ::yaff::FieldId {";
            ForFields(oneOf, [&](const auto* fieldDef) {
                if (fieldDef->Deprecated) {
                    return;
                }
                Writer_ >=
                    GenerateOneOfFieldName(oneOf.Name, fieldDef) + " = " + std::to_string(fieldDef->OneOfId) + ",";
            });
            Writer_ >= GenerateOneOfDefaultCaseEnumName(oneOf.Name) + " = 0,";
            Writer_ |= "};\n";
        });

        ForOneOfs(msgDef, [&](const auto& oneOf) { GenerateMessageOneOfCase(oneOf); });

        ForRealFields(msgDef, [&](const auto& fieldDef) {
            if (ir::IsExplicitField(fieldDef)) {
                GenerateMessageFieldCheck(fieldDef);
            }
            if (IsSizeableField(fieldDef)) {
                GenerateMessageFieldSize(fieldDef);
                GenerateMessageFieldGetIdx(fieldDef);
            }
            GenerateMessageFieldGet(fieldDef);
        });
    }

    if (ir::IsAssociativePair(msgDef)) {
        auto g = Writer_.IdentGuard();
        GenerateMessageFieldCompare(msgDef.Name, msgDef.Fields[0]);
    }

    Writer_.IncrementIdentLevel();
    GenerateMessageAliasDefaultFunc(msgDef);
    Writer_.DecrementIdentLevel();

    if (Opts_.GenerateReflectionApi) {
        auto g = Writer_.IdentGuard();
        GenerateMessageAliasDescriptor(msgDef);
    }

    if (Opts_.GenerateProtobufApi) {
        auto g = Writer_.IdentGuard();
        GenerateMessageAliasTransformFunc(msgDef);
        GenerateMessageAliasSerializeFunc(msgDef);
        if (ir::IsFixedMessage(msgDef)) {
            GenerateMessageAliasDeferredSerializeFunc(msgDef);
        }
    }

    Writer_ |= "};";
    Writer_ |= "YAFF_LAYOUT_END\n";

    GenerateByMessageLayout(msgDef, std::bind(&CppGenerator::Impl::GenerateMessageSerializer, this, _1, _2));

    GenerateMessageBasicSerializeFunc(msgDef);
    if (ir::IsDynamicMessage(msgDef) && !ir::IsGapMessage(msgDef)) {
        GenerateMessageDynamicSerializeFunc(msgDef);
    }
}

void CppGenerator::Impl::GenerateMessageFieldCompare(const std::string& msgName,
                                                     const ir::MessageDef::FieldDef& fieldDef) {
    const std::string getter = GenerateFieldName(fieldDef) + "()";

    // Using `auto` for the return type lets the compiler pick
    // the right ordering category based on the field type.
    Writer_ |= "friend auto operator<=>(const " + msgName + "& lhs, const " + msgName + "& rhs) {";
    Writer_ >= "return lhs." + getter + " <=> rhs." + getter + ";";
    Writer_ |= "}\n";

    Writer_ |= "friend bool operator==(const " + msgName + "& lhs, const " + msgName + "& rhs) {";
    Writer_ >= "return lhs." + getter + " == rhs." + getter + ";";
    Writer_ |= "}\n";

    Writer_ |= "template <typename K>";
    Writer_ |= "friend auto operator<=>(const " + msgName + "& lhs, const K& k) {";
    Writer_ >= "return lhs." + getter + " <=> k;";
    Writer_ |= "}\n";

    Writer_ |= "template <typename K>";
    Writer_ |= "friend bool operator==(const " + msgName + "& lhs, const K& k) {";
    Writer_ >= "return lhs." + getter + " == k;";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateByMessageLayout(const ir::MessageDef& msgDef, MessageLayoutGenerator gen) {
    switch (msgDef.Layout) {
        case MessageLayout::MESSAGE_LAYOUT_FIXED: {
            gen(MessageType::MESSAGE_TYPE_FIXED, msgDef);
            break;
        }
        case MessageLayout::MESSAGE_LAYOUT_FLAT: {
            gen(MessageType::MESSAGE_TYPE_FLAT, msgDef);
            break;
        }
        case MessageLayout::MESSAGE_LAYOUT_SPARSE: {
            gen(MessageType::MESSAGE_TYPE_SPARSE, msgDef);
            break;
        }
        case MessageLayout::MESSAGE_LAYOUT_DYNAMIC: {
            if (!ir::IsGapMessage(msgDef)) {
                gen(MessageType::MESSAGE_TYPE_FLAT, msgDef);
            }
            gen(MessageType::MESSAGE_TYPE_SPARSE, msgDef);
            break;
        }
        default:
            YAFF_THROW("unknown message layout");
    }
}

void CppGenerator::Impl::GenerateMessageIdsEnum(const ir::MessageDef& msgDef) {
    Writer_ |= "enum MessageIds : ::yaff::FieldId {";
    ForFields(msgDef, [&](const auto& fieldDef) {
        Writer_ >= GenerateIdName(fieldDef) + " = " + std::to_string(fieldDef.Id) + ",";
    });
    Writer_ |= "};\n";
}

void CppGenerator::Impl::GenerateMessageStaticMeta(const ir::MessageDef& msgDef) {
    YAFF_REQUIRE(ir::IsStaticMetaMessage(msgDef));

    Writer_ |= "struct " + GenerateMessageStaticMetaName(msgDef) + " {";
    Writer_.IncrementIdentLevel();

    if (ir::IsFixedMessage(msgDef)) {
        Writer_ |= "inline static constexpr size_t LIMIT = " + std::to_string(ir::MaxMessageSize(msgDef)) + ";";
    }
    const auto meta = GenerateMessageMeta(msgDef);
    Writer_ |= "inline static constexpr std::array<yaff::FieldOffset, " + std::to_string(meta.FlatOffsets.size()) +
               "> FLAT_OFFSETS = {\\";
    for (const auto offset : meta.FlatOffsets) {
        Writer_ >= std::to_string(offset) + ", \\";
    }
    Writer_ |= "};";
    Writer_ |= "inline static constexpr std::array<yaff::FieldId, " + std::to_string(meta.DeletedIds.size()) +
               "> DELETED_IDS = {\\";
    for (const auto id : meta.DeletedIds) {
        Writer_ >= std::to_string(id) + ", \\";
    }
    Writer_ |= "};";
    Writer_ |=
        "inline static constexpr std::array<bool, " + std::to_string(meta.StaticFlags.size()) + "> STATIC_FLAGS = {\\";
    for (const auto flag : meta.StaticFlags) {
        Writer_ >= std::to_string(flag) + ", \\";
    }
    Writer_ |= "};";

    Writer_.DecrementIdentLevel();
    Writer_ |= "};\n";
}

void CppGenerator::Impl::GenerateMessageSerializer(MessageType msgType, const ir::MessageDef& msgDef) {
    const std::string messageSuffix = GenerateMessageType(msgType);
    const std::string serializerName = GenerateMessageSerializerName(msgType, msgDef);

    Writer_ |= "struct " + serializerName + " {";
    Writer_.IncrementIdentLevel();

    Writer_ |= "::yaff::Serializer& S;\n";

    Writer_ |= "explicit " + serializerName + "(::yaff::Serializer& ys)";
    Writer_ >= ": S(ys)";
    Writer_ |= "{";
    Writer_ >= "S.Start" + messageSuffix + "Message\\";
    if (msgType == MessageType::MESSAGE_TYPE_FIXED || msgType == MessageType::MESSAGE_TYPE_FLAT) {
        Writer_ |= "<" + msgDef.Name + "::MetaType" + ">\\";
    }
    Writer_ |= "(\\";
    if (msgType == MessageType::MESSAGE_TYPE_FLAT || msgType == MessageType::MESSAGE_TYPE_SPARSE) {
        const std::string arg = (HasExplicitFields(msgDef) ? "/* implicit */ false" : "/* implicit */ true");
        Writer_ |= arg + "\\";
    }
    if (msgType == MessageType::MESSAGE_TYPE_FLAT) {
        const std::string arg = (ir::IsDynamicMessage(msgDef) ? "/* sized */ true" : "/* sized */ false");
        Writer_ |= ", " + arg + "\\";
    }
    Writer_ |= ");";
    Writer_ |= "}\n";

    ForRealFields(msgDef, [&](const auto& fieldDef) { GenerateMessageFieldAdd(msgDef.Name, fieldDef); });

    const std::string finishType = "::yaff::InternalOffset<" + msgDef.Name + ">";
    Writer_ |= finishType + " Finish() && {";
    Writer_ >= "return " + finishType + "(S.Finish" + messageSuffix + "Message());";
    Writer_ |= "}";

    Writer_.DecrementIdentLevel();
    Writer_ |= "};\n";
}

void CppGenerator::Impl::GenerateMessageBasicSerializeFunc(const ir::MessageDef& msgDef) {
    Writer_ |= "template <typename Underlying = " + GenerateMessageBasicSerializeFuncDefault(msgDef) + ">";
    Writer_ |= "inline ::yaff::InternalOffset<" + msgDef.Name + "> " + GenerateSerializeFuncName(msgDef) + "(";
    Writer_ >= "::yaff::Serializer& ys\\";
    ForRealFields(msgDef, [&](const auto& fieldDef) {
        Writer_ >= ",";
        Writer_ >= GenerateMessageSerializeFuncArgument(fieldDef) + "\\";
    });
    Writer_ |= "";
    Writer_ |= ") {";
    Writer_ >= "Underlying s(ys);";

    ForReversedRealFields(msgDef, [&](const auto& fieldDef) {
        const bool isOptional = ir::IsScalar(fieldDef.Type->Type) && ir::IsExplicitField(fieldDef);
        const std::string argName = "a_" + GenerateFieldName(fieldDef);
        const std::string argPrefix = (isOptional ? "*" : "");
        const std::string addCall = "s.add_" + GenerateFieldName(fieldDef) + "(" + argPrefix + argName + ");";
        Writer_ >= (isOptional ? "if (" + argName + ") { " + addCall + " }" : addCall);
    });

    Writer_ >= "return std::move(s).Finish();";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageDynamicSerializeFunc(const ir::MessageDef& msgDef) {
    const std::string serializeFuncName = GenerateSerializeFuncName(msgDef);
    const std::string flatSerializer = GenerateMessageSerializerName(MessageType::MESSAGE_TYPE_FLAT, msgDef);
    const std::string sparseSerializer = GenerateMessageSerializerName(MessageType::MESSAGE_TYPE_SPARSE, msgDef);
    Writer_ |= "inline ::yaff::InternalOffset<" + msgDef.Name + "> " + serializeFuncName + "(";
    Writer_ >= "::yaff::Serializer& ys\\";
    ForRealFields(msgDef, [&](const auto& fieldDef) {
        Writer_ >= ",";
        Writer_ >= GenerateMessageSerializeFuncArgument(fieldDef) + "\\";
    });
    Writer_ |= "";
    Writer_ |= ") {";
    Writer_.IncrementIdentLevel();
    Writer_ |= "switch (ys.GetForcedDynamicAlternative()) {";
    Writer_ |= "case ::yaff::MessageLayout::MESSAGE_LAYOUT_FLAT: {";
    Writer_ >= "return " + GenerateMessageBasicSerializeFuncCall(msgDef, flatSerializer) + ";";
    Writer_ |= "}";
    Writer_ |= "case ::yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE: {";
    Writer_ >= "return " + GenerateMessageBasicSerializeFuncCall(msgDef, sparseSerializer) + ";";
    Writer_ |= "}";
    Writer_ |= "default:";
    Writer_ >= "YAFF_THROW(\"unknown preferable layout in '" + serializeFuncName + "'\");";
    Writer_ |= "}";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageFieldAdd(const std::string& msgName, const ir::MessageDef::FieldDef& fieldDef) {
    const std::string valueArgType = GenerateTypeExternal(*fieldDef.Type);
    const std::string valueArgName = GenerateFieldName(fieldDef);

    Writer_ |= "void add_" + GenerateFieldName(fieldDef) + "(" + valueArgType + " " + valueArgName + ") {";

    const std::string type = GenerateTypeInternal(*fieldDef.Type);
    const std::string id = msgName + "::" + GenerateIdName(fieldDef);
    const std::string defaultSuffix =
        (ir::IsScalar(fieldDef.Type->Type) ? ", " + GenerateDefaultValueInternal(fieldDef) : "");
    Writer_ >= "S.AddField<" + type + ">(" + id + ", " + GenerateTypeCastInternal(*fieldDef.Type, valueArgName) +
                   defaultSuffix + ");";

    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageOneOfCase(const ir::MessageDef::OneOfDef& oneOf) {
    const std::string enumName = GenerateOneOfCaseEnumName(oneOf.Name);
    Writer_ |= enumName + " " + oneOf.Name + "_case() const {";
    Writer_.IncrementIdentLevel();
    ForFields(oneOf, [&](const auto* field) {
        Writer_ |= "if (ReadPresence<" + GenerateTypeInternal(*field->Type) + ">(" + GenerateIdName(*field) + ")) {";
        Writer_ >= "return " + GenerateOneOfFieldName(oneOf.Name, field) + ";";
        Writer_ |= "}";
    });
    Writer_ |= "return " + GenerateOneOfDefaultCaseEnumName(oneOf.Name) + ";";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";

    auto generateCaseGet = [&](const std::string& get) {
        Writer_ |= enumName + " " + get + "() const {";
        Writer_ >= "return " + oneOf.Name + "_case();";
        Writer_ |= "}\n";
    };
    generateCaseGet("Get" + oneOf.Name + "Case");
    if (const std::string camelName = ToCamelCase(oneOf.Name, true); camelName != oneOf.Name) {
        generateCaseGet("Get" + camelName + "Case");
    }
}

void CppGenerator::Impl::GenerateMessageFieldGet(const ir::MessageDef::FieldDef& fieldDef) {
    const bool isScalar = ir::IsScalar(fieldDef.Type->Type);
    const std::string returnType = GenerateGetterReturnType(*fieldDef.Type);
    const std::string fieldName = GenerateFieldName(fieldDef);
    const std::string prefix = (isScalar ? "" : "&");
    const std::string reader = (isScalar ? "ReadValue" : "*ReadLayout");
    const std::string internalType = GenerateTypeInternal(*fieldDef.Type);
    const std::string idName = GenerateIdName(fieldDef);
    const std::string internalDefault = GenerateDefaultValueInternal(fieldDef);
    const std::string readCall = reader + "<" + internalType + ">(" + idName + ", " + prefix + internalDefault + ")";

    Writer_ |= "YAFF_PURE " + returnType + " " + fieldName + "() const {";
    Writer_ >= "return " + GenerateTypeCastExternal(*fieldDef.Type, readCall) + ";";
    Writer_ |= "}\n";

    Writer_ |= "YAFF_PURE " + returnType + " " + "Get" + GenerateEscapedName(fieldDef.Name) + "() const {";
    Writer_ >= "return " + fieldName + "();";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageFieldGetIdx(const ir::MessageDef::FieldDef& fieldDef) {
    YAFF_REQUIRE(IsSizeableField(fieldDef));

    const std::string returnType = GenerateGetterReturnType(*fieldDef.Type->ElementType);
    const std::string fieldName = GenerateFieldName(fieldDef);
    Writer_ |= "YAFF_PURE " + returnType + " " + fieldName + "(const int i) const {";
    Writer_ >= "return " + fieldName + "()[i];";
    Writer_ |= "}\n";

    Writer_ |= "YAFF_PURE " + returnType + " Get" + fieldName + "(const size_t i) const {";
    Writer_ >= "YAFF_ASSERT(i < static_cast<size_t>(std::numeric_limits<int>::max()));";
    Writer_ >= "return " + fieldName + "(i);";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageFieldCheck(const ir::MessageDef::FieldDef& fieldDef) {
    YAFF_REQUIRE(ir::IsExplicitField(fieldDef));

    const std::string fieldName = GenerateFieldName(fieldDef);
    const std::string internalType = GenerateTypeInternal(*fieldDef.Type);
    const std::string idName = GenerateIdName(fieldDef);
    const std::string checkCall = "ReadPresence<" + internalType + ">(" + idName + ")";
    Writer_ |= "YAFF_PURE bool has_" + fieldName + "() const {";
    Writer_ >= "return " + checkCall + ";";
    Writer_ |= "}\n";

    Writer_ |= "YAFF_PURE bool Has" + GenerateEscapedName(fieldDef.Name) + "() const {";
    Writer_ >= "return has_" + fieldName + "();";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageFieldSize(const ir::MessageDef::FieldDef& fieldDef) {
    YAFF_REQUIRE(IsSizeableField(fieldDef));

    Writer_ |= "YAFF_PURE int " + GenerateFieldName(fieldDef) + "_size() const {";
    Writer_ >= "return " + GenerateFieldName(fieldDef) + "().size();";
    Writer_ |= "}\n";

    Writer_ |= "YAFF_PURE size_t " + GenerateEscapedName(fieldDef.Name) + "Size() const {";
    Writer_ >= "return static_cast<size_t>(" + GenerateFieldName(fieldDef) + "_size());";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessagePre(const ir::MessageDef& msgDef) {
    Writer_ |= "struct " + msgDef.Name + ";";

    if (Opts_.GenerateProtobufApi) {
        Writer_ |= GenerateParseToProtobufFuncDeclaration(msgDef) + ";";
        Writer_ |= GenerateSerializeProtobufFuncDeclaration(msgDef) + ";";
        if (ir::IsFixedMessage(msgDef)) {
            Writer_ |= GenerateDeferredSerializeProtobufFuncDeclaration(msgDef) + ";";
        }
    }

    if (Opts_.GenerateReflectionApi) {
        Writer_ |= GenerateMessageDescriptorFuncDeclaration(msgDef.Name) + ";";
    }

    Writer_ |= GenerateDefaultFuncDeclaration(msgDef.Name, msgDef.Name) + ";";
    Writer_ |= "";

    // Forward declarations for external experimental generation;
    const std::string col = GenerateMessageColumnName(msgDef);
    Writer_ |= "template <template <typename> typename S> struct " + col + ";";
    Writer_ |= GenerateDefaultFuncDeclaration(col + "<::yaff::exp::Sizeable>", col) + ";";
    Writer_ |= "";
}

void CppGenerator::Impl::GenerateMessagePost(const ir::MessageDef& msgDef) {
    GenerateMessageDefaultFunc(msgDef);

    if (Opts_.GenerateProtobufApi) {
        GenerateMessageParseToProtobuf(msgDef);
        GenerateMessageProtobufSerializer(msgDef, /* deferred */ false);
        if (ir::IsFixedMessage(msgDef)) {
            GenerateMessageProtobufSerializer(msgDef, /* deferred */ true);
        }
    }

    if (Opts_.GenerateReflectionApi) {
        GenerateMessageDescriptor(msgDef);
    }
}

void CppGenerator::Impl::GenerateEnumDescriptor(const ir::EnumDef& enumDef) {
    Writer_ |= GenerateEnumDescriptorFuncDeclaration(enumDef.Name) + " {";
    Writer_.IncrementIdentLevel();

    const bool hasValues = !enumDef.Values.empty();
    if (hasValues) {
        GenerateEnumValueDescriptors(enumDef);
    }

    Writer_ |= "static constexpr ::yaff::reflect::EnumDescriptor enm = {";
    Writer_ >= ".Name = \"" + enumDef.Name + "\",";
    Writer_ >= ".ValueCount = " + std::to_string(enumDef.Values.size()) + ",";
    if (hasValues) {
        Writer_ >= ".Values = values,";
    }
    Writer_ |= "};";
    Writer_ |= "return &enm;";

    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateEnumValueDescriptors(const ir::EnumDef& enumDef) {
    Writer_ |= "static constexpr ::yaff::reflect::EnumValueDescriptor values[] = {";
    ForValues(enumDef, [&](const auto& enumVal) {
        auto g = Writer_.IdentGuard();
        Writer_ |= "{";
        Writer_ >= ".Name = \"" + enumVal.Name + "\",";
        Writer_ >= ".Value = " + std::to_string(enumVal.Value) + ",";
        Writer_ |= "},";
    });
    Writer_ |= "};\n";
}

void CppGenerator::Impl::GenerateMessageAliasDescriptor(const ir::MessageDef& msgDef) {
    Writer_ |= "static const ::yaff::reflect::MessageDescriptor* Descriptor() {";
    Writer_ >= "return " + GenerateDescriptorFuncName(msgDef.Name) + "();";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageDescriptor(const ir::MessageDef& msgDef) {
    Writer_ |= GenerateMessageDescriptorFuncDeclaration(msgDef.Name) + " {";
    Writer_.IncrementIdentLevel();

    const bool hasFields = !msgDef.Fields.empty();
    if (hasFields) {
        GenerateMessageFieldDescriptors(msgDef);
    }

    Writer_ |= "static constexpr ::yaff::reflect::MessageDescriptor message = {";
    Writer_ >= ".Name = \"" + msgDef.Name + "\",";
    Writer_ >= ".Layout = " + GenerateMessageLayoutDescriptor(msgDef.Layout) + ",";
    Writer_ >= ".FieldCount = " + std::to_string(msgDef.Fields.size()) + ",";
    if (hasFields != 0) {
        Writer_ >= ".Fields = fields,";
    }
    Writer_ |= "};";
    Writer_ |= "return &message;";

    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageFieldDescriptors(const ir::MessageDef& msgDef) {
    TypeIndex index;
    ForFields(msgDef, [&](const auto& fieldDef) {
        const auto* type = fieldDef.Type;
        index.Add(type);
        if (type->Type == Type::TYPE_ARRAY && type->ElementType) {
            index.Add(type->ElementType);
        }
    });

    Writer_ |= "static const ::yaff::reflect::TypeDescriptor types[] = {";
    index.ForEach([&](const auto& type) {
        auto g = Writer_.IdentGuard();
        Writer_ |= "{";
        Writer_ >= ".Type = " + GenerateBasicTypeDescriptor(type.Type) + ",";
        Writer_ >= ".Descriptor = " + GenerateCompositeTypeDescriptor(type, index) + ",";
        Writer_ >= ".Default = " + GenerateDefaultDescriptor(type) + ",";
        if (ir::IsAssociative(type)) {
            Writer_ >= ".Associative = true,";
        }
        if (ir::IsInline(type)) {
            Writer_ >= ".Inline = true,";
        }
        Writer_ |= "},";
    });
    Writer_ |= "};";

    Writer_ |= "static constexpr ::yaff::reflect::FieldDescriptor fields[] = {";
    ForFields(msgDef, [&](const auto& fieldDef) {
        auto g = Writer_.IdentGuard();
        Writer_ |= "{";
        Writer_ >= ".Id = " + std::to_string(fieldDef.Id) + ",";
        if (!fieldDef.Deprecated) {
            Writer_ >= ".Name = \"" + fieldDef.Name + "\",";
        }
        if (!fieldDef.OneOf.empty()) {
            Writer_ >= ".ContainingOneof = \"" + fieldDef.OneOf + "\",";
        }
        Writer_ >= ".Type = &types[" + std::to_string(index.At(fieldDef.Type)) + "],";
        Writer_ >= ".Presence = " + GeneratePresenceDescriptor(fieldDef.Presence) + ",";
        if (fieldDef.Deprecated) {
            Writer_ >= ".Deprecated = true,";
        }
        Writer_ >= ".FlatOffset = " + std::to_string(fieldDef.FlatOffset) + ",";
        Writer_ |= "},";
    });
    Writer_ |= "};";
}

void CppGenerator::Impl::GenerateMessageDefaultFunc(const ir::MessageDef& msgDef) {
    Writer_ |= GenerateDefaultFuncDeclaration(msgDef.Name, msgDef.Name) + " {";
    Writer_ >= "return " + msgDef.Name + "::Default();";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageAliasDefaultFunc(const ir::MessageDef& msgDef) {
    Writer_ |= "static const " + msgDef.Name + "& Default() {";
    Writer_ >= "return *::yaff::ReadLayout<" + msgDef.Name + ">(DEFAULT_MESSAGE);";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageAliasSerializeFunc(const ir::MessageDef& msgDef) {
    Writer_ |= "template <typename ...Ps>";
    Writer_ |= "static ::yaff::InternalOffset<" + msgDef.Name + "> Serialize(::yaff::Serializer& ys, Ps&&... ps) {";
    Writer_ >= "return " + GenerateSerializeFuncName(msgDef) + "(ys, std::forward<Ps>(ps)...);";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageAliasDeferredSerializeFunc(const ir::MessageDef& msgDef) {
    Writer_ |= "template <typename ...Ps>";
    Writer_ |= "static auto DeferSerialize(::yaff::Serializer& ys, Ps&&... ps) {";
    Writer_ >= "return " + GenerateDeferredSerializeFuncName(msgDef) + "(ys, std::forward<Ps>(ps)...);";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageAliasTransformFunc(const ir::MessageDef& msgDef) {
    Writer_ |= "template <typename T>";
    Writer_ |= "void ParseTo(T& to) const {";
    Writer_ >= GenerateWithNamespaceName(msgDef.Schema->Namespace, GenerateTransformFuncName(msgDef)) + "(*this, to);";
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageParseToProtobuf(const ir::MessageDef& msgDef) {
    Writer_ |= GenerateParseToProtobufFuncDeclaration(msgDef) + " {";
    Writer_.IncrementIdentLevel();

    if (ir::IsAssociativePair(msgDef)) {
        Writer_ |= GenerateProtobufPairFill(msgDef.Fields[0]);
        Writer_ |= GenerateProtobufPairFill(msgDef.Fields[1]);
    } else {
        // Read and transform scalars first to increase read locality.
        ForRealScalarFields(msgDef, [&](const auto& fieldDef) {
            const std::string n = GenerateFieldName(fieldDef);
            Writer_ |= GenerateProtobufScalarFill("proto", "proto.clear_" + n + "()", fieldDef);
        });
        ForRealStructureFields(msgDef, [&](const auto& fieldDef) {
            const std::string n = GenerateFieldName(fieldDef);
            Writer_ |= GenerateProtobufStructFill("proto.mutable_" + n + "()", "proto.clear_" + n + "()", fieldDef);
        });
    }

    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void CppGenerator::Impl::GenerateMessageProtobufSerializer(const ir::MessageDef& msgDef, const bool deferred) {
    const std::string declaration = (deferred ? GenerateDeferredSerializeProtobufFuncDeclaration(msgDef)
                                              : GenerateSerializeProtobufFuncDeclaration(msgDef));
    Writer_ |= declaration + " {";
    Writer_.IncrementIdentLevel();

    ForRealFields(msgDef, [&](const auto& fieldDef) {
        const std::string protobufValue =
            (ir::IsAssociativePair(msgDef) ? GenerateProtobufPairValue(fieldDef) : GenerateProtobufValue(fieldDef));
        Writer_ |= "const auto a_" + GenerateFieldName(fieldDef) + " = " + protobufValue + ";";
    });

    const std::string serializeCall = (deferred ? GenerateMessageBasicSerializeFuncDeferredCall(msgDef)
                                                : GenerateMessageBasicSerializeFuncCall(msgDef));
    Writer_ |= "return " + serializeCall + ";";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

std::string CppGenerator::Impl::GenerateProtobufStructFill(const std::string& mutableCall, const std::string& clearCall,
                                                           const ir::MessageDef::FieldDef& fieldDef) {
    switch (fieldDef.Type->Type) {
        case Type::TYPE_STRING: {
            const std::string fieldName = GenerateFieldName(fieldDef);
            if (ir::IsExplicitField(fieldDef)) {
                return "if (from.has_" + fieldName + "()) { const auto& _v = from." + fieldName + "(); " + mutableCall +
                       "->assign(_v.AsStringView()); } else { " + clearCall + "; }";
            }
            return "if (const auto& _v = from." + fieldName + "(); _v != " + GenerateDefaultValueExternal(fieldDef) +
                   ") { " + mutableCall + "->assign(_v.AsStringView()); } else { " + clearCall + "; }";
        }
        case Type::TYPE_ARRAY: {
            std::string fillArray =
                "if (const auto& _v = from." + GenerateFieldName(fieldDef) + "(); _v.Size() != 0) { ";
            if (fieldDef.Type->ElementType->Type == Type::TYPE_MESSAGE) {
                // Array element can not be empty, so there is no need for additional check;
                const auto* elemMessage = fieldDef.Type->ElementType->MessageDef;
                if (ir::IsAssociative(*fieldDef.Type)) {
                    fillArray += "for (uint32_t i = 0; i < _v.Size(); ++i) { " +
                                 GenerateMessageProtobufPair(*elemMessage) + " _p; _v.Get(i).ParseTo(_p); " +
                                 mutableCall + "->emplace(std::move(_p)); }";
                } else if (ir::IsSequentialMessage(*fieldDef.Type)) {
                    fillArray += "::yaff::exp::ColumnarParseTo(_v, *" + mutableCall + ");";
                } else {
                    fillArray += mutableCall + "->Reserve(_v.Size()); ";
                    fillArray +=
                        "for (uint32_t i = 0; i < _v.Size(); ++i) { _v.Get(i).ParseTo(*" + mutableCall + "->Add()); }";
                }
            } else if (fieldDef.Type->ElementType->Type == Type::TYPE_STRING) {
                fillArray += mutableCall + "->Reserve(_v.Size()); ";
                fillArray += "for (uint32_t i = 0; i < _v.Size(); ++i) { const auto& _e = _v.Get(i); " + mutableCall +
                             "->Add()->assign(_e.AsStringView()); }";
            } else {
                const std::string setValue =
                    (fieldDef.Type->ElementType->Type == Type::TYPE_ENUM
                         ? "static_cast<" + GenerateEnumProtobufName(*fieldDef.Type->ElementType->EnumDef) +
                               ">(_v.Get(i))"
                         : "_v.Get(i)");
                fillArray += mutableCall + "->Reserve(_v.Size()); ";
                fillArray += "for (uint32_t i = 0; i < _v.Size(); ++i) { " + mutableCall + "->Add(" + setValue + "); }";
            }
            fillArray += " } else { " + clearCall + "; }";
            return fillArray;
        }
        case Type::TYPE_MESSAGE: {
            const std::string fieldName = GenerateFieldName(fieldDef);
            return "if (from.has_" + fieldName + "()) { from." + fieldName + "().ParseTo(*" + mutableCall +
                   "); } else { " + clearCall + "; }";
        }
        default:
            YAFF_THROW("non struct type");
    }
}

std::string CppGenerator::Impl::GenerateProtobufScalarFill(const std::string& mutableCall, const std::string& clearCall,
                                                           const ir::MessageDef::FieldDef& fieldDef) {
    YAFF_REQUIRE(ir::IsScalar(fieldDef.Type->Type));
    const std::string fieldName = GenerateFieldName(fieldDef);
    const std::string checkCall =
        (ir::IsExplicitField(fieldDef)
             ? "from.has_" + fieldName + "()"
             : "const auto _v = from." + fieldName + "(); !::yaff::IsEqual<" + GenerateTypeExternal(*fieldDef.Type) +
                   ">(_v, " + GenerateDefaultValueExternal(fieldDef) + ")");
    const std::string getCall = (ir::IsExplicitField(fieldDef) ? "from." + fieldName + "()" : "_v");
    const std::string setValue =
        (fieldDef.Type->Type == Type::TYPE_ENUM
             ? "static_cast<" + GenerateEnumProtobufName(*fieldDef.Type->EnumDef) + ">(" + getCall + ")"
             : getCall);
    return "if (" + checkCall + ") { " + mutableCall + ".set_" + fieldName + "(" + setValue + "); } else { " +
           clearCall + "; }";
}

std::string CppGenerator::Impl::GenerateProtobufPairFill(const ir::MessageDef::FieldDef& fieldDef) {
    const std::string getCall = "from." + GenerateFieldName(fieldDef) + "()";
    const std::string pairName = "proto." + GenerateExternalPairName(fieldDef);
    const std::string mutableCall =
        (fieldDef.Id == 1 ? "const_cast<" + GenerateTypeProtobuf(*fieldDef.Type) + "&>(" + pairName + ")" : pairName);
    if (ir::IsScalar(fieldDef.Type->Type)) {
        const std::string setValue =
            (fieldDef.Type->Type == Type::TYPE_ENUM
                 ? "static_cast<" + GenerateEnumProtobufName(*fieldDef.Type->EnumDef) + ">(" + getCall + ")"
                 : getCall);
        return mutableCall + " = " + setValue + ";";
    }
    const std::string clearFunc = (fieldDef.Type->Type == Type::TYPE_STRING ? "clear" : "Clear");
    return GenerateProtobufStructFill("(&" + mutableCall + ")", "(&" + mutableCall + ")->" + clearFunc + "()",
                                      fieldDef);
}

std::string CppGenerator::Impl::GenerateProtobufValue(const ir::MessageDef::FieldDef& fieldDef) {
    const std::string fieldName = GenerateFieldName(fieldDef);
    const std::string getCall = "proto." + fieldName + "()";
    const std::string serializeCall = GenerateExternalSerialize(getCall, "", *fieldDef.Type);
    if (fieldDef.Type->Type == Type::TYPE_STRING) {
        const bool isExpl = ir::IsExplicitField(fieldDef);
        const std::string checkCall =
            (isExpl ? "proto.has_" + fieldName + "()" : getCall + " != " + GenerateDefaultValueExternal(fieldDef));
        return "(" + checkCall + " ? " + serializeCall + " : 0)";
    }
    if (fieldDef.Type->Type == Type::TYPE_ARRAY) {
        return "(!" + getCall + ".empty() ? " + serializeCall + " : 0)";
    }
    if (fieldDef.Type->Type == Type::TYPE_MESSAGE) {
        return "(proto.has_" + fieldName + "() ? " + serializeCall + " : 0)";
    }
    if (ir::IsExplicitField(fieldDef)) {
        const std::string extType = GenerateTypeExternal(*fieldDef.Type);
        return "(proto.has_" + fieldName + "() ? std::optional<" + extType + ">{" + serializeCall + "} : std::nullopt)";
    }
    return serializeCall;
}

std::string CppGenerator::Impl::GenerateProtobufPairValue(const ir::MessageDef::FieldDef& fieldDef) {
    return GenerateExternalSerialize("proto." + GenerateExternalPairName(fieldDef), "", *fieldDef.Type);
}

std::string CppGenerator::Impl::GenerateExternalSerialize(const std::string& getCall, const std::string& getPrefix,
                                                          const ir::TypeDef& type) {
    switch (type.Type) {
        case Type::TYPE_ENUM:
            return GenerateTypeCastExternal(type, getCall);
        case Type::TYPE_STRING:
            return "ys.SerializeString(" + getCall + ")";
        case Type::TYPE_ARRAY: {
            const std::string elemType = GenerateTypeExternal(*type.ElementType);
            if (ir::IsAssociative(type)) {
                const std::string getElemCall = "*" + getCall + ".find(_k)";
                const std::string serializePairCall = GenerateExternalSerialize(getElemCall, "", *type.ElementType);
                return "ys.SerializeArray<" + elemType + ">(::yaff::SortedKeys(" + getCall +
                       "), [&] (const auto& _k) { return " + serializePairCall + "; })";
            }
            if (ir::IsSequentialMessage(type)) {
                const std::string ns = type.ElementType->MessageDef->Schema->Namespace;
                const std::string col = GenerateMessageColumnName(*type.ElementType->MessageDef);
                return "::yaff::exp::ColumnarSerialize<" +
                       GenerateWithNamespaceName(ns, col + "<::yaff::exp::Sizeable>") + ">(ys, " + getCall + ")";
            }
            if (!ir::IsScalar(type.ElementType->Type)) {
                // checkCall is empty because array element can not be uninitialized;
                const std::string getElemCall = getCall + "[i]";
                const std::string serialCall = GenerateExternalSerialize(getElemCall, getPrefix, *type.ElementType);
                return "ys.SerializeArray<" + elemType + ">(" + getCall + ".size(), [&] (size_t i) { return " +
                       serialCall + "; })";
            }
            // Always reinterpret_cast for easy support of enum scalars;
            return "ys.SerializeArray<" + elemType + ">(reinterpret_cast<const " + elemType + "*>(" + getCall +
                   ".data()), " + getCall + ".size())";
        }
        case Type::TYPE_MESSAGE: {
            const std::string ns = type.MessageDef->Schema->Namespace;
            const std::string serialFunc = (ir::IsInline(type) ? GenerateDeferredSerializeFuncName(*type.MessageDef)
                                                               : GenerateSerializeFuncName(*type.MessageDef));
            return GenerateWithNamespaceName(ns, serialFunc) + "(ys, " + getPrefix + getCall + ")";
        }
        default:
            return getCall;
    }
}

std::string CppGenerator::Impl::GenerateExternalPairName(const ir::MessageDef::FieldDef& fieldDef) {
    switch (fieldDef.Id) {
        case 1:
            return "first";
        case 2:
            return "second";
        default:
            YAFF_THROW("unknown pair number");
    }
}

std::string CppGenerator::Impl::GenerateParseToProtobufFuncDeclaration(const ir::MessageDef& msgDef) {
    const bool nonEmpty = HasRealFields(msgDef);
    const std::string fromArgName = (nonEmpty ? " from" : "");
    const std::string protoArgName = (nonEmpty ? " proto" : "");
    return "template <typename F> inline void " + GenerateTransformFuncName(msgDef) + "(const F&" + fromArgName + ", " +
           GenerateMessageProtobufType(msgDef) + "&" + protoArgName + ")";
}

std::string CppGenerator::Impl::GenerateDeferredSerializeProtobufFuncDeclaration(const ir::MessageDef& msgDef) {
    const std::string protoArgName = (HasRealFields(msgDef) ? " proto" : "");
    return "inline auto " + GenerateDeferredSerializeFuncName(msgDef) + "(::yaff::Serializer& ys, const " +
           GenerateMessageProtobufType(msgDef) + "&" + protoArgName + ")";
}

std::string CppGenerator::Impl::GenerateSerializeProtobufFuncDeclaration(const ir::MessageDef& msgDef) {
    const std::string protoArgName = (HasRealFields(msgDef) ? " proto" : "");
    return "inline ::yaff::InternalOffset<" + msgDef.Name + "> " + GenerateSerializeFuncName(msgDef) +
           "(::yaff::Serializer& ys, const " + GenerateMessageProtobufType(msgDef) + "&" + protoArgName + ")";
}

std::string CppGenerator::Impl::GenerateDeferredSerializeFuncName(const ir::MessageDef& msgDef) {
    return "Defer" + GenerateSerializeFuncName(msgDef);
}

std::string CppGenerator::Impl::GenerateSerializeFuncName(const ir::MessageDef& msgDef) {
    return "Serialize" + msgDef.Name;
}

std::string CppGenerator::Impl::GenerateTransformFuncName(const ir::MessageDef& msgDef) {
    return "Transform" + msgDef.Name;
}

std::string CppGenerator::Impl::GenerateEnumDescriptorFuncDeclaration(const std::string& name) {
    return "inline const ::yaff::reflect::EnumDescriptor* " + GenerateDescriptorFuncName(name) + "()";
}

std::string CppGenerator::Impl::GenerateMessageDescriptorFuncDeclaration(const std::string& name) {
    return "inline const ::yaff::reflect::MessageDescriptor* " + GenerateDescriptorFuncName(name) + "()";
}

std::string CppGenerator::Impl::GenerateDescriptorFuncName(const std::string& name) {
    return name + "Descriptor";
}

std::string CppGenerator::Impl::GenerateDefaultDescriptor(const ir::TypeDef& type) {
    const std::string* defaultPtr = FindKey(type.Modifiers, ir::DEFAULT_MODIFIER_NAME);
    if (defaultPtr == nullptr) {
        return "{}";
    }
    switch (type.Type) {
        case Type::TYPE_BOOL:
            return "{.Bool = " + *defaultPtr + "}";
        case Type::TYPE_INT32:
            return "{.Int32 = " + *defaultPtr + "}";
        case Type::TYPE_UINT32:
            return "{.Uint32 = " + *defaultPtr + "}";
        case Type::TYPE_INT64:
            return "{.Int64 = " + *defaultPtr + "}";
        case Type::TYPE_UINT64:
            return "{.Uint64 = " + *defaultPtr + "}";
        case Type::TYPE_FLOAT:
            return "{.Float = " + *defaultPtr + "}";
        case Type::TYPE_DOUBLE:
            return "{.Double = " + *defaultPtr + "}";
        case Type::TYPE_STRING:
            return "{.String = \"" + *defaultPtr + "\"}";
        case Type::TYPE_ENUM: {
            const std::string value = GenerateTypeCastInternal(type, GenerateTypeExternal(type) + "::" + *defaultPtr);
            return "{.Int32 = " + value + "}";
        }
        default:
            YAFF_THROW("unknown default value type");
    }
}

std::string CppGenerator::Impl::GenerateMessageLayoutDescriptor(const MessageLayout layout) {
    switch (layout) {
        case MessageLayout::MESSAGE_LAYOUT_UNKNOWN:
            return "::yaff::MessageLayout::MESSAGE_LAYOUT_UNKNOWN";
        case MessageLayout::MESSAGE_LAYOUT_FIXED:
            return "::yaff::MessageLayout::MESSAGE_LAYOUT_FIXED";
        case MessageLayout::MESSAGE_LAYOUT_FLAT:
            return "::yaff::MessageLayout::MESSAGE_LAYOUT_FLAT";
        case MessageLayout::MESSAGE_LAYOUT_SPARSE:
            return "::yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE";
        case MessageLayout::MESSAGE_LAYOUT_DYNAMIC:
            return "::yaff::MessageLayout::MESSAGE_LAYOUT_DYNAMIC";
        default:
            YAFF_THROW("unknown message layout");
    }
}

std::string CppGenerator::Impl::GeneratePresenceDescriptor(const Presence presence) {
    switch (presence) {
        case Presence::PRESENCE_NONE:
            return "::yaff::Presence::PRESENCE_NONE";
        case Presence::PRESENCE_IMPLICIT:
            return "::yaff::Presence::PRESENCE_IMPLICIT";
        case Presence::PRESENCE_EXPLICIT:
            return "::yaff::Presence::PRESENCE_EXPLICIT";
        default:
            YAFF_THROW("unknown message layout");
    }
}

std::string CppGenerator::Impl::GenerateBasicTypeDescriptor(const Type type) {
    switch (type) {
        case Type::TYPE_NONE:
            return "::yaff::Type::TYPE_NONE";
        case Type::TYPE_BOOL:
            return "::yaff::Type::TYPE_BOOL";
        case Type::TYPE_INT32:
            return "::yaff::Type::TYPE_INT32";
        case Type::TYPE_UINT32:
            return "::yaff::Type::TYPE_UINT32";
        case Type::TYPE_INT64:
            return "::yaff::Type::TYPE_INT64";
        case Type::TYPE_UINT64:
            return "::yaff::Type::TYPE_UINT64";
        case Type::TYPE_FLOAT:
            return "::yaff::Type::TYPE_FLOAT";
        case Type::TYPE_DOUBLE:
            return "::yaff::Type::TYPE_DOUBLE";
        case Type::TYPE_ENUM:
            return "::yaff::Type::TYPE_ENUM";
        case Type::TYPE_STRING:
            return "::yaff::Type::TYPE_STRING";
        case Type::TYPE_ARRAY:
            return "::yaff::Type::TYPE_ARRAY";
        case Type::TYPE_MESSAGE:
            return "::yaff::Type::TYPE_MESSAGE";
        default:
            YAFF_THROW("unknown type");
    }
}

std::string CppGenerator::Impl::GenerateCompositeTypeDescriptor(const ir::TypeDef& type, const TypeIndex& index) {
    switch (type.Type) {
        case Type::TYPE_ENUM: {
            if (!type.EnumDef) {
                return "{.Enum = nullptr}";
            }
            const std::string name = GenerateWithNamespaceName(type.EnumDef->Schema->Namespace, type.EnumDef->Name);
            return "{.Enum = " + GenerateDescriptorFuncName(name) + "()}";
        }
        case Type::TYPE_ARRAY: {
            if (!type.ElementType) {
                return "{.Element = nullptr}";
            }
            return "{.Element = &types[" + std::to_string(index.At(type.ElementType)) + "]}";
        }
        case Type::TYPE_MESSAGE: {
            if (!type.MessageDef) {
                return "{.Message = nullptr}";
            }
            const std::string name =
                GenerateWithNamespaceName(type.MessageDef->Schema->Namespace, type.MessageDef->Name);
            return "{.Message = " + GenerateDescriptorFuncName(name) + "()}";
        }
        default:
            return "{}";
    }
}

std::string CppGenerator::Impl::GenerateDefaultFuncDeclaration(const std::string& ret, const std::string& name) {
    return "inline const " + ret + "& " + GenerateDefaultFuncName(name) + "()";
}

std::string CppGenerator::Impl::GenerateDefaultFuncName(const std::string& name) {
    return name + "Default";
}

std::string CppGenerator::Impl::GenerateTypeCastExternal(const ir::TypeDef& type, const std::string& call) {
    switch (type.Type) {
        case Type::TYPE_ENUM:
            return "static_cast<" + GenerateTypeExternal(type) + ">(" + call + ")";
        default:
            return call;
    }
}

std::string CppGenerator::Impl::GenerateTypeCastInternal(const ir::TypeDef& type, const std::string& call) {
    switch (type.Type) {
        case Type::TYPE_ENUM:
            return "static_cast<" + GenerateTypeInternal(type) + ">(" + call + ")";
        default:
            return call;
    }
}

std::string CppGenerator::Impl::GenerateDefaultValueExternal(const ir::MessageDef::FieldDef& fieldDef) {
    switch (fieldDef.Type->Type) {
        case Type::TYPE_ENUM: {
            const std::string externalType = GenerateTypeExternal(*fieldDef.Type);
            const std::string* defaultPtr = FindKey(fieldDef.Type->Modifiers, ir::DEFAULT_MODIFIER_NAME);
            return (defaultPtr ? (externalType + "::" + *defaultPtr) : GenerateTypeCastExternal(*fieldDef.Type, "0"));
        }
        case Type::TYPE_STRING: {
            const std::string* defaultPtr = FindKey(fieldDef.Type->Modifiers, ir::DEFAULT_MODIFIER_NAME);
            return (defaultPtr ? "\"" + *defaultPtr + "\"" : "\"\"");
        }
        default: {
            return GenerateDefaultValueInternal(fieldDef);
        }
    }
}

std::string CppGenerator::Impl::GenerateDefaultValueInternal(const ir::MessageDef::FieldDef& fieldDef) {
    switch (fieldDef.Type->Type) {
        case Type::TYPE_ENUM: {
            const std::string externalType = GenerateTypeExternal(*fieldDef.Type);
            const std::string* defaultPtr = FindKey(fieldDef.Type->Modifiers, ir::DEFAULT_MODIFIER_NAME);
            const std::string defaultVal = defaultPtr ? (externalType + "::" + *defaultPtr) : "0";
            return GenerateTypeCastInternal(*fieldDef.Type, defaultVal);
        }
        case Type::TYPE_STRING: {
            if (fieldDef.Type->Modifiers.contains(ir::DEFAULT_MODIFIER_NAME)) {
                return GenerateDefaultName(fieldDef);
            }
            return GenerateTypeInternal(*fieldDef.Type) + "::Default()";
        }
        case Type::TYPE_ARRAY:
            return (ir::IsSequentialMessage(*fieldDef.Type)
                        ? GenerateWithNamespaceName(fieldDef.Type->ElementType->MessageDef->Schema->Namespace,
                                                    GenerateDefaultFuncName(GenerateMessageColumnName(
                                                        *fieldDef.Type->ElementType->MessageDef))) +
                              "()"
                        : GenerateTypeInternal(*fieldDef.Type) + "::Default()");
        case Type::TYPE_MESSAGE:
            return GenerateWithNamespaceName(fieldDef.Type->MessageDef->Schema->Namespace,
                                             GenerateDefaultFuncName(fieldDef.Type->MessageDef->Name)) +
                   "()";
        default: {
            const std::string* defaultPtr = FindKey(fieldDef.Type->Modifiers, ir::DEFAULT_MODIFIER_NAME);
            return (defaultPtr ? *defaultPtr : GenerateBasicDefault(fieldDef.Type->Type));
        }
    }
}

std::string CppGenerator::Impl::GenerateBasicDefault(const Type type) {
    switch (type) {
        case Type::TYPE_BOOL:
            return "false";
        case Type::TYPE_INT32:
        case Type::TYPE_UINT32:
        case Type::TYPE_INT64:
        case Type::TYPE_UINT64:
            return "0";
        case Type::TYPE_FLOAT:
        case Type::TYPE_DOUBLE:
            return "0.";
        default:
            YAFF_THROW("non-basic type");
    }
}

std::string CppGenerator::Impl::GenerateTypeProtobuf(const ir::TypeDef& type) {
    switch (type.Type) {
        case Type::TYPE_ENUM:
            return GenerateEnumProtobufName(*type.EnumDef);
        case Type::TYPE_STRING:
            return "std::string";
        case Type::TYPE_ARRAY:
            return (ir::IsAssociative(type)
                        ? GenerateMessageProtobufMap(*type.ElementType->MessageDef)
                        : "::google::protobuf::RepeatedPtrField<" + GenerateTypeProtobuf(*type.ElementType) + ">");
        case Type::TYPE_MESSAGE:
            return GenerateMessageProtobufType(*type.MessageDef);
        default:
            return GenerateTypeExternal(type);
    }
}

std::string CppGenerator::Impl::GenerateTypeExternal(const ir::TypeDef& type) {
    switch (type.Type) {
        case Type::TYPE_ENUM:
            return GenerateWithNamespaceName(type.EnumDef->Schema->Namespace, type.EnumDef->Name);
        case Type::TYPE_STRING:
        case Type::TYPE_ARRAY:
        case Type::TYPE_MESSAGE: {
            const std::string offsetType = (ir::IsInline(type) ? "::yaff::InlineOffset" : "::yaff::InternalOffset");
            return offsetType + "<" + GenerateTypeInternal(type) + ">";
        }
        default:
            return GenerateBasicType(type.Type);
    }
}

std::string CppGenerator::Impl::GenerateTypeInternal(const ir::TypeDef& type) {
    switch (type.Type) {
        case Type::TYPE_ENUM:
            return "int32_t";
        case Type::TYPE_STRING:
            return "::yaff::String";
        case Type::TYPE_ARRAY: {
            if (ir::IsSequentialMessage(type)) {
                const std::string ns = type.ElementType->MessageDef->Schema->Namespace;
                const std::string col = GenerateMessageColumnName(*type.ElementType->MessageDef);
                return GenerateWithNamespaceName(ns, col + "<::yaff::exp::Sizeable>");
            }
            return "::yaff::Array<" + GenerateTypeExternal(*type.ElementType) + ">";
        }
        case Type::TYPE_MESSAGE:
            return GenerateWithNamespaceName(type.MessageDef->Schema->Namespace, type.MessageDef->Name);
        default:
            return GenerateBasicType(type.Type);
    }
}

std::string CppGenerator::Impl::GenerateBasicType(const Type type) {
    switch (type) {
        case Type::TYPE_BOOL:
            return "bool";
        case Type::TYPE_INT32:
            return "int32_t";
        case Type::TYPE_UINT32:
            return "uint32_t";
        case Type::TYPE_INT64:
            return "int64_t";
        case Type::TYPE_UINT64:
            return "uint64_t";
        case Type::TYPE_FLOAT:
            return "float";
        case Type::TYPE_DOUBLE:
            return "double";
        default:
            YAFF_THROW("non-basic type");
    }
}

std::string CppGenerator::Impl::GenerateMessageType(const MessageType type) {
    switch (type) {
        case MessageType::MESSAGE_TYPE_FIXED:
            return "Fixed";
        case MessageType::MESSAGE_TYPE_FLAT:
            return "Flat";
        case MessageType::MESSAGE_TYPE_SPARSE:
            return "Sparse";
    }
    YAFF_THROW("unknown message type");
}

std::string CppGenerator::Impl::GenerateMessageBaseClass(const ir::MessageDef& msgDef) {
    switch (msgDef.Layout) {
        case MessageLayout::MESSAGE_LAYOUT_FIXED:
            return "::yaff::FixedMessage<" + GenerateMessageStaticMetaName(msgDef) + ">";
        case MessageLayout::MESSAGE_LAYOUT_FLAT:
            return "::yaff::FlatMessage<" + GenerateMessageStaticMetaName(msgDef) + ">";
        case MessageLayout::MESSAGE_LAYOUT_SPARSE:
            return "::yaff::SparseMessage";
        case MessageLayout::MESSAGE_LAYOUT_DYNAMIC:
            return "::yaff::DynamicMessage<" + GenerateMessageStaticMetaName(msgDef) + ">";
        default:
            YAFF_THROW("unknown message layout");
    }
}

std::string CppGenerator::Impl::GenerateFieldName(const ir::MessageDef::FieldDef& fieldDef) {
    return GenerateEscapedName(ToLowerString(fieldDef.Name));
}

std::string CppGenerator::Impl::GenerateEscapedName(const std::string& input) {
    static const std::unordered_set<std::string_view> keywords{"Default", "Serialize",    "DeferSerialize",
                                                               "ParseTo", "ProtobufType", "Descriptor"};
    return GetCppKeywords().contains(input) || keywords.contains(input) ? input + '_' : input;
}

std::string CppGenerator::Impl::GenerateStringDefault(const ir::TypeDef& type) {
    const auto bytes = ir::reflect::GenerateStringDefault(type);
    std::string arrayLiteral = "(uint8_t[]){";
    for (const uint8_t x : bytes) {
        arrayLiteral += "0x" + ToHexCode(x) + ", ";
    }
    arrayLiteral += "}";
    return arrayLiteral;
}

std::string CppGenerator::Impl::GenerateDefaultName(const ir::MessageDef::FieldDef& fieldDef) {
    std::string defaultName = fieldDef.Name + "_DEFAULT";
    std::transform(defaultName.cbegin(), defaultName.cend(), defaultName.begin(), toupper);
    return defaultName;
}

std::string CppGenerator::Impl::GenerateIdName(const ir::MessageDef::FieldDef& fieldDef) {
    std::string idName = "ID_" + (!fieldDef.Deprecated ? fieldDef.Name : "DEPRECATED" + std::to_string(fieldDef.Id));
    std::transform(idName.cbegin(), idName.cend(), idName.begin(), toupper);
    return idName;
}

std::string CppGenerator::Impl::GenerateOneOfCaseEnumName(const std::string& name) {
    return ToCamelCase(name, true) + "Case";
}

std::string CppGenerator::Impl::GenerateOneOfDefaultCaseEnumName(const std::string& name) {
    std::string defaultName = name + "_not_set";
    std::transform(defaultName.cbegin(), defaultName.cend(), defaultName.begin(), toupper);
    return defaultName;
}

std::string CppGenerator::Impl::GenerateOneOfFieldName(const std::string& name,
                                                       const ir::MessageDef::FieldDef* fieldDef) {
    YAFF_REQUIRE(fieldDef && fieldDef->OneOf == name);
    std::string fieldName = "k" + ToCamelCase(fieldDef->Name, true);
    return fieldName;
}

std::string CppGenerator::Impl::GenerateGetterReturnType(const ir::TypeDef& type) {
    // Return type is not real external type,
    // because arrays and nested structres are returned as reference (not WrittenOffset);
    return (!ir::IsScalar(type.Type) ? "const " + GenerateTypeInternal(type) + "&" : GenerateTypeExternal(type));
}

std::string CppGenerator::Impl::GenerateMessageSerializeFuncArgument(const ir::MessageDef::FieldDef& fieldDef) {
    const bool isScalar = ir::IsScalar(fieldDef.Type->Type);
    const bool isOptional = ir::IsExplicitField(fieldDef) && isScalar;
    const std::string fieldType = GenerateTypeExternal(*fieldDef.Type);
    const std::string fieldDefault = (isScalar ? " = " + GenerateDefaultValueExternal(fieldDef) : " = 0");
    const std::string argName = "a_" + GenerateFieldName(fieldDef);
    const std::string argType = (isOptional ? "std::optional<" + fieldType + ">" : fieldType);
    const std::string defaultSuffix = (isOptional ? " = std::nullopt" : fieldDefault);
    return argType + " " + argName + defaultSuffix;
}

std::string CppGenerator::Impl::GenerateMessageBasicSerializeFuncDefault(const ir::MessageDef& msgDef) {
    switch (msgDef.Layout) {
        case MessageLayout::MESSAGE_LAYOUT_FIXED:
            return GenerateMessageSerializerName(MessageType::MESSAGE_TYPE_FIXED, msgDef);
        case MessageLayout::MESSAGE_LAYOUT_FLAT:
            return GenerateMessageSerializerName(MessageType::MESSAGE_TYPE_FLAT, msgDef);
        case MessageLayout::MESSAGE_LAYOUT_SPARSE:
            return GenerateMessageSerializerName(MessageType::MESSAGE_TYPE_SPARSE, msgDef);
        case MessageLayout::MESSAGE_LAYOUT_DYNAMIC: {
            const MessageType type =
                (ir::IsGapMessage(msgDef) ? MessageType::MESSAGE_TYPE_SPARSE : MessageType::MESSAGE_TYPE_FLAT);
            return GenerateMessageSerializerName(type, msgDef);
        }
        default:
            YAFF_THROW("unknown message layout");
    }
}

std::string CppGenerator::Impl::GenerateMessageBasicSerializeFuncCall(const ir::MessageDef& msgDef,
                                                                      const std::string& templateArg) {
    const std::string func = GenerateSerializeFuncName(msgDef);
    std::string result = func + (!templateArg.empty() ? "<" + templateArg + ">" : "") + "(ys";
    ForRealFields(msgDef, [&](const auto& fieldDef) { result += ", a_" + GenerateFieldName(fieldDef); });
    result += ")";
    return result;
}

std::string CppGenerator::Impl::GenerateMessageBasicSerializeFuncDeferredCall(const ir::MessageDef& msgDef,
                                                                              const std::string& templateArg) {
    const std::string func = GenerateSerializeFuncName(msgDef);
    std::string result = "[=, &ys]() { return " + func + (!templateArg.empty() ? "<" + templateArg + ">" : "") + "(ys";
    ForRealFields(msgDef, [&](const auto& fieldDef) { result += ", a_" + GenerateFieldName(fieldDef); });
    result += "); }";
    return result;
}

std::string CppGenerator::Impl::GenerateMessageProtobufType(const ir::MessageDef& msgDef) {
    return ir::IsAssociativePair(msgDef) ? GenerateMessageProtobufPair(msgDef) : GenerateMessageProtobufName(msgDef);
}

std::string CppGenerator::Impl::GenerateMessageProtobufPair(const ir::MessageDef& msgDef) {
    YAFF_REQUIRE(ir::IsAssociativePair(msgDef));
    return "std::pair<const " + GenerateTypeProtobuf(*msgDef.Fields.at(0).Type) + ", " +
           GenerateTypeProtobuf(*msgDef.Fields.at(1).Type) + ">";
}

std::string CppGenerator::Impl::GenerateMessageProtobufMap(const ir::MessageDef& msgDef) {
    YAFF_REQUIRE(ir::IsAssociativePair(msgDef));
    return "::google::protobuf::Map<" + GenerateTypeProtobuf(*msgDef.Fields.at(0).Type) + ", " +
           GenerateTypeProtobuf(*msgDef.Fields.at(1).Type) + ">";
}

std::string CppGenerator::Impl::GenerateEnumProtobufName(const ir::EnumDef& enumDef) {
    const std::string* protoNamespace = FindKey(enumDef.Schema->Attributes, ir::PROTO_NAMESPACE_ATTRIBUTE_NAME);
    YAFF_REQUIRE(protoNamespace);
    return GenerateWithNamespaceName(*protoNamespace, enumDef.Name);
}

std::string CppGenerator::Impl::GenerateMessageProtobufName(const ir::MessageDef& msgDef) {
    const std::string* protoNamespace = FindKey(msgDef.Schema->Attributes, ir::PROTO_NAMESPACE_ATTRIBUTE_NAME);
    YAFF_REQUIRE(protoNamespace);
    return GenerateWithNamespaceName(*protoNamespace, msgDef.Name);
}

std::string CppGenerator::Impl::GenerateMessageColumnName(const ir::MessageDef& msgDef) {
    return msgDef.Name + "Column";
}

std::string CppGenerator::Impl::GenerateMessageStaticMetaName(const ir::MessageDef& msgDef) {
    return msgDef.Name + "Meta";
}

std::string CppGenerator::Impl::GenerateMessageSerializerName(MessageType msgType, const ir::MessageDef& msgDef) {
    return msgDef.Name + GenerateMessageType(msgType) + "Serializer";
}

std::string CppGenerator::Impl::GenerateWithNamespaceName(const std::string& ns, const std::string& name) {
    return ns + "::" + name;
}

CppGenerator::Impl::MessageMeta CppGenerator::Impl::GenerateMessageMeta(const ir::MessageDef& msg) {
    MessageMeta meta;
    const size_t maxId = ir::MaxFieldId(msg);
    meta.FlatOffsets.reserve(maxId);
    meta.StaticFlags.reserve(maxId);
    for (size_t i = 1, j = 0; i <= maxId; ++i) {
        const auto& fieldDef = msg.Fields[j];
        meta.FlatOffsets.emplace_back(fieldDef.FlatOffset);
        meta.StaticFlags.emplace_back(meta.DeletedIds.empty());
        if (fieldDef.Id != i) {
            meta.DeletedIds.emplace_back(i);
        } else {
            ++j;
        }
    }
    meta.FlatOffsets.emplace_back(ir::MaxMessageSize(msg));
    return meta;
}

CppGenerator::CppGenerator(std::ostream& out, CppGenerationOptions opts)
    : Impl_(std::make_unique<Impl>(out, std::move(opts))) {
}

CppGenerator::~CppGenerator() {
}

void CppGenerator::Generate(const ir::IR& ir) {
    Impl_->Generate(ir);
}

}  // namespace yaff::compilation
