#pragma once

#include "ir.h"

namespace NYaFF::NCompile::NIR::NReflect {

std::vector<uint8_t> GenerateStringDefault(const NIR::TType& type);

}
