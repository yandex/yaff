#pragma once

#include "ir.h"

namespace yaff::compilation::ir::reflect {

std::vector<uint8_t> GenerateStringDefault(const ir::TypeDef& type);

}
