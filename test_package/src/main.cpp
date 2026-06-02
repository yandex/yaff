#include <yaff/yaff.h>

#include <cassert>
#include <iostream>

#include "proto/smoke.yaff.h"

int main() {
    yaff::Serializer ys;

    auto label = ys.SerializeString("origin");
    auto point = protoyaff::smoke::SerializePoint(ys,
        /*x=*/     0,
        /*y=*/     0,
        /*label=*/ label
    );
    ys.Finish(point);
    auto buffer = ys.Release();

    const auto& p = yaff::ReadMessage<protoyaff::smoke::Point>(buffer.Data());
    assert(p.x() == 0);
    assert(p.y() == 0);
    assert(p.label() == "origin");

    std::cout << "OK: YaFF smoke test passed.\n";
    return 0;
}
