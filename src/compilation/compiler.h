#pragma once

#include "error.h"
#include "ir.h"

namespace yaff::compilation {

class AbstractFrontEnd {
public:
    virtual ~AbstractFrontEnd() = default;

    virtual ir::IR Parse() = 0;
};

class AbstractGenerator {
public:
    virtual ~AbstractGenerator() = default;

    virtual void Generate(const ir::IR& ir) = 0;
};

void Compile(std::unique_ptr<AbstractFrontEnd> front, std::vector<std::unique_ptr<AbstractGenerator>> gens,
             std::unique_ptr<AbstractFrontEnd> compatFront, std::unique_ptr<AbstractErrorHandler> errors);

}  // namespace yaff::compilation
