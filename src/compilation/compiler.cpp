#include "compiler.h"

#include "ir_compat.h"
#include "ir_processor.h"

namespace NYaFF::NCompile {

NIR::TIR ParseIr(std::unique_ptr<IFrontEnd> front) {
    auto ir = front->Parse();
    ProcessIR(ir);
    return ir;
}

void Compile(std::unique_ptr<IFrontEnd> front, std::vector<std::unique_ptr<IGenerator>> gens,
             std::unique_ptr<IFrontEnd> baseFront, std::unique_ptr<IErrorHandler> errors) {
    YAFF_REQUIRE(front);

    auto ir = ParseIr(std::move(front));
    if (baseFront) {
        auto baseIr = ParseIr(std::move(baseFront));
        YAFF_REQUIRE(CheckIRCompatibility(baseIr, ir, std::move(errors)));
    }

    for (auto& gen : gens) {
        if (gen) {
            gen->Generate(ir);
        }
    }
}

}  // namespace NYaFF::NCompile
