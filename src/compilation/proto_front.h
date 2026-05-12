#pragma once

#include <google/protobuf/descriptor.h>

#include "compiler.h"
#include "ir.h"

namespace yaff::compilation {

struct ProtobufReflectionFrontEndOptions {
    std::unordered_set<std::string> TargetFiles;

    std::string RootNamespace = "protoyaff";
    std::string GeneratedProtobufExt = ".pb.h";

    bool SkipEmptyMessages = true;
    bool FillGaps = false;
};

struct ProtobufDeprecatedField {
    uint64_t Id = 0;
    google::protobuf::FieldDescriptor::Type Type = static_cast<google::protobuf::FieldDescriptor::Type>(0);
    google::protobuf::FieldDescriptor::Label Label = static_cast<google::protobuf::FieldDescriptor::Label>(0);
};

struct ProtobufField {
    struct AdditionalModifiers {
        std::map<std::string, std::string> Modifiers;
        std::map<std::string, std::string> ElementModifiers;
    };

    uint64_t Id = 0;
    AdditionalModifiers Modifiers;
};

struct ProtobufMessage {
    yaff::MessageLayout Layout = yaff::MessageLayout::MESSAGE_LAYOUT_UNKNOWN;
    std::vector<ProtobufDeprecatedField> DeprecatedFields;
};

class ProtobufReflectionTraits {
public:
    virtual ~ProtobufReflectionTraits() = default;

    // Return std::nullopt to skip field;
    virtual std::optional<ProtobufField> GetYaffFields(const google::protobuf::FieldDescriptor&) = 0;

    // Return std::nullopt to skip message;
    virtual std::optional<ProtobufMessage> GetYaffMessage(const google::protobuf::Descriptor&) = 0;
};

class DefaultProtobufReflectionTraits : public ProtobufReflectionTraits {
public:
    std::optional<ProtobufField> GetYaffFields(const google::protobuf::FieldDescriptor&) override;
    std::optional<ProtobufMessage> GetYaffMessage(const google::protobuf::Descriptor&) override;
};

class TaggedProtobufReflectionTraits : public ProtobufReflectionTraits {
public:
    explicit TaggedProtobufReflectionTraits(const std::string& sliceId);

    std::optional<ProtobufField> GetYaffFields(const google::protobuf::FieldDescriptor&) override;
    std::optional<ProtobufMessage> GetYaffMessage(const google::protobuf::Descriptor&) override;

private:
    std::string SliceId_;
};

class ProtobufReflectionFrontEnd : public AbstractFrontEnd {
public:
    ProtobufReflectionFrontEnd(
        const google::protobuf::FileDescriptor* fileDescriptor,
        std::unique_ptr<ProtobufReflectionTraits> traits = std::make_unique<DefaultProtobufReflectionTraits>(),
        ProtobufReflectionFrontEndOptions opts = {});

    ProtobufReflectionFrontEnd(
        const std::vector<const google::protobuf::Descriptor*>& descriptors,
        std::unique_ptr<ProtobufReflectionTraits> traits = std::make_unique<DefaultProtobufReflectionTraits>(),
        ProtobufReflectionFrontEndOptions opts = {});

    ir::IR Parse() override;

private:
    std::vector<const google::protobuf::Descriptor*> Descriptors_;
    const google::protobuf::FileDescriptor* FileDescriptor_;
    std::unique_ptr<ProtobufReflectionTraits> Traits_;
    ProtobufReflectionFrontEndOptions Opts_;
};

}  // namespace yaff::compilation
