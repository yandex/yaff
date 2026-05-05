#pragma once

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>

namespace NYaFF::NTool {

class TProtobufGeneratorPlugin : public ::google::protobuf::compiler::CodeGenerator {
public:
    bool Generate(const ::google::protobuf::FileDescriptor* file, const std::string& parameter,
                  ::google::protobuf::compiler::GeneratorContext* generator_context, std::string* error) const override;

    uint64_t GetSupportedFeatures() const override;
};

}  // namespace NYaFF::NTool
