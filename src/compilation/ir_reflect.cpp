#include "ir_reflect.h"

#include <yaff/builder.h>

namespace NYaFF::NCompile::NIR::NReflect {

std::vector<uint8_t> GenerateStringDefault(const NIR::TType& type) {
    YAFF_REQUIRE(type.Type == EType::TYPE_STRING);

    const auto* literal = FindKey(type.Modifiers, NIR::DEFAULT_MODIFIER_NAME);
    YAFF_REQUIRE(literal);

    NYaFF::TBuilder yffb;
    yffb.CreateString(*literal);
    yffb.Finish();

    std::vector<uint8_t> bytes;
    bytes.resize(yffb.GetSize());
    YAFF_MEMCPY(bytes.data(), yffb.GetBufferPointer(), yffb.GetSize());

    return bytes;
}

}  // namespace NYaFF::NCompile::NIR::NReflect
