#pragma once

#include <unordered_set>

#include "ir.h"

namespace yaff::compilation {

std::vector<const ir::SchemaDef*> GetSchemaDependencyOrder(const ir::IR& ir);
std::vector<const ir::MessageDef*> GetMessageDependencyOrder(const ir::SchemaDef& schemaDef);

std::string ToCamelCase(const std::string& input, bool upper);

std::string ToHexCode(uint8_t x);

bool IsLowerString(const std::string& input);
std::string ToLowerString(const std::string& input);

const std::unordered_set<std::string_view>& GetCppKeywords();

}  // namespace yaff::compilation
