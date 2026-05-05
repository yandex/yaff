#include "util.h"

namespace NYaFF::NCompile {

static void GetSchemaDependencyOrderImpl(const NIR::TSchemaDef* schemaDef,
                                         std::unordered_set<const NIR::TSchemaDef*>& used,
                                         std::vector<const NIR::TSchemaDef*>& order) {
    YAFF_REQUIRE(schemaDef);
    used.emplace(schemaDef);
    for (const auto* dependency : schemaDef->Dependencies) {
        if (!used.contains(dependency)) {
            GetSchemaDependencyOrderImpl(dependency, used, order);
        }
    }
    order.emplace_back(schemaDef);
}

static void GetTableDependencyOrderImpl(const NIR::TTableDef* tableDef, std::unordered_set<const NIR::TTableDef*>& used,
                                        std::vector<const NIR::TTableDef*>& order) {
    YAFF_REQUIRE(tableDef);
    used.emplace(tableDef);
    for (const auto& fieldDef : tableDef->Fields) {
        YAFF_REQUIRE(fieldDef.Type);
        const auto* next = NIR::ExtractTableDef(*fieldDef.Type);
        if (next && next->Schema == tableDef->Schema && !used.contains(next)) {
            GetTableDependencyOrderImpl(next, used, order);
        }
    }
    order.emplace_back(tableDef);
}

std::vector<const NIR::TSchemaDef*> GetSchemaDependencyOrder(const NIR::TIR& ir) {
    const size_t schemaCnt = ir.Schemas.Table.size();

    std::vector<const NIR::TSchemaDef*> order;
    order.reserve(schemaCnt);
    std::unordered_set<const NIR::TSchemaDef*> used;
    used.reserve(schemaCnt);

    for (const auto& [_, schema] : ir.Schemas.Table) {
        if (!used.contains(&schema)) {
            GetSchemaDependencyOrderImpl(&schema, used, order);
        }
    }

    return order;
}

std::vector<const NIR::TTableDef*> GetTableDependencyOrder(const NIR::TSchemaDef& schemaDef) {
    const size_t tableCount = schemaDef.Tables.size();
    std::vector<const NIR::TTableDef*> order;
    order.reserve(tableCount);
    std::unordered_set<const NIR::TTableDef*> used;
    used.reserve(tableCount);

    for (const auto* tableDef : schemaDef.Tables) {
        if (!used.contains(tableDef)) {
            GetTableDependencyOrderImpl(tableDef, used, order);
        }
    }

    return order;
}

std::string ToCamelCase(const std::string& input, bool upper) {
    std::string camelCase;
    camelCase.reserve(input.size());

    for (size_t i = 0; i < input.length(); ++i) {
        if (i == 0) {
            camelCase += (upper ? toupper(input[i]) : tolower(input[i]));
        } else if (input[i] == '_' && i + 1 < input.length()) {
            camelCase += toupper(input[++i]);
        } else {
            camelCase += input[i];
        }
    }

    return camelCase;
}

std::string ToHexCode(uint8_t x) {
    static const std::string_view dict = "0123456789abcdef";
    std::string code;
    code.resize(2);
    code[0] = dict[(x & 0xf0) >> 4];
    code[1] = dict[(x & 0x0f)];
    return code;
}

bool IsLowerString(const std::string& input) {
    for (char ch : input) {
        if (std::isalpha(ch) && !std::islower(ch)) {
            return false;
        }
    }
    return true;
}

std::string ToLowerString(const std::string& input) {
    std::string result = input;
    for (char& c : result) {
        c = std::tolower(c);
    }
    return result;
}

const std::unordered_set<std::string_view>& GetCppKeywords() {
    // clang-format off
    static const std::unordered_set<std::string_view> keywords {
        "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand",
        "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class", "compl",
        "concept", "const", "constexpr", "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype", "default",
        "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
        "float", "for", "friend", "goto", "if", "import", "inline", "int", "long", "module",
        "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq",
        "private", "protected", "public", "register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof",
        "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local", "throw",
        "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void",
        "volatile", "wchar_t", "while", "xor", "xor_eq",
    };
    // clang-format on
    return keywords;
}

}  // namespace NYaFF::NCompile
