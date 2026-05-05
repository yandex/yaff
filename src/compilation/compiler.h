#pragma once

#include "error.h"
#include "ir.h"

namespace NYaFF::NCompile {

class IFrontEnd {
public:
    virtual ~IFrontEnd() = default;

    virtual NIR::TIR Parse() = 0;
};

class IGenerator {
public:
    virtual ~IGenerator() = default;

    virtual void Generate(const NIR::TIR& ir) = 0;
};

void Compile(std::unique_ptr<IFrontEnd> front, std::vector<std::unique_ptr<IGenerator>> gens,
             std::unique_ptr<IFrontEnd> compatFront, std::unique_ptr<IErrorHandler> errors);

}  // namespace NYaFF::NCompile
