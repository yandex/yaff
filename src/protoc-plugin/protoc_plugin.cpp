#include "protoc_plugin.h"

#include <compilation/cpp_gen.h>
#include <compilation/proto_front.h>
#include <google/protobuf/compiler/cpp/helpers.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor_database.h>

#include <string>
#include <string_view>

namespace yaff::tool {

namespace {

struct ProtobufPluginOptions {
    std::string Tag;
    std::string RootNamespace;
    std::unordered_set<std::string> FileWhitelist;
    std::unordered_set<std::string> ExpFilter;
};

ProtobufPluginOptions ParseProtobufPluginOptions(const std::string& parameter) {
    ProtobufPluginOptions opts;

    std::vector<std::pair<std::string, std::string>> options;
    google::protobuf::compiler::ParseGeneratorParameter(parameter, &options);
    for (const auto& p : options) {
        if (p.first == "tag") {
            opts.Tag = p.second;
        } else if (p.first == "namespace") {
            opts.RootNamespace = p.second;
        } else if (p.first == "file" && !p.second.empty()) {
            opts.FileWhitelist.emplace(p.second);
        } else if (p.first == "experimental" && !p.second.empty()) {
            opts.ExpFilter.emplace(p.second);
        }
    }

    return opts;
}

std::string_view StripPath(const std::string_view fileName) {
    return fileName.substr(fileName.rfind('/') + 1);
}

bool NeedToProcessFile(const ProtobufPluginOptions& opts, const google::protobuf::FileDescriptor* file) {
    if (opts.FileWhitelist.empty()) {
        return true;
    }
    const std::string path{StripPath(file->name())};
    return opts.FileWhitelist.contains(path);
}

std::string GetFileSuffix(const std::string& tag) {
    return (!tag.empty() ? "_" + tag : "");
}

std::string GetYaffFilePath(const google::protobuf::FileDescriptor* file, const std::string& tag,
                            const std::string& ext) {
    return google::protobuf::compiler::cpp::StripProto(file->name()) + GetFileSuffix(tag) + ".yaff." + ext;
}

static std::unique_ptr<yaff::compilation::ProtobufReflectionTraits> GetReflectionTraits(const std::string& tag) {
    if (tag.empty()) {
        return std::make_unique<yaff::compilation::DefaultProtobufReflectionTraits>();
    }
    return std::make_unique<yaff::compilation::TaggedProtobufReflectionTraits>(tag);
}

std::unique_ptr<yaff::compilation::AbstractFrontEnd> GetFrontEnd(const google::protobuf::FileDescriptor* descriptor,
                                                                 const ProtobufPluginOptions& options) {
    yaff::compilation::ProtobufReflectionFrontEndOptions protoReflectOpts;
    protoReflectOpts.SkipEmptyMessages = false;
    protoReflectOpts.TargetFiles.emplace(descriptor->name());
    if (!options.RootNamespace.empty()) {
        protoReflectOpts.RootNamespace = options.RootNamespace;
    }
    return std::make_unique<yaff::compilation::ProtobufReflectionFrontEnd>(descriptor, GetReflectionTraits(options.Tag),
                                                                           std::move(protoReflectOpts));
}

std::vector<std::unique_ptr<yaff::compilation::AbstractGenerator>> GetGenerators(std::ostream& output,
                                                                                 const ProtobufPluginOptions& options) {
    std::vector<std::unique_ptr<yaff::compilation::AbstractGenerator>> generators;

    yaff::compilation::CppGenerationOptions cppGenOpts;
    cppGenOpts.GenerateProtobufApi = true;
    cppGenOpts.GenerateReflectionApi = true;
    cppGenOpts.IncludesSuffix = GetFileSuffix(options.Tag);
    generators.emplace_back(std::make_unique<yaff::compilation::CppGenerator>(output, std::move(cppGenOpts)));

    return generators;
}

void WriteFile(::google::protobuf::io::ZeroCopyOutputStream* stream, std::string_view content) {
    ::google::protobuf::io::Printer printer(stream);
    printer.WriteRaw(content.data(), content.size());
}

}  // namespace

bool ProtobufGeneratorPlugin::Generate(const ::google::protobuf::FileDescriptor* file, const std::string& parameter,
                                       ::google::protobuf::compiler::GeneratorContext* generator_context,
                                       std::string* /*error*/
) const {
    const auto options = ParseProtobufPluginOptions(parameter);
    auto headerPath = GetYaffFilePath(file, options.Tag, "h");
    auto sourcePath = GetYaffFilePath(file, options.Tag, "cpp");

    std::ostringstream headerOutput;
    std::ostringstream sourceOutput;
    if (NeedToProcessFile(options, file)) {
        auto front = GetFrontEnd(file, options);
        auto gen = GetGenerators(headerOutput, options);
        auto errorHandler = std::make_unique<yaff::compilation::ErrorPrinter>(std::cerr);
        yaff::compilation::Compile(std::move(front), std::move(gen), nullptr, std::move(errorHandler));
    }

    WriteFile(generator_context->Open(headerPath), headerOutput.view());

    sourceOutput << "#include \"" << StripPath(headerPath) << "\"";
    WriteFile(generator_context->Open(sourcePath), sourceOutput.view());

    return true;
}

uint64_t ProtobufGeneratorPlugin::GetSupportedFeatures() const {
    return ::google::protobuf::compiler::CodeGenerator::FEATURE_PROTO3_OPTIONAL;
}

}  // namespace yaff::tool
