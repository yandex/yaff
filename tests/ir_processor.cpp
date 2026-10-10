#include <compilation/ir_processor.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace yaff::compilation {
namespace {

using ir::IR;

struct TestIR {
    IR Ir;

    ir::SchemaDef* AddSchema(bool defined = true) {
        auto [schema, inserted] = Ir.Schemas.TryEmplace(ir::SchemaDef("test.proto"));
        EXPECT_TRUE(inserted);
        schema->Namespace = "test";
        schema->Defined = defined;
        return schema;
    }

    ir::MessageDef* AddMessage(ir::SchemaDef* schema, MessageLayout layout = MessageLayout::MESSAGE_LAYOUT_DYNAMIC,
                               std::string name = "Message", bool defined = true) {
        auto [message, inserted] = Ir.Messages.TryEmplace(ir::MessageDef(std::move(name), schema, layout));
        EXPECT_TRUE(inserted);
        message->Defined = defined;
        if (schema) {
            schema->Messages.push_back(message);
        }
        return message;
    }

    ir::EnumDef* AddEnum(ir::SchemaDef* schema, bool defined = true) {
        auto [enm, inserted] = Ir.Enums.TryEmplace(ir::EnumDef("Enum", schema));
        EXPECT_TRUE(inserted);
        enm->Defined = defined;
        if (schema) {
            schema->Enums.push_back(enm);
        }
        return enm;
    }

    const ir::TypeDef* AddType(Type type, const ir::TypeDef* element = nullptr,
                               const ir::MessageDef* message = nullptr) {
        return Ir.Types.GetOrEmplace(ir::TypeDef{.Type = type, .ElementType = element, .MessageDef = message});
    }

    ir::MessageDef::FieldDef& AddField(ir::MessageDef* message, const ir::TypeDef* type, uint64_t id = 1,
                                       std::string name = "field") {
        return message->Fields.emplace_back(ir::MessageDef::FieldDef{
            .Id = id, .Name = std::move(name), .Type = type, .Presence = Presence::PRESENCE_IMPLICIT});
    }

    void ExpectProcessError(const std::string_view expected) {
        try {
            ProcessIR(Ir);
            FAIL() << "ProcessIR did not throw";
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(error.what(), expected);
        }
    }
};

TEST(IRProcessorErrorHandling, EnumMustHaveSchema) {
    TestIR t;
    auto* enm = t.AddEnum(t.AddSchema());
    enm->Schema = nullptr;
    t.ExpectProcessError("enum can not be not connected to schema 'Enum'");
}

TEST(IRProcessorErrorHandling, EnumDefinitionMustMatchSchema) {
    TestIR t;
    auto* schema = t.AddSchema();
    t.AddEnum(schema, false);
    t.AddMessage(schema);
    t.ExpectProcessError("incomplete definition of enum 'Enum' in defined schema 'test'");
}

TEST(IRProcessorErrorHandling, MessageMustHaveSchema) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    message->Schema = nullptr;
    t.ExpectProcessError("message can not be not connected to schema 'Message'");
}

TEST(IRProcessorErrorHandling, MessageDefinitionMustMatchSchema) {
    TestIR t;
    auto* schema = t.AddSchema(false);
    t.AddMessage(schema, MessageLayout::MESSAGE_LAYOUT_DYNAMIC, "Message", true);
    schema->Messages.clear();
    t.ExpectProcessError("incomplete definition of message 'Message'");
}

TEST(IRProcessorErrorHandling, FixedAndFlatMessagesMustNotHaveGaps) {
    for (const auto layout : {MessageLayout::MESSAGE_LAYOUT_FIXED, MessageLayout::MESSAGE_LAYOUT_FLAT}) {
        TestIR t;
        auto* message = t.AddMessage(t.AddSchema(), layout);
        t.AddField(message, t.AddType(Type::TYPE_INT32), 2);
        t.ExpectProcessError("message 'Message' can not contain any gaps because it has field size based layout");
    }
}

TEST(IRProcessorErrorHandling, ActiveFieldMustHaveName) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    t.AddField(message, t.AddType(Type::TYPE_INT32), 1, "");
    t.ExpectProcessError("field with id '1' has empty name");
}

TEST(IRProcessorErrorHandling, DefaultMustBeScalarOrString) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    auto type = ir::TypeDef{.Type = Type::TYPE_MESSAGE};
    type.Modifiers.emplace(ir::DEFAULT_MODIFIER_NAME, "value");
    t.AddField(message, t.Ir.Types.GetOrEmplace(std::move(type)));
    t.ExpectProcessError("field 'field' (id: 1) not allowed to have default value");
}

TEST(IRProcessorErrorHandling, InlineFieldIsRejected) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    auto type = ir::TypeDef{.Type = Type::TYPE_INT32};
    type.Modifiers.emplace(ir::INLINE_MODIFIER_NAME, "");
    t.AddField(message, t.Ir.Types.GetOrEmplace(std::move(type)));
    t.ExpectProcessError("field 'field' (id: 1) is inlined, but inlined fields are not supported yet");
}

TEST(IRProcessorErrorHandling, TwoDimensionalArrayIsRejected) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    const auto* scalar = t.AddType(Type::TYPE_INT32);
    const auto* inner = t.AddType(Type::TYPE_ARRAY, scalar);
    t.AddField(message, t.AddType(Type::TYPE_ARRAY, inner));
    t.ExpectProcessError("field 'field' (id: 1) is 2d array, but 2d arrays are not supported");
}

TEST(IRProcessorErrorHandling, ArrayMustHaveImplicitPresence) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    auto& field = t.AddField(message, t.AddType(Type::TYPE_ARRAY, t.AddType(Type::TYPE_INT32)));
    field.Presence = Presence::PRESENCE_EXPLICIT;
    t.ExpectProcessError("field 'field' (id: 1) is array, but has explicit presence");
}

TEST(IRProcessorErrorHandling, MessageMustHaveExplicitPresence) {
    TestIR t;
    auto* schema = t.AddSchema();
    auto* child = t.AddMessage(schema, MessageLayout::MESSAGE_LAYOUT_DYNAMIC, "Child");
    auto* parent = t.AddMessage(schema);
    t.AddField(parent, t.AddType(Type::TYPE_MESSAGE, nullptr, child));
    t.ExpectProcessError("field 'field' (id: 1) is message, but has implicit presence");
}

TEST(IRProcessorErrorHandling, ActiveIndexMustInitiallyBeZero) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    auto& field = t.AddField(message, t.AddType(Type::TYPE_INT32));
    field.ActiveIndex = 42;
    t.ExpectProcessError("field 'field' (id: 1) has non-zero active index");
}

const ir::TypeDef* AddKeyType(TestIR& t, Type type) {
    auto key = ir::TypeDef{.Type = type};
    key.Modifiers.emplace(ir::KEY_MODIFIER_NAME, "");
    return t.Ir.Types.GetOrEmplace(std::move(key));
}

TEST(IRProcessorErrorHandling, DeprecatedFieldMustNotBeKey) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    auto& field = t.AddField(message, AddKeyType(t, Type::TYPE_INT32));
    field.Deprecated = true;
    t.ExpectProcessError("field (id: 1) is deprecated and can not be key");
}

TEST(IRProcessorErrorHandling, KeyMustBeScalarOrString) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    auto& field = t.AddField(message, AddKeyType(t, Type::TYPE_MESSAGE));
    field.Presence = Presence::PRESENCE_EXPLICIT;
    t.ExpectProcessError("field 'field' (id: 1) is not scalar or string and can not be key");
}

TEST(IRProcessorErrorHandling, MessageMustNotHaveMultipleKeys) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    const auto* key = AddKeyType(t, Type::TYPE_INT32);
    t.AddField(message, key, 1, "first");
    t.AddField(message, key, 2, "second");
    t.ExpectProcessError("message 'Message' contains multiple key fields");
}

TEST(IRProcessorErrorHandling, FixedMessageMustNotHaveOneof) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema(), MessageLayout::MESSAGE_LAYOUT_FIXED);
    auto& field = t.AddField(message, t.AddType(Type::TYPE_INT32));
    field.OneOf = "choice";
    t.ExpectProcessError("message 'Message' is fixed and can not have any oneof fields");
}

TEST(IRProcessorErrorHandling, ArrayMustNotBeOneof) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    auto& field = t.AddField(message, t.AddType(Type::TYPE_ARRAY, t.AddType(Type::TYPE_INT32)));
    field.OneOf = "choice";
    t.ExpectProcessError("field 'field' (id: 1) is array and can not be oneof field");
}

TEST(IRProcessorErrorHandling, OneofMustHaveExplicitPresence) {
    TestIR t;
    auto* message = t.AddMessage(t.AddSchema());
    auto& field = t.AddField(message, t.AddType(Type::TYPE_INT32));
    field.OneOf = "choice";
    t.ExpectProcessError("field 'field' (id: 1) is oneof field, but has implicit presence");
}

ir::MessageDef* AddAssociativeField(TestIR& t, ir::MessageDef* parent, MessageLayout childLayout,
                                    size_t childFieldCount = 2) {
    auto* schema = parent->Schema ? const_cast<ir::SchemaDef*>(parent->Schema) : nullptr;
    auto* child = t.AddMessage(schema, childLayout, "Pair");
    const auto* scalar = t.AddType(Type::TYPE_INT32);
    for (size_t i = 0; i < childFieldCount; ++i) {
        t.AddField(child, scalar, i + 1, "part" + std::to_string(i));
    }
    auto associative = ir::TypeDef{.Type = Type::TYPE_MESSAGE, .MessageDef = child};
    associative.Modifiers.emplace(ir::ASSOCIATIVE_MODIFIER_NAME, "");
    auto& field = t.AddField(parent, t.Ir.Types.GetOrEmplace(std::move(associative)));
    field.Presence = Presence::PRESENCE_EXPLICIT;
    return child;
}

TEST(IRProcessorErrorHandling, AssociativePairMustBeFixed) {
    TestIR t;
    auto* parent = t.AddMessage(t.AddSchema());
    AddAssociativeField(t, parent, MessageLayout::MESSAGE_LAYOUT_DYNAMIC);
    t.ExpectProcessError("message 'Pair' marked as associative pair, but is not fixed");
}

TEST(IRProcessorErrorHandling, AssociativePairMustHaveTwoFields) {
    TestIR t;
    auto* parent = t.AddMessage(t.AddSchema());
    AddAssociativeField(t, parent, MessageLayout::MESSAGE_LAYOUT_FIXED, 1);
    t.ExpectProcessError("message 'Pair' marked as associative pair, but contains not 2 fields");
}

TEST(IRProcessorErrorHandling, MapKeyMustNotBeDeprecated) {
    TestIR t;
    auto* parent = t.AddMessage(t.AddSchema());
    auto* pair = AddAssociativeField(t, parent, MessageLayout::MESSAGE_LAYOUT_FIXED);
    pair->Fields[0].Deprecated = true;
    t.ExpectProcessError("field (id: 1) is deprecated and can not be map key");
}

TEST(IRProcessorErrorHandling, MapKeyMustBeScalarOrString) {
    TestIR t;
    auto* parent = t.AddMessage(t.AddSchema());
    auto* pair = AddAssociativeField(t, parent, MessageLayout::MESSAGE_LAYOUT_FIXED);
    pair->Fields[0].Type = t.AddType(Type::TYPE_MESSAGE);
    pair->Fields[0].Presence = Presence::PRESENCE_EXPLICIT;
    t.ExpectProcessError("field 'part0' (id: 1) is not scalar or string and can not be map key");
}

}  // namespace
}  // namespace yaff::compilation
