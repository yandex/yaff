#include <yaff/visitor.h>

#include <iostream>

#include "proto/order.yaff.h"

NYaFF::TDetachedBuffer BuildOrderDirect() {
    // clang-format off
    NYaFF::TBuilder yffb;

    // Pre-create nested objects (strings, vectors, nested messages)
    // before starting the main message.
    auto symbol = yffb.CreateString("AAPL");

    auto exec0 = NProtoYaFF::NTrading::CreateTExecution(yffb,
        /*exec_id=*/           1001ULL,
        /*fill_price_micros=*/ 17325000000ULL,  // $173.25
        /*fill_qty=*/          50U,
        /*exchange_ts_ns=*/    1700000000000000100ULL
    );

    auto exec1 = NProtoYaFF::NTrading::CreateTExecution(yffb,
        /*exec_id=*/           1002ULL,
        /*fill_price_micros=*/ 17326500000ULL,  // $173.265
        /*fill_qty=*/          50U,
        /*exchange_ts_ns=*/    1700000000000000200ULL
    );

    std::vector<NYaFF::TInternalOffset<NProtoYaFF::NTrading::TExecution>> execs{exec0, exec1};
    auto executions = yffb.CreateVector(execs);

    auto root = NProtoYaFF::NTrading::CreateTTradeOrder(yffb,
        /*order_id=*/           7700ULL,
        /*client_order_id=*/    100500ULL,
        /*symbol=*/             symbol,
        /*side=*/               NProtoYaFF::NTrading::ESide::SIDE_BUY,
        /*limit_price_micros=*/ 17330000000ULL,  // $173.30 limit
        /*quantity=*/           100U,
        /*account_id=*/         42U,
        /*submit_ts_ns=*/       1700000000000000000ULL,
        /*executions=*/         executions
    );
    // clang-format on

    yffb.Finish(root);
    return yffb.Release();
}

void DemoOrder(const NYaFF::TDetachedBuffer& buffer) {
    const auto& order = NYaFF::ReadRoot<NProtoYaFF::NTrading::TTradeOrder>(buffer.Data());
    std::cout << "order_id:           " << order.order_id() << "\n";
    std::cout << "client_order_id:    " << order.client_order_id() << "\n";
    std::cout << "symbol:             " << order.symbol() << "\n";
    std::cout << "side:               " << ESide_Name(order.side()) << "\n";
    std::cout << "limit_price_micros: " << order.limit_price_micros() << "\n";
    std::cout << "quantity:           " << order.quantity() << "\n";
    std::cout << "account_id:         " << order.account_id() << "\n";
    std::cout << "submit_ts_ns:       " << order.submit_ts_ns() << "\n";

    const auto& fills = order.executions();
    std::cout << "executions (" << fills.Size() << "):\n";
    for (uint32_t i = 0; i < fills.Size(); ++i) {
        const auto& e = fills.Get(i);
        std::cout << "  [" << i << "] exec_id=" << e.exec_id() << " price=" << e.fill_price_micros()
                  << " qty=" << e.fill_qty() << " ts=" << e.exchange_ts_ns() << "\n";
    }

    std::cout << "\nHuman Readable:\n";
    std::cout << NYaFF::NReflect::DebugString(&order, NProtoYaFF::NTrading::TTradeOrder::Descriptor()) << "\n";
}

NTrading::TTradeOrder YaffToProto(const NProtoYaFF::NTrading::TTradeOrder& yaff) {
    NTrading::TTradeOrder proto;
    yaff.TransformTo(proto);
    return proto;
}

void Verify(const NProtoYaFF::NTrading::TTradeOrder& original, const NTrading::TTradeOrder& restored) {
    YAFF_REQUIRE(original.order_id() == restored.order_id());
    YAFF_REQUIRE(original.symbol() == restored.symbol());
    YAFF_REQUIRE(static_cast<int>(original.side()) == static_cast<int>(restored.side()));
    YAFF_REQUIRE(original.quantity() == restored.quantity());
    YAFF_REQUIRE(original.executions_size() == restored.executions_size());
    YAFF_REQUIRE(original.executions(0).exec_id() == restored.executions(0).exec_id());
    YAFF_REQUIRE(original.executions(0).fill_qty() == restored.executions(0).fill_qty());
    YAFF_REQUIRE(original.executions(1).exec_id() == restored.executions(1).exec_id());

    std::cout << "OK: Transform verified.\n";
}

int main() {
    // 1. Build YaFF directly — bypasses protobuf entirely.
    // This is significantly cheaper than building a protobuf message
    // and then converting it: we skip protobuf heap allocations,
    // string copies, and the intermediate serialization step entirely.
    auto buffer = BuildOrderDirect();

    // 2. Read via proto-like API.
    DemoOrder(buffer);

    // 3. Transform YaFF -> Protobuf.
    const auto& yaff = NYaFF::ReadRoot<NProtoYaFF::NTrading::TTradeOrder>(buffer.Data());
    const auto& proto = YaffToProto(yaff);
    Verify(yaff, proto);

    return 0;
}
