#pragma once

#include <google/protobuf/descriptor.h>

#include "compiler.h"
#include "ir.h"

namespace NYaFF::NCompile {

struct TProtobufReflectionFrontEndOptions {
    std::unordered_set<std::string> TargetFiles;

    std::string RootNamespace = "NProtoYaFF";
    std::string GeneratedProtobufExt = ".pb.h";

    bool SkipEmptyMessages = true;
    bool FillGaps = false;
};

struct TProtobufDeprecatedField {
    uint64_t Id = 0;
    google::protobuf::FieldDescriptor::Type Type = static_cast<google::protobuf::FieldDescriptor::Type>(0);
    google::protobuf::FieldDescriptor::Label Label = static_cast<google::protobuf::FieldDescriptor::Label>(0);
};

struct TProtobufField {
    struct TAdditionalModifiers {
        std::map<std::string, std::string> Modifiers;
        std::map<std::string, std::string> ElementModifiers;
    };

    uint64_t Id = 0;
    TAdditionalModifiers AdditionalModifiers;
};

struct TProtobufMessage {
    NYaFF::EMessageLayout Layout = NYaFF::EMessageLayout::MESSAGE_LAYOUT_UNKNOWN;
    std::vector<TProtobufDeprecatedField> DeprecatedFields;
};

class IProtobufReflectionTraits {
public:
    virtual ~IProtobufReflectionTraits() = default;

    // Return std::nullopt to skip field;
    virtual std::optional<TProtobufField> GetYaffFields(const google::protobuf::FieldDescriptor&) = 0;

    // Return std::nullopt to skip message;
    virtual std::optional<TProtobufMessage> GetYaffMessage(const google::protobuf::Descriptor&) = 0;
};

class TProtobufReflectionDefaultTraits : public IProtobufReflectionTraits {
public:
    std::optional<TProtobufField> GetYaffFields(const google::protobuf::FieldDescriptor&) override;
    std::optional<TProtobufMessage> GetYaffMessage(const google::protobuf::Descriptor&) override;
};

class TProtobufReflectionTaggedTraits : public IProtobufReflectionTraits {
public:
    explicit TProtobufReflectionTaggedTraits(const std::string& sliceId);

    std::optional<TProtobufField> GetYaffFields(const google::protobuf::FieldDescriptor&) override;
    std::optional<TProtobufMessage> GetYaffMessage(const google::protobuf::Descriptor&) override;

private:
    std::string SliceId_;
};

class TProtobufReflectionFrontEnd : public IFrontEnd {
public:
    TProtobufReflectionFrontEnd(
        const google::protobuf::FileDescriptor* fileDescriptor,
        std::unique_ptr<IProtobufReflectionTraits> traits = std::make_unique<TProtobufReflectionDefaultTraits>(),
        TProtobufReflectionFrontEndOptions opts = {});

    TProtobufReflectionFrontEnd(
        const std::vector<const google::protobuf::Descriptor*>& descriptors,
        std::unique_ptr<IProtobufReflectionTraits> traits = std::make_unique<TProtobufReflectionDefaultTraits>(),
        TProtobufReflectionFrontEndOptions opts = {});

    NIR::TIR Parse() override;

private:
    std::vector<const google::protobuf::Descriptor*> Descriptors_;
    const google::protobuf::FileDescriptor* FileDescriptor_;
    std::unique_ptr<IProtobufReflectionTraits> Traits_;
    TProtobufReflectionFrontEndOptions Opts_;
};

}  // namespace NYaFF::NCompile
