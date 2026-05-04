#pragma once

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>

namespace yaff::tool {

class ProtobufGeneratorPlugin : public ::google::protobuf::compiler::CodeGenerator {
public:
    bool Generate(const ::google::protobuf::FileDescriptor* file, const std::string& parameter,
                  ::google::protobuf::compiler::GeneratorContext* generator_context, std::string* error) const override;

    uint64_t GetSupportedFeatures() const override;
};

}  // namespace yaff::tool
