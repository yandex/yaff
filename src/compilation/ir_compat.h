#pragma once

#include "error.h"
#include "ir.h"

namespace yaff::compilation {

bool CheckIRCompatibility(const ir::IR& base, const ir::IR& patched,
                          std::unique_ptr<AbstractErrorHandler> errorHandler);

}
