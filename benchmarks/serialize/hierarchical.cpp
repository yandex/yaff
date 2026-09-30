// clang-format off
#include "protoyaff/hierarchical.pb.h"
#include "protoyaff/hierarchical.yaff.h"

#include <benchmark/benchmark.h>
#include <google/protobuf/arena.h>
#include <cstdint>
#include <string>
// clang-format on

namespace {

namespace pb = benchmark_serialize;
namespace yf = protoyaff::benchmark_serialize;

void FillBatch(pb::Batch& batch, int count, bool varied, int valueCount, bool uniqueValues) {
    batch.mutable_records()->Reserve(count);
    batch.set_elapsed(0);
    for (int i = 0; i < count; ++i) {
        auto& record = *batch.add_records();
        record.set_id(i + 1);
        record.set_group(i / 2 + 1);
        record.set_owner(i / 8 + 1);
        record.set_domain(i / 40 + 1);
        record.set_generation(1);
        record.set_flags(1);
        record.set_fallback_flags(0);
        record.set_slot(0);
        record.set_deleted(false);
        record.set_filtered(false);
        record.set_experimental(false);
        record.set_kind(0);
        record.set_subtype(0);
        record.set_score(0.5);
        record.set_rank(i);
        record.set_type_mask(0);
        record.set_template_id(0);
        record.mutable_version()->set_id(varied ? i % 7 : 240);
        record.mutable_version()->set_revision(varied ? i + 1 : 1);
        record.mutable_query_version()->set_id(varied ? i % 9 : 240);
        record.mutable_query_version()->set_revision(varied ? i + 1001 : 1);
        record.mutable_timestamp()->set_id(varied ? i + 1 : 1);
        record.mutable_timestamp()->set_revision(varied ? i + 1 : 1);
        auto& attributes = *record.add_attributes();
        attributes.set_key(varied ? i + 1 : 11);
        attributes.set_kind(178);
        if (!varied || i % 2 == 0) {
            attributes.set_category(2);
        }
        if (varied && i % 3 != 0) {
            attributes.set_source(i + 2);
        }
        if (varied && i % 5 != 0) {
            attributes.set_enabled(false);
        }
        if (varied && i % 7 != 0) {
            record.set_subgroup(i);
        }
        if (varied && i % 11 != 0) {
            record.set_label("label-" + std::to_string(i % 8));
        }
        record.mutable_values()->Reserve(valueCount);
        for (int j = 0; j < valueCount; ++j) {
            // Identical arrays versus distinct arrays expose the cost/benefit of deduplication.
            const uint64_t index = uniqueValues ? uint64_t(i) * valueCount + j : j;
            record.add_values(static_cast<float>((index * 2654435761ULL) & 0xFFFFFF) / 16777216.0f);
        }
    }
}

void Serialize(yaff::Serializer& serializer, const pb::Batch& batch) {
    serializer.Finish(yf::SerializeBatch(serializer, batch));
    benchmark::DoNotOptimize(serializer.Data());
    benchmark::DoNotOptimize(serializer.Size());
    benchmark::ClobberMemory();
}

template <bool Reuse>
void ProtoToYaff(benchmark::State& state) {
    // Fixture construction runs before State starts the timer.
    google::protobuf::Arena arena;
    auto* batch = google::protobuf::Arena::Create<pb::Batch>(&arena);
    FillBatch(*batch, state.range(0), state.range(1), state.range(2), state.range(3));

    yaff::Serializer serializer;
    Serialize(serializer, *batch);
    const auto bytes = serializer.Size();

    for (auto _ : state) {
        if constexpr (Reuse) {
            // Clear is part of serialization; capacity is reused, contents/dedup state are not.
            serializer.Clear();
            Serialize(serializer, *batch);
        } else {
            // Per-message serializer allocation and destruction are included.
            yaff::Serializer fresh;
            Serialize(fresh, *batch);
        }
    }
    state.counters["wire_bytes"] = bytes;
    state.counters["records"] = batch->records_size();
    state.SetItemsProcessed(state.iterations() * batch->records_size());
}

void Scenarios(benchmark::Benchmark* benchmark) {
    benchmark->ArgNames({"records", "varied", "values", "unique_values"});
    for (int count : {1, 50, 750, 3000}) {
        benchmark->Args({count, 0, 0, 0});
    }
    benchmark->Args({750, 1, 0, 0});
    benchmark->Args({750, 1, 1, 0});
    benchmark->Args({750, 1, 32, 0});
    benchmark->Args({750, 1, 32, 1});
    benchmark->Args({0, 0, 0, 0});
}

BENCHMARK_TEMPLATE(ProtoToYaff, false)->Name("ProtoToYaff/Fresh")->Apply(Scenarios);
BENCHMARK_TEMPLATE(ProtoToYaff, true)->Name("ProtoToYaff/Reuse")->Apply(Scenarios);

}  // namespace
