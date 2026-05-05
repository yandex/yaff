#pragma once

#include "error.h"
#include "ir.h"

namespace NYaFF::NCompile {

bool CheckIRCompatibility(const NIR::TIR& base, const NIR::TIR& patched, std::unique_ptr<IErrorHandler> errorHandler);

}
