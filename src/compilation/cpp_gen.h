#pragma once

#include "compiler.h"

namespace yaff::compilation {

struct CppGenerationOptions {
    std::string IncludesSuffix;
    std::vector<std::string> ExternalIncludes;

    bool GenerateProtobufApi = false;
    bool SuppressProtobufIncludes = false;
    bool GenerateReflectionApi = false;
};

class CppGenerator : public AbstractGenerator {
public:
    CppGenerator(std::ostream& out, CppGenerationOptions opts = {});
    ~CppGenerator();

    void Generate(const ir::IR& ir) override;

private:
    class Impl;
    std::unique_ptr<Impl> Impl_;
};

}  // namespace yaff::compilation
