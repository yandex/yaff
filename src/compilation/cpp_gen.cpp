#include "cpp_gen.h"

#include <yaff/base.h>
#include <yaff/util.h>

#include <functional>

#include "ir_reflect.h"
#include "util.h"

using namespace std::placeholders;

namespace NYaFF::NCompile {

static bool IsSizeableField(const NIR::TMessageDef::TFieldDef& field) {
    return field.Type->Type == EType::TYPE_VECTOR && !NIR::IsAssociative(*field.Type) &&
           !NIR::IsSequentialMessage(*field.Type);
}

template <typename D>
static bool HasRealFields(const D& def) {
    return std::any_of(def.Fields.begin(), def.Fields.end(),
                       [](const auto& field) { return !std::invoke(&NIR::TMessageDef::TFieldDef::Deprecated, field); });
}

template <typename D>
static bool HasExplicitFields(const D& def) {
    return std::any_of(def.Fields.begin(), def.Fields.end(), [](const auto& field) {
        return std::invoke(&NIR::TMessageDef::TFieldDef::Presence, field) == EPresence::PRESENCE_EXPLICIT;
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
        if (!std::invoke(&NIR::TMessageDef::TFieldDef::Deprecated, fieldDef)) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForReversedRealFields(const D& def, F&& cb) {
    ForReversedFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        if (!std::invoke(&NIR::TMessageDef::TFieldDef::Deprecated, fieldDef)) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForRealScalarFields(const D& def, F&& cb) {
    ForRealFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        const auto* type = std::invoke(&NIR::TMessageDef::TFieldDef::Type, fieldDef);
        if (NIR::IsScalar(type->Type)) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForRealStructureFields(const D& def, F&& cb) {
    ForRealFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        const auto* type = std::invoke(&NIR::TMessageDef::TFieldDef::Type, fieldDef);
        if (!NIR::IsScalar(type->Type)) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForRealStringFields(const D& def, F&& cb) {
    ForRealFields(def, [cb = std::move(cb)](const auto& fieldDef) {
        const auto* type = std::invoke(&NIR::TMessageDef::TFieldDef::Type, fieldDef);
        if (type->Type == EType::TYPE_STRING) {
            cb(fieldDef);
        }
    });
}

template <typename D, typename F>
static void ForSharedOffsets(const D& def, F&& cb) {
    const auto& sharedOffsets = def.SharedOffsets;
    for (auto it = sharedOffsets.begin(); it != sharedOffsets.end(); ++it) {
        cb(it->second);
    }
}

template <typename D, typename F>
static void ForRealSharedOffsets(const D& def, F&& cb) {
    ForSharedOffsets(def, [cb = std::move(cb)](const auto& sharedOffset) {
        if (HasRealFields(sharedOffset)) {
            cb(sharedOffset);
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

class TCppGenerator::TImpl {
public:
    TImpl(std::ostream& out, TCppGenerationOptions opts = {});

    void Generate(const NIR::TIR& ir);

private:
    enum class EMessageType : int32_t {
        FIXED = 0,
        FLAT = 1,
        SPARSE = 2,
    };

    struct TTypeIndex {
        using TDict = std::unordered_map<const NIR::TType*, size_t>;
        using TList = std::vector<TDict::value_type*>;

        TDict Dict;
        TList List;

        void Add(const NIR::TType* t) {
            auto [it, emplaced] = Dict.try_emplace(t);
            if (emplaced) {
                it->second = List.size();
                List.emplace_back(&*it);
            }
        }

        size_t At(const NIR::TType* t) const {
            const size_t* idx = FindKey(Dict, t);
            YAFF_REQUIRE(idx);
            return *idx;
        }

        template <typename F>
        void ForEach(F&& cb) const {
            for (const auto* it : List) {
                cb(*it->first);
            }
        }
    };

    using TMessageLayoutGenerator = std::function<void(EMessageType, const NIR::TMessageDef&)>;

    // External experimental generators;
    static std::string GenerateMessageColumnName(const NIR::TMessageDef& msgDef);

    static std::string GenerateMessageType(const EMessageType type);
    static std::string GenerateBasicType(const EType type);
    static std::string GenerateTypeProtobuf(const NIR::TType& type);
    static std::string GenerateTypeInternal(const NIR::TType& type);
    static std::string GenerateTypeExternal(const NIR::TType& type);
    static std::string GenerateGetterReturnType(const NIR::TType& type);

    static std::string GenerateBasicDefault(const EType type);
    static std::string GenerateStringDefault(const NIR::TType& type);
    static std::string GenerateDefaultValueInternal(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateDefaultValueExternal(const NIR::TMessageDef::TFieldDef& fieldDef);

    static std::string GenerateTypeCastInternal(const NIR::TType& type, const std::string& call);
    static std::string GenerateTypeCastExternal(const NIR::TType& type, const std::string& call);

    static std::string GenerateBasicTypeDescriptor(const EType type);
    static std::string GenerateCompositeTypeDescriptor(const NIR::TType& type, const TTypeIndex& index);
    static std::string GeneratePresenceDescriptor(const EPresence presence);
    static std::string GenerateMessageLayoutDescriptor(const EMessageLayout layout);
    static std::string GenerateDefaultDescriptor(const NIR::TType& type);
    static std::string GenerateDescriptorFuncName(const std::string& name);
    static std::string GenerateEnumDescriptorFuncDeclaration(const std::string& name);
    static std::string GenerateMessageDescriptorFuncDeclaration(const std::string& name);

    static std::string GenerateDefaultFuncName(const std::string& name);
    static std::string GenerateMessageDefaultFuncDeclaration(const std::string& ret, const std::string& name);

    static std::string GenerateProtobufValue(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateProtobufPairValue(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateProtobufPairFill(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateProtobufStructFill(const std::string& mutableCall, const std::string& clearCall,
                                                  const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateProtobufScalarFill(const std::string& mutableCall, const std::string& clearCall,
                                                  const NIR::TMessageDef::TFieldDef& fieldDef);

    static std::string GenerateExternalPairName(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateExternalCreate(const std::string& getCall, const std::string& getSuffix,
                                              const NIR::TType& type);
    static std::string GenerateDeferredCreateFuncName(const NIR::TMessageDef& msgDef);
    static std::string GenerateCreateFuncName(const NIR::TMessageDef& msgDef);
    static std::string GenerateTransformFuncName(const NIR::TMessageDef& msgDef);

    static std::string GenerateDeferredCreateFromProtobufFuncDeclaration(const NIR::TMessageDef& msgDef);
    static std::string GenerateCreateFromProtobufFuncDeclaration(const NIR::TMessageDef& msgDef);
    static std::string GenerateTransformToProtobufFuncDeclaration(const NIR::TMessageDef& msgDef);

    static std::string GenerateMessageCreateFuncArgument(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateMessageBasicCreateFuncCall(const NIR::TMessageDef& msgDef,
                                                          const std::string& templateArg = "");
    static std::string GenerateMessageBasicCreateFuncDeferredCall(const NIR::TMessageDef& msgDef,
                                                                  const std::string& templateArg = "");
    static std::string GenerateMessageBasicCreateFuncDefault(const NIR::TMessageDef& msgDef);

    static std::string GenerateEscapedName(const std::string& input);
    static std::string GenerateFieldName(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateDefaultName(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateIdName(const NIR::TMessageDef::TFieldDef& fieldDef);
    static std::string GenerateSharedOffsetCaseEnumName(const std::string& name);
    static std::string GenerateSharedOffsetDefaultCaseEnumName(const std::string& name);
    static std::string GenerateSharedOffsetFieldName(const std::string& name,
                                                     const NIR::TMessageDef::TFieldDef* fieldDef);
    static std::string GenerateMessageBuilderName(EMessageType msgType, const NIR::TMessageDef& msgDef);
    static std::string GenerateMessageStaticMetaName(const NIR::TMessageDef& msgDef);
    static std::string GenerateMessageProtobufName(const NIR::TMessageDef& msgDef);
    static std::string GenerateMessageProtobufPair(const NIR::TMessageDef& msgDef);
    static std::string GenerateMessageProtobufMap(const NIR::TMessageDef& msgDef);
    static std::string GenerateMessageProtobufType(const NIR::TMessageDef& msgDef);
    static std::string GenerateMessageBaseClass(const NIR::TMessageDef& msgDef);
    static std::string GenerateEnumProtobufName(const NIR::TEnumDef& enumDef);
    static std::string GenerateWithNamespaceName(const std::string& ns, const std::string& name);

    void GenerateHeader();
    void GenerateIncludes(const NIR::TSchemaDef& schema);
    void GenerateSchema(const NIR::TSchemaDef& ir);

    void GenerateEnum(const NIR::TEnumDef& enumDef);
    void GenerateEnumPre(const NIR::TEnumDef& enumDef);
    void GenerateEnumPost(const NIR::TEnumDef& enumDef);

    void GenerateMessage(const NIR::TMessageDef& msgDef);
    void GenerateMessagePre(const NIR::TMessageDef& msgDef);
    void GenerateMessagePost(const NIR::TMessageDef& msgDef);

    void GenerateMessageFieldDescriptors(const NIR::TMessageDef& msgDef);
    void GenerateMessageDescriptor(const NIR::TMessageDef& msgDef);
    void GenerateMessageAliasDescriptor(const NIR::TMessageDef& msgDef);
    void GenerateTypeDescriptor(const NIR::TType& type);

    void GenerateEnumValueDescriptors(const NIR::TEnumDef& enumDef);
    void GenerateEnumDescriptor(const NIR::TEnumDef& enumDef);
    void GenerateEnumStruct(const NIR::TEnumDef& enumDef);
    void GenerateEnumConst(const NIR::TEnumDef& enumDef);
    void GenerateEnumIsValidFunc(const NIR::TEnumDef& enumDef);
    void GenerateEnumNameFunc(const NIR::TEnumDef& enumDef);

    void GenerateMessageTransformToProtobuf(const NIR::TMessageDef& msgDef);
    void GenerateMessageProtobufBuilder(const NIR::TMessageDef& msgDef, const bool deferred = false);
    void GenerateMessageAliasCreateFunc(const NIR::TMessageDef& msgDef);
    void GenerateMessageAliasDeferredCreateFunc(const NIR::TMessageDef& msgDef);
    void GenerateMessageAliasTransformFunc(const NIR::TMessageDef& msgDef);
    void GenerateMessageBasicCreateFunc(const NIR::TMessageDef& msgDef);
    void GenerateMessageDynamicCreateFunc(const NIR::TMessageDef& msgDef);
    void GenerateMessageBuilder(EMessageType msgType, const NIR::TMessageDef& msgDef);
    void GenerateMessageIdsEnum(const NIR::TMessageDef& msgDef);
    void GenerateMessageStaticMeta(const NIR::TMessageDef& msgDef);
    void GenerateMessageAliasDefaultFunc(const NIR::TMessageDef& msgDef);
    void GenerateMessageDefaultFunc(const NIR::TMessageDef& msgDef);
    void GenerateMessageFieldGet(const NIR::TMessageDef::TFieldDef& fieldDef);
    void GenerateMessageFieldGetIdx(const NIR::TMessageDef::TFieldDef& fieldDef);
    void GenerateMessageFieldCheck(const NIR::TMessageDef::TFieldDef& fieldDef);
    void GenerateMessageFieldSize(const NIR::TMessageDef::TFieldDef& fieldDef);
    void GenerateMessageFieldCompare(const std::string& msgName, const NIR::TMessageDef::TFieldDef& fieldDef);
    void GenerateMessageFieldAdd(const std::string& msgName, const NIR::TMessageDef::TFieldDef& fieldDef);
    void GenerateMessageSharedOffsetCase(const NIR::TMessageDef::TSharedOffset& sharedOffset);
    void GenerateByMessageLayout(const NIR::TMessageDef& msgDef, TMessageLayoutGenerator gen);

    TCppGenerationOptions Opts_;
    TCodeWriter Writer_;
};

TCppGenerator::TImpl::TImpl(std::ostream& out, TCppGenerationOptions opts)
    : Opts_(std::move(opts)), Writer_(out, "\t") {
}

void TCppGenerator::TImpl::Generate(const NIR::TIR& ir) {
    auto schemaOrder = GetSchemaDependencyOrder(ir);
    for (const auto* schema : schemaOrder) {
        if (schema->Defined) {
            GenerateSchema(*schema);
        }
    }
}

void TCppGenerator::TImpl::GenerateSchema(const NIR::TSchemaDef& schema) {
    GenerateHeader();
    GenerateIncludes(schema);

    const bool includeProto = (Opts_.GenerateProtobufApi && !Opts_.SuppressProtobufIncludes);
    if (includeProto) {
        const std::string* protoPath = FindKey(schema.Attributes, NIR::PROTO_FILE_ATTRIBUTE_NAME);
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

void TCppGenerator::TImpl::GenerateHeader() {
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
}

void TCppGenerator::TImpl::GenerateIncludes(const NIR::TSchemaDef& schema) {
    for (const auto* depend : schema.Dependencies) {
        if (!depend->Defined) {
            Writer_ |= "#include \"" + depend->Name + Opts_.IncludesSuffix + ".yaff.h\"";
        }
    }

    if (!schema.Dependencies.empty()) {
        Writer_ |= "";
    }
}

void TCppGenerator::TImpl::GenerateEnum(const NIR::TEnumDef& enumDef) {
    GenerateEnumStruct(enumDef);
    GenerateEnumConst(enumDef);
    GenerateEnumIsValidFunc(enumDef);
    GenerateEnumNameFunc(enumDef);
}

void TCppGenerator::TImpl::GenerateEnumStruct(const NIR::TEnumDef& enumDef) {
    Writer_ |= "enum class " + enumDef.Name + " : int32_t {";
    ForValues(enumDef, [&](const auto& enumVal) {
        Writer_ >= GenerateEscapedName(enumVal.Name) + " = " + std::to_string(enumVal.Value) + ",";
    });
    Writer_ |= "};\n";
}

void TCppGenerator::TImpl::GenerateEnumConst(const NIR::TEnumDef& enumDef) {
    Writer_ |= "inline constexpr " + enumDef.Name + " " + enumDef.Name + "_MAX = static_cast<" + enumDef.Name + ">(" +
               std::to_string(enumDef.Values.back().Value) + ");";
    Writer_ |= "inline constexpr " + enumDef.Name + " " + enumDef.Name + "_MIN = static_cast<" + enumDef.Name + ">(" +
               std::to_string(enumDef.Values.front().Value) + ");";
    Writer_ |= "inline constexpr int " + enumDef.Name +
               "_ARRAYSIZE = " + std::to_string(enumDef.Values.back().Value + 1) + ";\n";
}

void TCppGenerator::TImpl::GenerateEnumIsValidFunc(const NIR::TEnumDef& enumDef) {
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

void TCppGenerator::TImpl::GenerateEnumNameFunc(const NIR::TEnumDef& enumDef) {
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

void TCppGenerator::TImpl::GenerateEnumPre(const NIR::TEnumDef& enumDef) {
    Writer_ |= "enum class " + enumDef.Name + " : int32_t;";

    if (Opts_.GenerateReflectionApi) {
        Writer_ |= GenerateEnumDescriptorFuncDeclaration(enumDef.Name) + ";";
    }

    Writer_ |= "";
}

void TCppGenerator::TImpl::GenerateEnumPost(const NIR::TEnumDef& enumDef) {
    if (Opts_.GenerateReflectionApi) {
        GenerateEnumDescriptor(enumDef);
    }
}

void TCppGenerator::TImpl::GenerateMessage(const NIR::TMessageDef& msgDef) {
    if (NIR::IsStaticMetaMessage(msgDef)) {
        GenerateMessageStaticMeta(msgDef);
    }

    const std::string baseClass = GenerateMessageBaseClass(msgDef);
    Writer_ |= "YAFF_LAYOUT_BEGIN(" + msgDef.Name + ") final : private " + baseClass + " {";

    Writer_.TrackText();
    Writer_ >= "using TMetaType = " +
                   (NIR::IsStaticMetaMessage(msgDef) ? GenerateMessageStaticMetaName(msgDef) : "void") + ";";
    if (Opts_.GenerateProtobufApi) {
        Writer_ >= "using TProtobufType = " + GenerateMessageProtobufType(msgDef) + ";";
    }
    if (Writer_.TextWritten()) {
        Writer_ |= "";
    }

    if (!msgDef.Fields.empty()) {
        auto messageStructGuard = Writer_.IdentGuard();

        ForRealStringFields(msgDef, [&](const auto& fieldDef) {
            if (fieldDef.Type->Modifiers.contains(NIR::DEFAULT_MODIFIER_NAME)) {
                Writer_ |= "inline static const auto& " + GenerateDefaultName(fieldDef) +
                           " = *::NYaFF::ReadLayout<::NYaFF::TString>(" + GenerateStringDefault(*fieldDef.Type) + ");";
            }
        });

        GenerateMessageIdsEnum(msgDef);
        ForSharedOffsets(msgDef, [&](const auto& sharedOffset) {
            Writer_ |= "enum " + GenerateSharedOffsetCaseEnumName(sharedOffset.Name) + " : ::NYaFF::TFieldId {";
            ForFields(sharedOffset, [&](const auto* fieldDef) {
                if (fieldDef->Deprecated) {
                    return;
                }
                Writer_ >= GenerateSharedOffsetFieldName(sharedOffset.Name, fieldDef) + " = " +
                               std::to_string(fieldDef->SharedOffsetId) + ",";
            });
            Writer_ >= GenerateSharedOffsetDefaultCaseEnumName(sharedOffset.Name) + " = 0,";
            Writer_ |= "};\n";
        });

        ForSharedOffsets(msgDef, [&](const auto& sharedOffset) { GenerateMessageSharedOffsetCase(sharedOffset); });

        ForRealFields(msgDef, [&](const auto& fieldDef) {
            if (NIR::IsExplicitField(fieldDef)) {
                GenerateMessageFieldCheck(fieldDef);
            }
            if (IsSizeableField(fieldDef)) {
                GenerateMessageFieldSize(fieldDef);
                GenerateMessageFieldGetIdx(fieldDef);
            }
            GenerateMessageFieldGet(fieldDef);
        });
    }

    if (NIR::IsAssociativePair(msgDef)) {
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
        GenerateMessageAliasCreateFunc(msgDef);
        if (NIR::IsFixedMessage(msgDef)) {
            GenerateMessageAliasDeferredCreateFunc(msgDef);
        }
    }

    Writer_ |= "};";
    Writer_ |= "YAFF_LAYOUT_END\n";

    GenerateByMessageLayout(msgDef, std::bind(&TCppGenerator::TImpl::GenerateMessageBuilder, this, _1, _2));

    GenerateMessageBasicCreateFunc(msgDef);
    if (NIR::IsDynamicMessage(msgDef) && !NIR::IsGapMessage(msgDef)) {
        GenerateMessageDynamicCreateFunc(msgDef);
    }
}

void TCppGenerator::TImpl::GenerateMessageFieldCompare(const std::string& msgName,
                                                       const NIR::TMessageDef::TFieldDef& fieldDef) {
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

void TCppGenerator::TImpl::GenerateByMessageLayout(const NIR::TMessageDef& msgDef, TMessageLayoutGenerator gen) {
    switch (msgDef.Layout) {
        case EMessageLayout::MESSAGE_LAYOUT_FIXED: {
            gen(EMessageType::FIXED, msgDef);
            break;
        }
        case EMessageLayout::MESSAGE_LAYOUT_FLAT: {
            gen(EMessageType::FLAT, msgDef);
            break;
        }
        case EMessageLayout::MESSAGE_LAYOUT_SPARSE: {
            gen(EMessageType::SPARSE, msgDef);
            break;
        }
        case EMessageLayout::MESSAGE_LAYOUT_DYNAMIC: {
            if (!NIR::IsGapMessage(msgDef)) {
                gen(EMessageType::FLAT, msgDef);
            }
            gen(EMessageType::SPARSE, msgDef);
            break;
        }
        default:
            YAFF_THROW("unknown message layout");
    }
}

void TCppGenerator::TImpl::GenerateMessageIdsEnum(const NIR::TMessageDef& msgDef) {
    Writer_ |= "enum EMessageIds : ::NYaFF::TFieldId {";
    ForFields(msgDef, [&](const auto& fieldDef) {
        Writer_ >= GenerateIdName(fieldDef) + " = " + std::to_string(fieldDef.Id) + ",";
    });
    Writer_ |= "};\n";
}

void TCppGenerator::TImpl::GenerateMessageStaticMeta(const NIR::TMessageDef& msgDef) {
    YAFF_REQUIRE(NIR::IsStaticMetaMessage(msgDef));

    Writer_ |= "struct " + GenerateMessageStaticMetaName(msgDef) + " {";
    Writer_.IncrementIdentLevel();

    if (NIR::IsFixedMessage(msgDef)) {
        Writer_ |= "inline static constexpr size_t LIMIT = " + std::to_string(NIR::FixedMessageSize(msgDef)) + ";";
    }

    Writer_ |= "inline static constexpr std::array<NYaFF::TFieldOffset, " + std::to_string(msgDef.Fields.size()) +
               "> FLAT_OFFSETS = {\\";
    ForFields(msgDef, [&](const auto& fieldDef) { Writer_ >= std::to_string(fieldDef.FlatOffset) + ", \\"; });
    Writer_ |= "};\n";

    Writer_ |= "inline static constexpr NYaFF::TFieldOffset ResolveField(const NYaFF::TFieldId id) {";
    Writer_ >= "return FLAT_OFFSETS[id - 1];";
    Writer_ |= "}";

    Writer_.DecrementIdentLevel();
    Writer_ |= "};\n";
}

void TCppGenerator::TImpl::GenerateMessageBuilder(EMessageType msgType, const NIR::TMessageDef& msgDef) {
    const std::string messageSuffix = GenerateMessageType(msgType);
    const std::string builderName = GenerateMessageBuilderName(msgType, msgDef);

    Writer_ |= "struct " + builderName + " {";
    Writer_.IncrementIdentLevel();

    Writer_ |= "::NYaFF::TBuilder& B;\n";

    const std::string templateArgs =
        (msgType == EMessageType::FIXED || msgType == EMessageType::FLAT ? "<" + msgDef.Name + "::TMetaType" + ">"
                                                                         : "");
    const std::string callArgs =
        ((msgType == EMessageType::FLAT || msgType == EMessageType::SPARSE) && !HasExplicitFields(msgDef)
             ? "/* implicit */ true"
             : "");
    Writer_ |= "explicit " + builderName + "(::NYaFF::TBuilder& yffb)";
    Writer_ >= ": B(yffb)";
    Writer_ |= "{";
    Writer_ >= "B.Start" + messageSuffix + "Message" + templateArgs + "(" + callArgs + ");";
    Writer_ |= "}\n";

    ForRealFields(msgDef, [&](const auto& fieldDef) { GenerateMessageFieldAdd(msgDef.Name, fieldDef); });

    const std::string finishType = "::NYaFF::TInternalOffset<" + msgDef.Name + ">";
    Writer_ |= finishType + " Finish() && {";
    Writer_ >= "return " + finishType + "(B.Finish" + messageSuffix + "Message());";
    Writer_ |= "}";

    Writer_.DecrementIdentLevel();
    Writer_ |= "};\n";
}

void TCppGenerator::TImpl::GenerateMessageBasicCreateFunc(const NIR::TMessageDef& msgDef) {
    Writer_ |= "template <typename TUnderlying = " + GenerateMessageBasicCreateFuncDefault(msgDef) + ">";
    Writer_ |= "inline ::NYaFF::TInternalOffset<" + msgDef.Name + "> " + GenerateCreateFuncName(msgDef) + "(";
    Writer_ >= "::NYaFF::TBuilder& yffb\\";
    ForRealFields(msgDef, [&](const auto& fieldDef) {
        Writer_ >= ",";
        Writer_ >= GenerateMessageCreateFuncArgument(fieldDef) + "\\";
    });
    Writer_ |= "";
    Writer_ |= ") {";
    Writer_ >= "TUnderlying b(yffb);";

    ForReversedRealFields(msgDef, [&](const auto& fieldDef) {
        const bool isOptional = NIR::IsScalar(fieldDef.Type->Type) && NIR::IsExplicitField(fieldDef);
        const std::string argName = "a_" + GenerateFieldName(fieldDef);
        const std::string argPrefix = (isOptional ? "*" : "");
        const std::string addCall = "b.add_" + GenerateFieldName(fieldDef) + "(" + argPrefix + argName + ");";
        Writer_ >= (isOptional ? "if (" + argName + ") { " + addCall + " }" : addCall);
    });

    Writer_ >= "return std::move(b).Finish();";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageDynamicCreateFunc(const NIR::TMessageDef& msgDef) {
    const std::string createFuncName = GenerateCreateFuncName(msgDef);
    Writer_ |= "inline ::NYaFF::TInternalOffset<" + msgDef.Name + "> " + createFuncName + "(";
    Writer_ >= "::NYaFF::TBuilder& yffb\\";
    ForRealFields(msgDef, [&](const auto& fieldDef) {
        Writer_ >= ",";
        Writer_ >= GenerateMessageCreateFuncArgument(fieldDef) + "\\";
    });
    Writer_ |= "";
    Writer_ |= ") {";
    Writer_.IncrementIdentLevel();
    Writer_ |= "switch (yffb.GetForcedDynamicAlternative()) {";
    Writer_ |= "case ::NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT: {";
    Writer_ >= "return " +
                   GenerateMessageBasicCreateFuncCall(msgDef, GenerateMessageBuilderName(EMessageType::FLAT, msgDef)) +
                   ";";
    Writer_ |= "}";
    Writer_ |= "case ::NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE: {";
    Writer_ >=
        "return " +
            GenerateMessageBasicCreateFuncCall(msgDef, GenerateMessageBuilderName(EMessageType::SPARSE, msgDef)) + ";";
    Writer_ |= "}";
    Writer_ |= "default:";
    Writer_ >= "YAFF_THROW(\"unknown preferable layout in '" + createFuncName + "'\");";
    Writer_ |= "}";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageFieldAdd(const std::string& msgName,
                                                   const NIR::TMessageDef::TFieldDef& fieldDef) {
    const std::string valueArgType = GenerateTypeExternal(*fieldDef.Type);
    const std::string valueArgName = GenerateFieldName(fieldDef);

    Writer_ |= "void add_" + GenerateFieldName(fieldDef) + "(" + valueArgType + " " + valueArgName + ") {";

    const std::string type = GenerateTypeInternal(*fieldDef.Type);
    const std::string id = msgName + "::" + GenerateIdName(fieldDef);
    const std::string defaultSuffix =
        (NIR::IsScalar(fieldDef.Type->Type) ? ", " + GenerateDefaultValueInternal(fieldDef) : "");
    Writer_ >= "B.AddField<" + type + ">(" + id + ", " + GenerateTypeCastInternal(*fieldDef.Type, valueArgName) +
                   defaultSuffix + ");";

    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageSharedOffsetCase(const NIR::TMessageDef::TSharedOffset& sharedOffset) {
    const std::string enumName = GenerateSharedOffsetCaseEnumName(sharedOffset.Name);
    Writer_ |= enumName + " " + sharedOffset.Name + "_case() const {";
    Writer_.IncrementIdentLevel();
    ForFields(sharedOffset, [&](const auto* field) {
        Writer_ |= "if (ReadPresence<" + GenerateTypeInternal(*field->Type) + ">(" + GenerateIdName(*field) + ")) {";
        Writer_ >= "return " + GenerateSharedOffsetFieldName(sharedOffset.Name, field) + ";";
        Writer_ |= "}";
    });
    Writer_ |= "return " + GenerateSharedOffsetDefaultCaseEnumName(sharedOffset.Name) + ";";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";

    auto generateCaseGet = [&](const std::string& get) {
        Writer_ |= enumName + " " + get + "() const {";
        Writer_ >= "return " + sharedOffset.Name + "_case();";
        Writer_ |= "}\n";
    };
    generateCaseGet("Get" + sharedOffset.Name + "Case");
    if (const std::string camelName = ToCamelCase(sharedOffset.Name, true); camelName != sharedOffset.Name) {
        generateCaseGet("Get" + camelName + "Case");
    }
}

void TCppGenerator::TImpl::GenerateMessageFieldGet(const NIR::TMessageDef::TFieldDef& fieldDef) {
    Writer_ |=
        "YAFF_PURE " + GenerateGetterReturnType(*fieldDef.Type) + " " + GenerateFieldName(fieldDef) + "() const {";
    Writer_.IncrementIdentLevel();

    const bool isScalar = NIR::IsScalar(fieldDef.Type->Type);
    const std::string valuePrefix = (isScalar ? "" : "&");
    const std::string reader = (isScalar ? "ReadValue" : "*ReadLayout");
    const std::string readCall = reader + "<" + GenerateTypeInternal(*fieldDef.Type) + ">(" + GenerateIdName(fieldDef) +
                                 ", " + valuePrefix + GenerateDefaultValueInternal(fieldDef) + ")";
    Writer_ |= "return " + GenerateTypeCastExternal(*fieldDef.Type, readCall) + ";";

    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";

    Writer_ |= "YAFF_PURE " + GenerateGetterReturnType(*fieldDef.Type) + " " + "Get" +
               GenerateEscapedName(fieldDef.Name) + "() const {";
    Writer_ >= "return " + GenerateFieldName(fieldDef) + "();";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageFieldGetIdx(const NIR::TMessageDef::TFieldDef& fieldDef) {
    YAFF_REQUIRE(IsSizeableField(fieldDef));

    Writer_ |= "YAFF_PURE " + GenerateGetterReturnType(*fieldDef.Type->ElementType) + " " +
               GenerateFieldName(fieldDef) + "(const int i) const {";
    Writer_ >= "return " + GenerateFieldName(fieldDef) + "()[i];";
    Writer_ |= "}\n";

    Writer_ |= "YAFF_PURE " + GenerateGetterReturnType(*fieldDef.Type->ElementType) + " Get" +
               GenerateFieldName(fieldDef) + "(const size_t i) const {";
    Writer_.IncrementIdentLevel();
    Writer_ |= "YAFF_ASSERT(i < static_cast<size_t>(std::numeric_limits<int>::max()));";
    Writer_ |= "return " + GenerateFieldName(fieldDef) + "(i);";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageFieldCheck(const NIR::TMessageDef::TFieldDef& fieldDef) {
    YAFF_REQUIRE(NIR::IsExplicitField(fieldDef));

    Writer_ |= "YAFF_PURE bool has_" + GenerateFieldName(fieldDef) + "() const {";
    Writer_.IncrementIdentLevel();

    const std::string checkCall =
        "ReadPresence<" + GenerateTypeInternal(*fieldDef.Type) + ">(" + GenerateIdName(fieldDef) + ")";
    Writer_ |= "return " + checkCall + ";";

    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";

    Writer_ |= "YAFF_PURE bool Has" + GenerateEscapedName(fieldDef.Name) + "() const {";
    Writer_ >= "return has_" + GenerateFieldName(fieldDef) + "();";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageFieldSize(const NIR::TMessageDef::TFieldDef& fieldDef) {
    YAFF_REQUIRE(IsSizeableField(fieldDef));

    Writer_ |= "YAFF_PURE int " + GenerateFieldName(fieldDef) + "_size() const {";
    Writer_ >= "return " + GenerateFieldName(fieldDef) + "().size();";
    Writer_ |= "}\n";

    Writer_ |= "YAFF_PURE size_t " + GenerateEscapedName(fieldDef.Name) + "Size() const {";
    Writer_ >= "return static_cast<size_t>(" + GenerateFieldName(fieldDef) + "_size());";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessagePre(const NIR::TMessageDef& msgDef) {
    Writer_ |= "struct " + msgDef.Name + ";";

    if (Opts_.GenerateProtobufApi) {
        Writer_ |= GenerateTransformToProtobufFuncDeclaration(msgDef) + ";";
        Writer_ |= GenerateCreateFromProtobufFuncDeclaration(msgDef) + ";";
        if (NIR::IsFixedMessage(msgDef)) {
            Writer_ |= GenerateDeferredCreateFromProtobufFuncDeclaration(msgDef) + ";";
        }
    }

    if (Opts_.GenerateReflectionApi) {
        Writer_ |= GenerateMessageDescriptorFuncDeclaration(msgDef.Name) + ";";
    }

    Writer_ |= GenerateMessageDefaultFuncDeclaration(msgDef.Name, msgDef.Name) + ";";
    Writer_ |= "";

    // Forward declarations for external experimental generation;
    Writer_ |= "template <template <typename> typename S> struct " + GenerateMessageColumnName(msgDef) + ";";
    Writer_ |= GenerateMessageDefaultFuncDeclaration(GenerateMessageColumnName(msgDef) + "<::NYaFF::NExp::TSizeable>",
                                                     GenerateMessageColumnName(msgDef)) +
               ";";
    Writer_ |= "";
}

void TCppGenerator::TImpl::GenerateMessagePost(const NIR::TMessageDef& msgDef) {
    GenerateMessageDefaultFunc(msgDef);

    if (Opts_.GenerateProtobufApi) {
        GenerateMessageTransformToProtobuf(msgDef);
        GenerateMessageProtobufBuilder(msgDef, /* deferred */ false);
        if (NIR::IsFixedMessage(msgDef)) {
            GenerateMessageProtobufBuilder(msgDef, /* deferred */ true);
        }
    }

    if (Opts_.GenerateReflectionApi) {
        GenerateMessageDescriptor(msgDef);
    }
}

void TCppGenerator::TImpl::GenerateEnumDescriptor(const NIR::TEnumDef& enumDef) {
    Writer_ |= GenerateEnumDescriptorFuncDeclaration(enumDef.Name) + " {";
    Writer_.IncrementIdentLevel();

    const bool hasValues = !enumDef.Values.empty();
    if (hasValues) {
        GenerateEnumValueDescriptors(enumDef);
    }

    Writer_ |= "static constexpr ::NYaFF::NReflect::TEnumDescriptor enm = {";
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

void TCppGenerator::TImpl::GenerateEnumValueDescriptors(const NIR::TEnumDef& enumDef) {
    Writer_ |= "static constexpr ::NYaFF::NReflect::TEnumValueDescriptor values[] = {";
    ForValues(enumDef, [&](const auto& enumVal) {
        auto g = Writer_.IdentGuard();
        Writer_ |= "{";
        Writer_ >= ".Name = \"" + enumVal.Name + "\",";
        Writer_ >= ".Value = " + std::to_string(enumVal.Value) + ",";
        Writer_ |= "},";
    });
    Writer_ |= "};\n";
}

void TCppGenerator::TImpl::GenerateMessageAliasDescriptor(const NIR::TMessageDef& msgDef) {
    Writer_ |= "static const ::NYaFF::NReflect::TMessageDescriptor* Descriptor() {";
    Writer_ >= "return " + GenerateDescriptorFuncName(msgDef.Name) + "();";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageDescriptor(const NIR::TMessageDef& msgDef) {
    Writer_ |= GenerateMessageDescriptorFuncDeclaration(msgDef.Name) + " {";
    Writer_.IncrementIdentLevel();

    const bool hasFields = !msgDef.Fields.empty();
    if (hasFields) {
        GenerateMessageFieldDescriptors(msgDef);
    }

    Writer_ |= "static constexpr ::NYaFF::NReflect::TMessageDescriptor message = {";
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

void TCppGenerator::TImpl::GenerateMessageFieldDescriptors(const NIR::TMessageDef& msgDef) {
    TTypeIndex index;
    ForFields(msgDef, [&](const auto& fieldDef) {
        const auto* type = fieldDef.Type;
        index.Add(type);
        if (type->Type == EType::TYPE_VECTOR && type->ElementType) {
            index.Add(type->ElementType);
        }
    });

    Writer_ |= "static const ::NYaFF::NReflect::TTypeDescriptor types[] = {";
    index.ForEach([&](const auto& type) {
        auto g = Writer_.IdentGuard();
        Writer_ |= "{";
        Writer_ >= ".Type = " + GenerateBasicTypeDescriptor(type.Type) + ",";
        Writer_ >= ".Descriptor = " + GenerateCompositeTypeDescriptor(type, index) + ",";
        Writer_ >= ".Default = " + GenerateDefaultDescriptor(type) + ",";
        if (NIR::IsAssociative(type)) {
            Writer_ >= ".Associative = true,";
        }
        if (NIR::IsInline(type)) {
            Writer_ >= ".Inline = true,";
        }
        Writer_ |= "},";
    });
    Writer_ |= "};";

    Writer_ |= "static constexpr ::NYaFF::NReflect::TFieldDescriptor fields[] = {";
    ForFields(msgDef, [&](const auto& fieldDef) {
        auto g = Writer_.IdentGuard();
        Writer_ |= "{";
        Writer_ >= ".Id = " + std::to_string(fieldDef.Id) + ",";
        if (!fieldDef.Deprecated) {
            Writer_ >= ".Name = \"" + fieldDef.Name + "\",";
        }
        if (!fieldDef.SharedOffsetVia.empty()) {
            Writer_ >= ".ContainingOneof = \"" + fieldDef.SharedOffsetVia + "\",";
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

void TCppGenerator::TImpl::GenerateMessageDefaultFunc(const NIR::TMessageDef& msgDef) {
    Writer_ |= GenerateMessageDefaultFuncDeclaration(msgDef.Name, msgDef.Name) + " {";
    Writer_ >= "return " + msgDef.Name + "::Default();";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageAliasDefaultFunc(const NIR::TMessageDef& msgDef) {
    Writer_ |= "static const " + msgDef.Name + "& Default() {";
    Writer_ >= "return *::NYaFF::ReadLayout<" + msgDef.Name + ">(DEFAULT_MESSAGE);";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageAliasCreateFunc(const NIR::TMessageDef& msgDef) {
    Writer_ |= "template <typename ...Ps>";
    Writer_ |=
        "static ::NYaFF::TInternalOffset<" + msgDef.Name + "> CreateFrom(::NYaFF::TBuilder& yffb, Ps&&... params) {";
    Writer_ >= "return " + GenerateCreateFuncName(msgDef) + "(yffb, std::forward<Ps>(params)...);";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageAliasDeferredCreateFunc(const NIR::TMessageDef& msgDef) {
    Writer_ |= "template <typename ...Ps>";
    Writer_ |= "static auto DeferCreateFrom(::NYaFF::TBuilder& yffb, Ps&&... params) {";
    Writer_ >= "return " + GenerateDeferredCreateFuncName(msgDef) + "(yffb, std::forward<Ps>(params)...);";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageAliasTransformFunc(const NIR::TMessageDef& msgDef) {
    Writer_ |= "template <typename T>";
    Writer_ |= "void TransformTo(T& to) const {";
    Writer_ >= GenerateWithNamespaceName(msgDef.Schema->Namespace, GenerateTransformFuncName(msgDef)) + "(*this, to);";
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageTransformToProtobuf(const NIR::TMessageDef& msgDef) {
    Writer_ |= GenerateTransformToProtobufFuncDeclaration(msgDef) + " {";
    Writer_.IncrementIdentLevel();

    if (NIR::IsAssociativePair(msgDef)) {
        Writer_ |= GenerateProtobufPairFill(msgDef.Fields[0]);
        Writer_ |= GenerateProtobufPairFill(msgDef.Fields[1]);
    } else {
        // Read and transform scalars first to increase read locality.
        ForRealScalarFields(msgDef, [&](const auto& fieldDef) {
            Writer_ |=
                GenerateProtobufScalarFill("proto", "proto.clear_" + GenerateFieldName(fieldDef) + "()", fieldDef);
        });
        ForRealStructureFields(msgDef, [&](const auto& fieldDef) {
            Writer_ |= GenerateProtobufStructFill("proto.mutable_" + GenerateFieldName(fieldDef) + "()",
                                                  "proto.clear_" + GenerateFieldName(fieldDef) + "()", fieldDef);
        });
    }

    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

void TCppGenerator::TImpl::GenerateMessageProtobufBuilder(const NIR::TMessageDef& msgDef, const bool deferred) {
    const std::string declaration = (deferred ? GenerateDeferredCreateFromProtobufFuncDeclaration(msgDef)
                                              : GenerateCreateFromProtobufFuncDeclaration(msgDef));
    Writer_ |= declaration + " {";
    Writer_.IncrementIdentLevel();

    ForRealFields(msgDef, [&](const auto& fieldDef) {
        const std::string protobufValue =
            (NIR::IsAssociativePair(msgDef) ? GenerateProtobufPairValue(fieldDef) : GenerateProtobufValue(fieldDef));
        Writer_ |= "const auto a_" + GenerateFieldName(fieldDef) + " = " + protobufValue + ";";
    });

    const std::string createCall =
        (deferred ? GenerateMessageBasicCreateFuncDeferredCall(msgDef) : GenerateMessageBasicCreateFuncCall(msgDef));
    Writer_ |= "return " + createCall + ";";
    Writer_.DecrementIdentLevel();
    Writer_ |= "}\n";
}

std::string TCppGenerator::TImpl::GenerateProtobufStructFill(const std::string& mutableCall,
                                                             const std::string& clearCall,
                                                             const NIR::TMessageDef::TFieldDef& fieldDef) {
    switch (fieldDef.Type->Type) {
        case EType::TYPE_STRING: {
            const std::string fieldName = GenerateFieldName(fieldDef);
            if (NIR::IsExplicitField(fieldDef)) {
                return "if (from.has_" + fieldName + "()) { const auto& _v = from." + fieldName + "(); " + mutableCall +
                       "->assign(_v.AsStringView()); } else { " + clearCall + "; }";
            }
            return "if (const auto& _v = from." + fieldName + "(); _v != " + GenerateDefaultValueExternal(fieldDef) +
                   ") { " + mutableCall + "->assign(_v.AsStringView()); } else { " + clearCall + "; }";
        }
        case EType::TYPE_VECTOR: {
            std::string fillVector =
                "if (const auto& _v = from." + GenerateFieldName(fieldDef) + "(); _v.Size() != 0) { ";
            if (fieldDef.Type->ElementType->Type == EType::TYPE_MESSAGE) {
                // Vector element can not be empty, so there is no need for additional check;
                const auto* elemMessage = fieldDef.Type->ElementType->MessageDef;
                if (NIR::IsAssociative(*fieldDef.Type)) {
                    fillVector += "for (uint32_t i = 0; i < _v.Size(); ++i) { " +
                                  GenerateMessageProtobufPair(*elemMessage) + " _p; _v.Get(i).TransformTo(_p); " +
                                  mutableCall + "->emplace(std::move(_p)); }";
                } else if (NIR::IsSequentialMessage(*fieldDef.Type)) {
                    fillVector += "::NYaFF::NExp::ColumnarTransformTo(_v, *" + mutableCall + ");";
                } else {
                    fillVector += mutableCall + "->Reserve(_v.Size()); ";
                    fillVector += "for (uint32_t i = 0; i < _v.Size(); ++i) { _v.Get(i).TransformTo(*" + mutableCall +
                                  "->Add()); }";
                }
            } else if (fieldDef.Type->ElementType->Type == EType::TYPE_STRING) {
                fillVector += mutableCall + "->Reserve(_v.Size()); ";
                fillVector += "for (uint32_t i = 0; i < _v.Size(); ++i) { const auto& _e = _v.Get(i); " + mutableCall +
                              "->Add()->assign(_e.AsStringView()); }";
            } else {
                const std::string setValue =
                    (fieldDef.Type->ElementType->Type == EType::TYPE_ENUM
                         ? "static_cast<" + GenerateEnumProtobufName(*fieldDef.Type->ElementType->EnumDef) +
                               ">(_v.Get(i))"
                         : "_v.Get(i)");
                fillVector += mutableCall + "->Reserve(_v.Size()); ";
                fillVector +=
                    "for (uint32_t i = 0; i < _v.Size(); ++i) { " + mutableCall + "->Add(" + setValue + "); }";
            }
            fillVector += " } else { " + clearCall + "; }";
            return fillVector;
        }
        case EType::TYPE_MESSAGE: {
            const std::string fieldName = GenerateFieldName(fieldDef);
            return "if (from.has_" + fieldName + "()) { from." + fieldName + "().TransformTo(*" + mutableCall +
                   "); } else { " + clearCall + "; }";
        }
        default:
            YAFF_THROW("non struct type");
    }
}

std::string TCppGenerator::TImpl::GenerateProtobufScalarFill(const std::string& mutableCall,
                                                             const std::string& clearCall,
                                                             const NIR::TMessageDef::TFieldDef& fieldDef) {
    YAFF_REQUIRE(NIR::IsScalar(fieldDef.Type->Type));
    const std::string fieldName = GenerateFieldName(fieldDef);
    const std::string checkCall =
        (NIR::IsExplicitField(fieldDef)
             ? "from.has_" + fieldName + "()"
             : "const auto _v = from." + fieldName + "(); !::NYaFF::IsEqual<" + GenerateTypeExternal(*fieldDef.Type) +
                   ">(_v, " + GenerateDefaultValueExternal(fieldDef) + ")");
    const std::string getCall = (NIR::IsExplicitField(fieldDef) ? "from." + fieldName + "()" : "_v");
    const std::string setValue =
        (fieldDef.Type->Type == EType::TYPE_ENUM
             ? "static_cast<" + GenerateEnumProtobufName(*fieldDef.Type->EnumDef) + ">(" + getCall + ")"
             : getCall);
    return "if (" + checkCall + ") { " + mutableCall + ".set_" + fieldName + "(" + setValue + "); } else { " +
           clearCall + "; }";
}

std::string TCppGenerator::TImpl::GenerateProtobufPairFill(const NIR::TMessageDef::TFieldDef& fieldDef) {
    const std::string getCall = "from." + GenerateFieldName(fieldDef) + "()";
    const std::string pairName = "proto." + GenerateExternalPairName(fieldDef);
    const std::string mutableCall =
        (fieldDef.Id == 1 ? "const_cast<" + GenerateTypeProtobuf(*fieldDef.Type) + "&>(" + pairName + ")" : pairName);
    if (NIR::IsScalar(fieldDef.Type->Type)) {
        const std::string setValue =
            (fieldDef.Type->Type == EType::TYPE_ENUM
                 ? "static_cast<" + GenerateEnumProtobufName(*fieldDef.Type->EnumDef) + ">(" + getCall + ")"
                 : getCall);
        return mutableCall + " = " + setValue + ";";
    }
    const std::string clearFunc = (fieldDef.Type->Type == EType::TYPE_STRING ? "clear" : "Clear");
    return GenerateProtobufStructFill("(&" + mutableCall + ")", "(&" + mutableCall + ")->" + clearFunc + "()",
                                      fieldDef);
}

std::string TCppGenerator::TImpl::GenerateProtobufValue(const NIR::TMessageDef::TFieldDef& fieldDef) {
    const std::string getCall = "proto." + GenerateFieldName(fieldDef) + "()";
    const std::string createCall = GenerateExternalCreate(getCall, "", *fieldDef.Type);
    if (fieldDef.Type->Type == EType::TYPE_STRING) {
        const std::string checkCall =
            (NIR::IsExplicitField(fieldDef) ? "proto.has_" + GenerateFieldName(fieldDef) + "()"
                                            : getCall + " != " + GenerateDefaultValueExternal(fieldDef));
        return "(" + checkCall + " ? " + createCall + " : 0)";
    }
    if (fieldDef.Type->Type == EType::TYPE_VECTOR) {
        return "(!" + getCall + ".empty() ? " + createCall + " : 0)";
    }
    if (fieldDef.Type->Type == EType::TYPE_MESSAGE) {
        return "(proto.has_" + GenerateFieldName(fieldDef) + "() ? " + createCall + " : 0)";
    }
    return (NIR::IsExplicitField(fieldDef)
                ? "(proto.has_" + GenerateFieldName(fieldDef) + "() ? std::optional<" +
                      GenerateTypeExternal(*fieldDef.Type) + ">{" + createCall + "} : std::nullopt)"
                : createCall);
}

std::string TCppGenerator::TImpl::GenerateProtobufPairValue(const NIR::TMessageDef::TFieldDef& fieldDef) {
    return GenerateExternalCreate("proto." + GenerateExternalPairName(fieldDef), "", *fieldDef.Type);
}

std::string TCppGenerator::TImpl::GenerateExternalCreate(const std::string& getCall, const std::string& getPrefix,
                                                         const NIR::TType& type) {
    switch (type.Type) {
        case EType::TYPE_ENUM:
            return GenerateTypeCastExternal(type, getCall);
        case EType::TYPE_STRING:
            return "yffb.CreateString(" + getCall + ")";
        case EType::TYPE_VECTOR: {
            const std::string elemType = GenerateTypeExternal(*type.ElementType);
            if (NIR::IsAssociative(type)) {
                const std::string getElemCall = "*" + getCall + ".find(_k)";
                const std::string createPairCall = GenerateExternalCreate(getElemCall, "", *type.ElementType);
                return "yffb.CreateVector<" + elemType + ">(::NYaFF::SortedKeys(" + getCall +
                       "), [&] (const auto& _k) { return " + createPairCall + "; })";
            }
            if (NIR::IsSequentialMessage(type)) {
                return "::NYaFF::NExp::ColumnarCreateFrom<" +
                       GenerateWithNamespaceName(
                           type.ElementType->MessageDef->Schema->Namespace,
                           GenerateMessageColumnName(*type.ElementType->MessageDef) + "<::NYaFF::NExp::TSizeable>") +
                       ">(yffb, " + getCall + ")";
            }
            if (!NIR::IsScalar(type.ElementType->Type)) {
                // checkCall is empty because vector element can not be uninitialized;
                const std::string getElemCall = getCall + "[i]";
                const std::string createElemCall = GenerateExternalCreate(getElemCall, getPrefix, *type.ElementType);
                return "yffb.CreateVector<" + elemType + ">(" + getCall + ".size(), [&] (size_t i) { return " +
                       createElemCall + "; })";
            }
            // Always reinterpret_cast for easy support of enum scalars;
            return "yffb.CreateVector<" + elemType + ">(reinterpret_cast<const " + elemType + "*>(" + getCall +
                   ".data()), " + getCall + ".size())";
        }
        case EType::TYPE_MESSAGE: {
            const std::string createFunc = (NIR::IsInline(type) ? GenerateDeferredCreateFuncName(*type.MessageDef)
                                                                : GenerateCreateFuncName(*type.MessageDef));
            return GenerateWithNamespaceName(type.MessageDef->Schema->Namespace, createFunc) + "(yffb, " + getPrefix +
                   getCall + ")";
        }
        default:
            return getCall;
    }
}

std::string TCppGenerator::TImpl::GenerateExternalPairName(const NIR::TMessageDef::TFieldDef& fieldDef) {
    switch (fieldDef.Id) {
        case 1:
            return "first";
        case 2:
            return "second";
        default:
            YAFF_THROW("unknown pair number");
    }
}

std::string TCppGenerator::TImpl::GenerateTransformToProtobufFuncDeclaration(const NIR::TMessageDef& msgDef) {
    const bool nonEmpty = HasRealFields(msgDef);
    const std::string fromArgName = (nonEmpty ? " from" : "");
    const std::string protoArgName = (nonEmpty ? " proto" : "");
    return "template <typename F> inline void " + GenerateTransformFuncName(msgDef) + "(const F&" + fromArgName + ", " +
           GenerateMessageProtobufType(msgDef) + "&" + protoArgName + ")";
}

std::string TCppGenerator::TImpl::GenerateDeferredCreateFromProtobufFuncDeclaration(const NIR::TMessageDef& msgDef) {
    const std::string protoArgName = (HasRealFields(msgDef) ? " proto" : "");
    return "inline auto " + GenerateDeferredCreateFuncName(msgDef) + "(::NYaFF::TBuilder& yffb, const " +
           GenerateMessageProtobufType(msgDef) + "&" + protoArgName + ")";
}

std::string TCppGenerator::TImpl::GenerateCreateFromProtobufFuncDeclaration(const NIR::TMessageDef& msgDef) {
    const std::string protoArgName = (HasRealFields(msgDef) ? " proto" : "");
    return "inline ::NYaFF::TInternalOffset<" + msgDef.Name + "> " + GenerateCreateFuncName(msgDef) +
           "(::NYaFF::TBuilder& yffb, const " + GenerateMessageProtobufType(msgDef) + "&" + protoArgName + ")";
}

std::string TCppGenerator::TImpl::GenerateDeferredCreateFuncName(const NIR::TMessageDef& msgDef) {
    return "Defer" + GenerateCreateFuncName(msgDef);
}

std::string TCppGenerator::TImpl::GenerateCreateFuncName(const NIR::TMessageDef& msgDef) {
    return "Create" + msgDef.Name;
}

std::string TCppGenerator::TImpl::GenerateTransformFuncName(const NIR::TMessageDef& msgDef) {
    return "Transform" + msgDef.Name;
}

std::string TCppGenerator::TImpl::GenerateEnumDescriptorFuncDeclaration(const std::string& name) {
    return "inline const ::NYaFF::NReflect::TEnumDescriptor* " + GenerateDescriptorFuncName(name) + "()";
}

std::string TCppGenerator::TImpl::GenerateMessageDescriptorFuncDeclaration(const std::string& name) {
    return "inline const ::NYaFF::NReflect::TMessageDescriptor* " + GenerateDescriptorFuncName(name) + "()";
}

std::string TCppGenerator::TImpl::GenerateDescriptorFuncName(const std::string& name) {
    return name + "Descriptor";
}

std::string TCppGenerator::TImpl::GenerateDefaultDescriptor(const NIR::TType& type) {
    const std::string* defaultPtr = FindKey(type.Modifiers, NIR::DEFAULT_MODIFIER_NAME);
    if (defaultPtr == nullptr) {
        return "{}";
    }
    switch (type.Type) {
        case EType::TYPE_BOOL:
            return "{.Bool = " + *defaultPtr + "}";
        case EType::TYPE_INT32:
            return "{.Int32 = " + *defaultPtr + "}";
        case EType::TYPE_UINT32:
            return "{.Uint32 = " + *defaultPtr + "}";
        case EType::TYPE_INT64:
            return "{.Int64 = " + *defaultPtr + "}";
        case EType::TYPE_UINT64:
            return "{.Uint64 = " + *defaultPtr + "}";
        case EType::TYPE_FLOAT:
            return "{.Float = " + *defaultPtr + "}";
        case EType::TYPE_DOUBLE:
            return "{.Double = " + *defaultPtr + "}";
        case EType::TYPE_STRING:
            return "{.String = \"" + *defaultPtr + "\"}";
        case EType::TYPE_ENUM: {
            const std::string value = GenerateTypeCastInternal(type, GenerateTypeExternal(type) + "::" + *defaultPtr);
            return "{.Int32 = " + value + "}";
        }
        default:
            YAFF_THROW("unknown default value type");
    }
}

std::string TCppGenerator::TImpl::GenerateMessageLayoutDescriptor(const EMessageLayout layout) {
    switch (layout) {
        case EMessageLayout::MESSAGE_LAYOUT_UNKNOWN:
            return "::NYaFF::EMessageLayout::MESSAGE_LAYOUT_UNKNOWN";
        case EMessageLayout::MESSAGE_LAYOUT_FIXED:
            return "::NYaFF::EMessageLayout::MESSAGE_LAYOUT_FIXED";
        case EMessageLayout::MESSAGE_LAYOUT_FLAT:
            return "::NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT";
        case EMessageLayout::MESSAGE_LAYOUT_SPARSE:
            return "::NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE";
        case EMessageLayout::MESSAGE_LAYOUT_DYNAMIC:
            return "::NYaFF::EMessageLayout::MESSAGE_LAYOUT_DYNAMIC";
        default:
            YAFF_THROW("unknown message layout");
    }
}

std::string TCppGenerator::TImpl::GeneratePresenceDescriptor(const EPresence presence) {
    switch (presence) {
        case EPresence::PRESENCE_NONE:
            return "::NYaFF::EPresence::PRESENCE_NONE";
        case EPresence::PRESENCE_IMPLICIT:
            return "::NYaFF::EPresence::PRESENCE_IMPLICIT";
        case EPresence::PRESENCE_EXPLICIT:
            return "::NYaFF::EPresence::PRESENCE_EXPLICIT";
        default:
            YAFF_THROW("unknown message layout");
    }
}

std::string TCppGenerator::TImpl::GenerateBasicTypeDescriptor(const EType type) {
    switch (type) {
        case EType::TYPE_NONE:
            return "::NYaFF::EType::TYPE_NONE";
        case EType::TYPE_BOOL:
            return "::NYaFF::EType::TYPE_BOOL";
        case EType::TYPE_INT32:
            return "::NYaFF::EType::TYPE_INT32";
        case EType::TYPE_UINT32:
            return "::NYaFF::EType::TYPE_UINT32";
        case EType::TYPE_INT64:
            return "::NYaFF::EType::TYPE_INT64";
        case EType::TYPE_UINT64:
            return "::NYaFF::EType::TYPE_UINT64";
        case EType::TYPE_FLOAT:
            return "::NYaFF::EType::TYPE_FLOAT";
        case EType::TYPE_DOUBLE:
            return "::NYaFF::EType::TYPE_DOUBLE";
        case EType::TYPE_ENUM:
            return "::NYaFF::EType::TYPE_ENUM";
        case EType::TYPE_STRING:
            return "::NYaFF::EType::TYPE_STRING";
        case EType::TYPE_VECTOR:
            return "::NYaFF::EType::TYPE_VECTOR";
        case EType::TYPE_MESSAGE:
            return "::NYaFF::EType::TYPE_MESSAGE";
        default:
            YAFF_THROW("unknown type");
    }
}

std::string TCppGenerator::TImpl::GenerateCompositeTypeDescriptor(const NIR::TType& type, const TTypeIndex& index) {
    switch (type.Type) {
        case EType::TYPE_ENUM: {
            if (!type.EnumDef) {
                return "{.Enum = nullptr}";
            }
            const std::string name = GenerateWithNamespaceName(type.EnumDef->Schema->Namespace, type.EnumDef->Name);
            return "{.Enum = " + GenerateDescriptorFuncName(name) + "()}";
        }
        case EType::TYPE_VECTOR: {
            if (!type.ElementType) {
                return "{.Element = nullptr}";
            }
            return "{.Element = &types[" + std::to_string(index.At(type.ElementType)) + "]}";
        }
        case EType::TYPE_MESSAGE: {
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

std::string TCppGenerator::TImpl::GenerateMessageDefaultFuncDeclaration(const std::string& ret,
                                                                        const std::string& name) {
    return "inline const " + ret + "& " + GenerateDefaultFuncName(name) + "()";
}

std::string TCppGenerator::TImpl::GenerateDefaultFuncName(const std::string& name) {
    return name + "Default";
}

std::string TCppGenerator::TImpl::GenerateTypeCastExternal(const NIR::TType& type, const std::string& call) {
    switch (type.Type) {
        case EType::TYPE_ENUM:
            return "static_cast<" + GenerateTypeExternal(type) + ">(" + call + ")";
        default:
            return call;
    }
}

std::string TCppGenerator::TImpl::GenerateTypeCastInternal(const NIR::TType& type, const std::string& call) {
    switch (type.Type) {
        case EType::TYPE_ENUM:
            return "static_cast<" + GenerateTypeInternal(type) + ">(" + call + ")";
        default:
            return call;
    }
}

std::string TCppGenerator::TImpl::GenerateDefaultValueExternal(const NIR::TMessageDef::TFieldDef& fieldDef) {
    switch (fieldDef.Type->Type) {
        case EType::TYPE_ENUM: {
            const std::string externalType = GenerateTypeExternal(*fieldDef.Type);
            const std::string* defaultPtr = FindKey(fieldDef.Type->Modifiers, NIR::DEFAULT_MODIFIER_NAME);
            return (defaultPtr ? (externalType + "::" + *defaultPtr) : GenerateTypeCastExternal(*fieldDef.Type, "0"));
        }
        case EType::TYPE_STRING: {
            const std::string* defaultPtr = FindKey(fieldDef.Type->Modifiers, NIR::DEFAULT_MODIFIER_NAME);
            return (defaultPtr ? "\"" + *defaultPtr + "\"" : "\"\"");
        }
        default: {
            return GenerateDefaultValueInternal(fieldDef);
        }
    }
}

std::string TCppGenerator::TImpl::GenerateDefaultValueInternal(const NIR::TMessageDef::TFieldDef& fieldDef) {
    switch (fieldDef.Type->Type) {
        case EType::TYPE_ENUM: {
            const std::string externalType = GenerateTypeExternal(*fieldDef.Type);
            const std::string* defaultPtr = FindKey(fieldDef.Type->Modifiers, NIR::DEFAULT_MODIFIER_NAME);
            const std::string defaultVal = defaultPtr ? (externalType + "::" + *defaultPtr) : "0";
            return GenerateTypeCastInternal(*fieldDef.Type, defaultVal);
        }
        case EType::TYPE_STRING: {
            if (fieldDef.Type->Modifiers.contains(NIR::DEFAULT_MODIFIER_NAME)) {
                return GenerateDefaultName(fieldDef);
            }
            return GenerateTypeInternal(*fieldDef.Type) + "::Default()";
        }
        case EType::TYPE_VECTOR:
            return (NIR::IsSequentialMessage(*fieldDef.Type)
                        ? GenerateWithNamespaceName(fieldDef.Type->ElementType->MessageDef->Schema->Namespace,
                                                    GenerateDefaultFuncName(GenerateMessageColumnName(
                                                        *fieldDef.Type->ElementType->MessageDef))) +
                              "()"
                        : GenerateTypeInternal(*fieldDef.Type) + "::Default()");
        case EType::TYPE_MESSAGE:
            return GenerateWithNamespaceName(fieldDef.Type->MessageDef->Schema->Namespace,
                                             GenerateDefaultFuncName(fieldDef.Type->MessageDef->Name)) +
                   "()";
        default: {
            const std::string* defaultPtr = FindKey(fieldDef.Type->Modifiers, NIR::DEFAULT_MODIFIER_NAME);
            return (defaultPtr ? *defaultPtr : GenerateBasicDefault(fieldDef.Type->Type));
        }
    }
}

std::string TCppGenerator::TImpl::GenerateBasicDefault(const EType type) {
    switch (type) {
        case EType::TYPE_BOOL:
            return "false";
        case EType::TYPE_INT32:
        case EType::TYPE_UINT32:
        case EType::TYPE_INT64:
        case EType::TYPE_UINT64:
            return "0";
        case EType::TYPE_FLOAT:
        case EType::TYPE_DOUBLE:
            return "0.";
        default:
            YAFF_THROW("non-basic type");
    }
}

std::string TCppGenerator::TImpl::GenerateTypeProtobuf(const NIR::TType& type) {
    switch (type.Type) {
        case EType::TYPE_ENUM:
            return GenerateEnumProtobufName(*type.EnumDef);
        case EType::TYPE_STRING:
            return "std::string";
        case EType::TYPE_VECTOR:
            return (NIR::IsAssociative(type)
                        ? GenerateMessageProtobufMap(*type.ElementType->MessageDef)
                        : "::google::protobuf::RepeatedPtrField<" + GenerateTypeProtobuf(*type.ElementType) + ">");
        case EType::TYPE_MESSAGE:
            return GenerateMessageProtobufType(*type.MessageDef);
        default:
            return GenerateTypeExternal(type);
    }
}

std::string TCppGenerator::TImpl::GenerateTypeExternal(const NIR::TType& type) {
    switch (type.Type) {
        case EType::TYPE_ENUM:
            return GenerateWithNamespaceName(type.EnumDef->Schema->Namespace, type.EnumDef->Name);
        case EType::TYPE_STRING:
        case EType::TYPE_VECTOR:
        case EType::TYPE_MESSAGE: {
            const std::string offsetType =
                (NIR::IsInline(type) ? "::NYaFF::TInlineOffset" : "::NYaFF::TInternalOffset");
            return offsetType + "<" + GenerateTypeInternal(type) + ">";
        }
        default:
            return GenerateBasicType(type.Type);
    }
}

std::string TCppGenerator::TImpl::GenerateTypeInternal(const NIR::TType& type) {
    switch (type.Type) {
        case EType::TYPE_ENUM:
            return "int32_t";
        case EType::TYPE_STRING:
            return "::NYaFF::TString";
        case EType::TYPE_VECTOR:
            return NIR::IsSequentialMessage(type)
                       ? GenerateWithNamespaceName(
                             type.ElementType->MessageDef->Schema->Namespace,
                             GenerateMessageColumnName(*type.ElementType->MessageDef) + "<::NYaFF::NExp::TSizeable>")
                       : "::NYaFF::TVector<" + GenerateTypeExternal(*type.ElementType) + ">";
        case EType::TYPE_MESSAGE:
            return GenerateWithNamespaceName(type.MessageDef->Schema->Namespace, type.MessageDef->Name);
        default:
            return GenerateBasicType(type.Type);
    }
}

std::string TCppGenerator::TImpl::GenerateBasicType(const EType type) {
    switch (type) {
        case EType::TYPE_BOOL:
            return "bool";
        case EType::TYPE_INT32:
            return "int32_t";
        case EType::TYPE_UINT32:
            return "uint32_t";
        case EType::TYPE_INT64:
            return "int64_t";
        case EType::TYPE_UINT64:
            return "uint64_t";
        case EType::TYPE_FLOAT:
            return "float";
        case EType::TYPE_DOUBLE:
            return "double";
        default:
            YAFF_THROW("non-basic type");
    }
}

std::string TCppGenerator::TImpl::GenerateMessageType(const EMessageType type) {
    switch (type) {
        case EMessageType::FIXED:
            return "Fixed";
        case EMessageType::FLAT:
            return "Flat";
        case EMessageType::SPARSE:
            return "Sparse";
    }
    YAFF_THROW("unknown message type");
}

std::string TCppGenerator::TImpl::GenerateMessageBaseClass(const NIR::TMessageDef& msgDef) {
    switch (msgDef.Layout) {
        case EMessageLayout::MESSAGE_LAYOUT_FIXED:
            return "::NYaFF::TFixedMessage<" + GenerateMessageStaticMetaName(msgDef) + ">";
        case EMessageLayout::MESSAGE_LAYOUT_FLAT:
            return "::NYaFF::TFlatMessage<" + GenerateMessageStaticMetaName(msgDef) + ">";
        case EMessageLayout::MESSAGE_LAYOUT_SPARSE:
            return "::NYaFF::TSparseMessage";
        case EMessageLayout::MESSAGE_LAYOUT_DYNAMIC: {
            const std::string templateArgs =
                (!NIR::IsGapMessage(msgDef) ? GenerateMessageStaticMetaName(msgDef) : "void");
            return "::NYaFF::TDynamicMessage<" + templateArgs + ">";
        }
        default:
            YAFF_THROW("unknown message layout");
    }
}

std::string TCppGenerator::TImpl::GenerateFieldName(const NIR::TMessageDef::TFieldDef& fieldDef) {
    return GenerateEscapedName(ToLowerString(fieldDef.Name));
}

std::string TCppGenerator::TImpl::GenerateEscapedName(const std::string& input) {
    static const std::unordered_set<std::string_view> keywords{
        "Default", "CreateFrom", "DeferCreateFrom", "TransformTo", "TStructType", "TProtobufType", "Descriptor"};
    return GetCppKeywords().contains(input) || keywords.contains(input) ? input + '_' : input;
}

std::string TCppGenerator::TImpl::GenerateStringDefault(const NIR::TType& type) {
    const auto bytes = NIR::NReflect::GenerateStringDefault(type);
    std::string arrayLiteral = "(uint8_t[]){";
    for (const uint8_t x : bytes) {
        arrayLiteral += "0x" + ToHexCode(x) + ", ";
    }
    arrayLiteral += "}";
    return arrayLiteral;
}

std::string TCppGenerator::TImpl::GenerateDefaultName(const NIR::TMessageDef::TFieldDef& fieldDef) {
    std::string defaultName = fieldDef.Name + "_DEFAULT";
    std::transform(defaultName.cbegin(), defaultName.cend(), defaultName.begin(), toupper);
    return defaultName;
}

std::string TCppGenerator::TImpl::GenerateIdName(const NIR::TMessageDef::TFieldDef& fieldDef) {
    std::string idName = "ID_" + (!fieldDef.Deprecated ? fieldDef.Name : "DEPRECATED" + std::to_string(fieldDef.Id));
    std::transform(idName.cbegin(), idName.cend(), idName.begin(), toupper);
    return idName;
}

std::string TCppGenerator::TImpl::GenerateSharedOffsetCaseEnumName(const std::string& name) {
    return ToCamelCase(name, true) + "Case";
}

std::string TCppGenerator::TImpl::GenerateSharedOffsetDefaultCaseEnumName(const std::string& name) {
    std::string defaultName = name + "_not_set";
    std::transform(defaultName.cbegin(), defaultName.cend(), defaultName.begin(), toupper);
    return defaultName;
}

std::string TCppGenerator::TImpl::GenerateSharedOffsetFieldName(const std::string& name,
                                                                const NIR::TMessageDef::TFieldDef* fieldDef) {
    YAFF_REQUIRE(fieldDef && fieldDef->SharedOffsetVia == name);
    std::string fieldName = "k" + ToCamelCase(fieldDef->Name, true);
    return fieldName;
}

std::string TCppGenerator::TImpl::GenerateGetterReturnType(const NIR::TType& type) {
    // Return type is not real external type, because vectors and nested structres are returned as reference (not
    // WrittenOffset);
    return (!NIR::IsScalar(type.Type) ? "const " + GenerateTypeInternal(type) + "&" : GenerateTypeExternal(type));
}

std::string TCppGenerator::TImpl::GenerateMessageCreateFuncArgument(const NIR::TMessageDef::TFieldDef& fieldDef) {
    const bool isScalar = NIR::IsScalar(fieldDef.Type->Type);
    const bool isOptional = NIR::IsExplicitField(fieldDef) && isScalar;
    const std::string fieldType = GenerateTypeExternal(*fieldDef.Type);
    const std::string fieldDefault = (isScalar ? " = " + GenerateDefaultValueExternal(fieldDef) : " = 0");
    const std::string argName = "a_" + GenerateFieldName(fieldDef);
    const std::string argType = (isOptional ? "std::optional<" + fieldType + ">" : fieldType);
    const std::string defaultSuffix = (isOptional ? " = std::nullopt" : fieldDefault);
    return argType + " " + argName + defaultSuffix;
}

std::string TCppGenerator::TImpl::GenerateMessageBasicCreateFuncDefault(const NIR::TMessageDef& msgDef) {
    switch (msgDef.Layout) {
        case EMessageLayout::MESSAGE_LAYOUT_FIXED:
            return GenerateMessageBuilderName(EMessageType::FIXED, msgDef);
        case EMessageLayout::MESSAGE_LAYOUT_FLAT:
            return GenerateMessageBuilderName(EMessageType::FLAT, msgDef);
        case EMessageLayout::MESSAGE_LAYOUT_SPARSE:
            return GenerateMessageBuilderName(EMessageType::SPARSE, msgDef);
        case EMessageLayout::MESSAGE_LAYOUT_DYNAMIC: {
            const EMessageType type = (NIR::IsGapMessage(msgDef) ? EMessageType::SPARSE : EMessageType::FLAT);
            return GenerateMessageBuilderName(type, msgDef);
        }
        default:
            YAFF_THROW("unknown message layout");
    }
}

std::string TCppGenerator::TImpl::GenerateMessageBasicCreateFuncCall(const NIR::TMessageDef& msgDef,
                                                                     const std::string& templateArg) {
    std::string result =
        GenerateCreateFuncName(msgDef) + (!templateArg.empty() ? "<" + templateArg + ">" : "") + "(yffb";
    ForRealFields(msgDef, [&](const auto& fieldDef) { result += ", a_" + GenerateFieldName(fieldDef); });
    result += ")";
    return result;
}

std::string TCppGenerator::TImpl::GenerateMessageBasicCreateFuncDeferredCall(const NIR::TMessageDef& msgDef,
                                                                             const std::string& templateArg) {
    std::string result = "[=, &yffb]() { return " + GenerateCreateFuncName(msgDef) +
                         (!templateArg.empty() ? "<" + templateArg + ">" : "") + "(yffb";
    ForRealFields(msgDef, [&](const auto& fieldDef) { result += ", a_" + GenerateFieldName(fieldDef); });
    result += "); }";
    return result;
}

std::string TCppGenerator::TImpl::GenerateMessageProtobufType(const NIR::TMessageDef& msgDef) {
    return NIR::IsAssociativePair(msgDef) ? GenerateMessageProtobufPair(msgDef) : GenerateMessageProtobufName(msgDef);
}

std::string TCppGenerator::TImpl::GenerateMessageProtobufPair(const NIR::TMessageDef& msgDef) {
    YAFF_REQUIRE(NIR::IsAssociativePair(msgDef));
    return "std::pair<const " + GenerateTypeProtobuf(*msgDef.Fields.at(0).Type) + ", " +
           GenerateTypeProtobuf(*msgDef.Fields.at(1).Type) + ">";
}

std::string TCppGenerator::TImpl::GenerateMessageProtobufMap(const NIR::TMessageDef& msgDef) {
    YAFF_REQUIRE(NIR::IsAssociativePair(msgDef));
    return "::google::protobuf::Map<" + GenerateTypeProtobuf(*msgDef.Fields.at(0).Type) + ", " +
           GenerateTypeProtobuf(*msgDef.Fields.at(1).Type) + ">";
}

std::string TCppGenerator::TImpl::GenerateEnumProtobufName(const NIR::TEnumDef& enumDef) {
    const std::string* protoNamespace = FindKey(enumDef.Schema->Attributes, NIR::PROTO_NAMESPACE_ATTRIBUTE_NAME);
    YAFF_REQUIRE(protoNamespace);
    return GenerateWithNamespaceName(*protoNamespace, enumDef.Name);
}

std::string TCppGenerator::TImpl::GenerateMessageProtobufName(const NIR::TMessageDef& msgDef) {
    const std::string* protoNamespace = FindKey(msgDef.Schema->Attributes, NIR::PROTO_NAMESPACE_ATTRIBUTE_NAME);
    YAFF_REQUIRE(protoNamespace);
    return GenerateWithNamespaceName(*protoNamespace, msgDef.Name);
}

std::string TCppGenerator::TImpl::GenerateMessageColumnName(const NIR::TMessageDef& msgDef) {
    return msgDef.Name + "Column";
}

std::string TCppGenerator::TImpl::GenerateMessageStaticMetaName(const NIR::TMessageDef& msgDef) {
    return msgDef.Name + "Meta";
}

std::string TCppGenerator::TImpl::GenerateMessageBuilderName(EMessageType msgType, const NIR::TMessageDef& msgDef) {
    return msgDef.Name + GenerateMessageType(msgType) + "Builder";
}

std::string TCppGenerator::TImpl::GenerateWithNamespaceName(const std::string& ns, const std::string& name) {
    return ns + "::" + name;
}

TCppGenerator::TCppGenerator(std::ostream& out, TCppGenerationOptions opts)
    : Impl_(std::make_unique<TImpl>(out, std::move(opts))) {
}

TCppGenerator::~TCppGenerator() {
}

void TCppGenerator::Generate(const NIR::TIR& ir) {
    Impl_->Generate(ir);
}

}  // namespace NYaFF::NCompile
