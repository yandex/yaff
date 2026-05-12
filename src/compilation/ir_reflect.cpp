#include "ir_reflect.h"

#include <yaff/serializer.h>

namespace yaff::compilation::ir::reflect {

std::vector<uint8_t> GenerateStringDefault(const ir::TypeDef& type) {
    YAFF_REQUIRE(type.Type == Type::TYPE_STRING);

    const auto* literal = FindKey(type.Modifiers, ir::DEFAULT_MODIFIER_NAME);
    YAFF_REQUIRE(literal);

    yaff::Serializer ys;
    ys.SerializeString(*literal);
    ys.FinishRootless();

    std::vector<uint8_t> bytes;
    bytes.resize(ys.Size());
    YAFF_MEMCPY(bytes.data(), ys.Data(), ys.Size());

    return bytes;
}

}  // namespace yaff::compilation::ir::reflect
