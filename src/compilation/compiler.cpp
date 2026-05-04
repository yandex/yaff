#include "compiler.h"

#include "ir_compat.h"
#include "ir_processor.h"

namespace yaff::compilation {

ir::IR ParseIr(std::unique_ptr<AbstractFrontEnd> front) {
    auto ir = front->Parse();
    ProcessIR(ir);
    return ir;
}

void Compile(std::unique_ptr<AbstractFrontEnd> front, std::vector<std::unique_ptr<AbstractGenerator>> gens,
             std::unique_ptr<AbstractFrontEnd> baseFront, std::unique_ptr<AbstractErrorHandler> errors) {
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

}  // namespace yaff::compilation
