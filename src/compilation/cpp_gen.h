#pragma once

#include "compiler.h"

namespace NYaFF::NCompile {

struct TCppGenerationOptions {
    std::string IncludesSuffix;
    std::vector<std::string> ExternalIncludes;

    bool GenerateProtobufApi = false;
    bool SuppressProtobufIncludes = false;
    bool GenerateReflectionApi = false;
};

class TCppGenerator : public IGenerator {
public:
    TCppGenerator(std::ostream& out, TCppGenerationOptions opts = {});
    ~TCppGenerator();

    void Generate(const NIR::TIR& ir) override;

private:
    class TImpl;
    std::unique_ptr<TImpl> Impl_;
};

}  // namespace NYaFF::NCompile
