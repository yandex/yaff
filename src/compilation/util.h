#pragma once

#include <unordered_set>

#include "ir.h"

namespace NYaFF::NCompile {

std::vector<const NIR::TSchemaDef*> GetSchemaDependencyOrder(const NIR::TIR& ir);
std::vector<const NIR::TMessageDef*> GetMessageDependencyOrder(const NIR::TSchemaDef& schemaDef);

std::string ToCamelCase(const std::string& input, bool upper);

std::string ToHexCode(uint8_t x);

bool IsLowerString(const std::string& input);
std::string ToLowerString(const std::string& input);

const std::unordered_set<std::string_view>& GetCppKeywords();

template <class... Fs>
struct TOverloaded : Fs... {
    using Fs::operator()...;
};

template <class... Fs>
TOverloaded(Fs...) -> TOverloaded<Fs...>;

}  // namespace NYaFF::NCompile
