#include <benchmark/benchmark.h>

#include <random>

#include "flatbuffers/flat_generated.h"
#include "protoyaff/flat.pb.h"
#include "protoyaff/flat.yaff.h"

static constexpr size_t SPACE_SIZE_SAMPLES = 100000;

class TDataGenerator {
public:
    explicit TDataGenerator(uint64_t seed) : Rng_(seed) {
    }

    std::string GenerateProtobufFlat5(size_t max, double density) {
        const std::vector<uint64_t> values = GenerateRandomVector(5, max);

        benchmark_space::Flat5 proto;
        if (const auto v = WithDensity(values[0], density)) {
            proto.set_v0(*v);
        }
        if (const auto v = WithDensity(values[1], density)) {
            proto.set_v1(*v);
        }
        if (const auto v = WithDensity(values[2], density)) {
            proto.set_v2(*v);
        }
        if (const auto v = WithDensity(values[3], density)) {
            proto.set_v3(*v);
        }
        if (const auto v = WithDensity(values[4], density)) {
            proto.set_v4(*v);
        }

        return proto.SerializeAsString();
    }

    std::string GenerateProtobufFlat50(size_t max, double density) {
        const std::vector<uint64_t> values = GenerateRandomVector(50, max);

        benchmark_space::Flat50 proto;
        if (const auto v = WithDensity(values[0], density)) {
            proto.set_v0(*v);
        }
        if (const auto v = WithDensity(values[1], density)) {
            proto.set_v1(*v);
        }
        if (const auto v = WithDensity(values[2], density)) {
            proto.set_v2(*v);
        }
        if (const auto v = WithDensity(values[3], density)) {
            proto.set_v3(*v);
        }
        if (const auto v = WithDensity(values[4], density)) {
            proto.set_v4(*v);
        }
        if (const auto v = WithDensity(values[5], density)) {
            proto.set_v5(*v);
        }
        if (const auto v = WithDensity(values[6], density)) {
            proto.set_v6(*v);
        }
        if (const auto v = WithDensity(values[7], density)) {
            proto.set_v7(*v);
        }
        if (const auto v = WithDensity(values[8], density)) {
            proto.set_v8(*v);
        }
        if (const auto v = WithDensity(values[9], density)) {
            proto.set_v9(*v);
        }
        if (const auto v = WithDensity(values[10], density)) {
            proto.set_v10(*v);
        }
        if (const auto v = WithDensity(values[11], density)) {
            proto.set_v11(*v);
        }
        if (const auto v = WithDensity(values[12], density)) {
            proto.set_v12(*v);
        }
        if (const auto v = WithDensity(values[13], density)) {
            proto.set_v13(*v);
        }
        if (const auto v = WithDensity(values[14], density)) {
            proto.set_v14(*v);
        }
        if (const auto v = WithDensity(values[15], density)) {
            proto.set_v15(*v);
        }
        if (const auto v = WithDensity(values[16], density)) {
            proto.set_v16(*v);
        }
        if (const auto v = WithDensity(values[17], density)) {
            proto.set_v17(*v);
        }
        if (const auto v = WithDensity(values[18], density)) {
            proto.set_v18(*v);
        }
        if (const auto v = WithDensity(values[19], density)) {
            proto.set_v19(*v);
        }
        if (const auto v = WithDensity(values[20], density)) {
            proto.set_v20(*v);
        }
        if (const auto v = WithDensity(values[21], density)) {
            proto.set_v21(*v);
        }
        if (const auto v = WithDensity(values[22], density)) {
            proto.set_v22(*v);
        }
        if (const auto v = WithDensity(values[23], density)) {
            proto.set_v23(*v);
        }
        if (const auto v = WithDensity(values[24], density)) {
            proto.set_v24(*v);
        }
        if (const auto v = WithDensity(values[25], density)) {
            proto.set_v25(*v);
        }
        if (const auto v = WithDensity(values[26], density)) {
            proto.set_v26(*v);
        }
        if (const auto v = WithDensity(values[27], density)) {
            proto.set_v27(*v);
        }
        if (const auto v = WithDensity(values[28], density)) {
            proto.set_v28(*v);
        }
        if (const auto v = WithDensity(values[29], density)) {
            proto.set_v29(*v);
        }
        if (const auto v = WithDensity(values[30], density)) {
            proto.set_v30(*v);
        }
        if (const auto v = WithDensity(values[31], density)) {
            proto.set_v31(*v);
        }
        if (const auto v = WithDensity(values[32], density)) {
            proto.set_v32(*v);
        }
        if (const auto v = WithDensity(values[33], density)) {
            proto.set_v33(*v);
        }
        if (const auto v = WithDensity(values[34], density)) {
            proto.set_v34(*v);
        }
        if (const auto v = WithDensity(values[35], density)) {
            proto.set_v35(*v);
        }
        if (const auto v = WithDensity(values[36], density)) {
            proto.set_v36(*v);
        }
        if (const auto v = WithDensity(values[37], density)) {
            proto.set_v37(*v);
        }
        if (const auto v = WithDensity(values[38], density)) {
            proto.set_v38(*v);
        }
        if (const auto v = WithDensity(values[39], density)) {
            proto.set_v39(*v);
        }
        if (const auto v = WithDensity(values[40], density)) {
            proto.set_v40(*v);
        }
        if (const auto v = WithDensity(values[41], density)) {
            proto.set_v41(*v);
        }
        if (const auto v = WithDensity(values[42], density)) {
            proto.set_v42(*v);
        }
        if (const auto v = WithDensity(values[43], density)) {
            proto.set_v43(*v);
        }
        if (const auto v = WithDensity(values[44], density)) {
            proto.set_v44(*v);
        }
        if (const auto v = WithDensity(values[45], density)) {
            proto.set_v45(*v);
        }
        if (const auto v = WithDensity(values[46], density)) {
            proto.set_v46(*v);
        }
        if (const auto v = WithDensity(values[47], density)) {
            proto.set_v47(*v);
        }
        if (const auto v = WithDensity(values[48], density)) {
            proto.set_v48(*v);
        }
        if (const auto v = WithDensity(values[49], density)) {
            proto.set_v49(*v);
        }

        return proto.SerializeAsString();
    }

    flatbuffers::DetachedBuffer GenerateFlatBuffersFlat5(size_t max, double density) {
        const std::vector<uint64_t> values = GenerateRandomVector(5, max);

        flatbuffers::FlatBufferBuilder fbb;
        fbb.ForceDefaults(false);
        const auto root = NFlatBuffersBench::CreateFlat5(
            fbb, WithDensity(values[0], density), WithDensity(values[1], density), WithDensity(values[2], density),
            WithDensity(values[3], density), WithDensity(values[4], density));
        fbb.Finish(root);

        return fbb.Release();
    }

    flatbuffers::DetachedBuffer GenerateFlatBuffersFlat50(size_t max, double density) {
        const std::vector<uint64_t> values = GenerateRandomVector(50, max);

        flatbuffers::FlatBufferBuilder fbb;
        fbb.ForceDefaults(false);
        const auto root = NFlatBuffersBench::CreateFlat50(
            fbb, WithDensity(values[0], density), WithDensity(values[1], density), WithDensity(values[2], density),
            WithDensity(values[3], density), WithDensity(values[4], density), WithDensity(values[5], density),
            WithDensity(values[6], density), WithDensity(values[7], density), WithDensity(values[8], density),
            WithDensity(values[9], density), WithDensity(values[10], density), WithDensity(values[11], density),
            WithDensity(values[12], density), WithDensity(values[13], density), WithDensity(values[14], density),
            WithDensity(values[15], density), WithDensity(values[16], density), WithDensity(values[17], density),
            WithDensity(values[18], density), WithDensity(values[19], density), WithDensity(values[20], density),
            WithDensity(values[21], density), WithDensity(values[22], density), WithDensity(values[23], density),
            WithDensity(values[24], density), WithDensity(values[25], density), WithDensity(values[26], density),
            WithDensity(values[27], density), WithDensity(values[28], density), WithDensity(values[29], density),
            WithDensity(values[30], density), WithDensity(values[31], density), WithDensity(values[32], density),
            WithDensity(values[33], density), WithDensity(values[34], density), WithDensity(values[35], density),
            WithDensity(values[36], density), WithDensity(values[37], density), WithDensity(values[38], density),
            WithDensity(values[39], density), WithDensity(values[40], density), WithDensity(values[41], density),
            WithDensity(values[42], density), WithDensity(values[43], density), WithDensity(values[44], density),
            WithDensity(values[45], density), WithDensity(values[46], density), WithDensity(values[47], density),
            WithDensity(values[48], density), WithDensity(values[49], density));
        fbb.Finish(root);

        return fbb.Release();
    }

    template <yaff::MessageLayout Layout>
    yaff::DetachedBuffer GenerateYaFFFlat5(size_t max, double density) {
        const std::vector<uint64_t> values = GenerateRandomVector(5, max);

        yaff::Serializer ys;
        ys.EnforceDynamicAlternative(Layout);
        const auto root = protoyaff::benchmark_space::SerializeFlat5(
            ys, WithDensity(values[0], density), WithDensity(values[1], density), WithDensity(values[2], density),
            WithDensity(values[3], density), WithDensity(values[4], density));
        ys.Finish(root);

        return ys.Release();
    }

    template <yaff::MessageLayout Layout>
    yaff::DetachedBuffer GenerateYaFFFlat50(size_t max, double density) {
        const std::vector<uint64_t> values = GenerateRandomVector(50, max);

        yaff::Serializer ys;
        ys.EnforceDynamicAlternative(Layout);
        const auto root = protoyaff::benchmark_space::SerializeFlat50(
            ys, WithDensity(values[0], density), WithDensity(values[1], density), WithDensity(values[2], density),
            WithDensity(values[3], density), WithDensity(values[4], density), WithDensity(values[5], density),
            WithDensity(values[6], density), WithDensity(values[7], density), WithDensity(values[8], density),
            WithDensity(values[9], density), WithDensity(values[10], density), WithDensity(values[11], density),
            WithDensity(values[12], density), WithDensity(values[13], density), WithDensity(values[14], density),
            WithDensity(values[15], density), WithDensity(values[16], density), WithDensity(values[17], density),
            WithDensity(values[18], density), WithDensity(values[19], density), WithDensity(values[20], density),
            WithDensity(values[21], density), WithDensity(values[22], density), WithDensity(values[23], density),
            WithDensity(values[24], density), WithDensity(values[25], density), WithDensity(values[26], density),
            WithDensity(values[27], density), WithDensity(values[28], density), WithDensity(values[29], density),
            WithDensity(values[30], density), WithDensity(values[31], density), WithDensity(values[32], density),
            WithDensity(values[33], density), WithDensity(values[34], density), WithDensity(values[35], density),
            WithDensity(values[36], density), WithDensity(values[37], density), WithDensity(values[38], density),
            WithDensity(values[39], density), WithDensity(values[40], density), WithDensity(values[41], density),
            WithDensity(values[42], density), WithDensity(values[43], density), WithDensity(values[44], density),
            WithDensity(values[45], density), WithDensity(values[46], density), WithDensity(values[47], density),
            WithDensity(values[48], density), WithDensity(values[49], density));
        ys.Finish(root);

        return ys.Release();
    }

private:
    inline static const uint64_t MAX_DENSITY = 100;

    std::optional<uint64_t> WithDensity(uint64_t value, uint64_t density) {
        std::uniform_int_distribution<uint64_t> dist(0, MAX_DENSITY);
        return (dist(Rng_) <= density ? std::optional<uint64_t>{value} : std::nullopt);
    }

    std::vector<uint64_t> GenerateRandomVector(size_t n, size_t max) {
        std::uniform_int_distribution<uint64_t> dist(0, max);

        std::vector<uint64_t> result;
        result.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            result.emplace_back(dist(Rng_));
        }
        return result;
    }

    std::mt19937 Rng_;
};

void BM_Space_Flat5_Protobuf(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    size_t totalSize = 0;
    for (size_t i = 0; i < SPACE_SIZE_SAMPLES; ++i) {
        totalSize += gen.GenerateProtobufFlat5(state.range(0), state.range(1)).size();
    }
    for (auto _ : state) {
    }
    state.counters["Size(bytes)"] = benchmark::Counter(static_cast<double>(totalSize) / SPACE_SIZE_SAMPLES,
                                                       benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

void BM_Space_Flat50_Protobuf(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    size_t totalSize = 0;
    for (size_t i = 0; i < SPACE_SIZE_SAMPLES; ++i) {
        totalSize += gen.GenerateProtobufFlat50(state.range(0), state.range(1)).size();
    }
    for (auto _ : state) {
    }
    state.counters["Size(bytes)"] = benchmark::Counter(static_cast<double>(totalSize) / SPACE_SIZE_SAMPLES,
                                                       benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

void BM_Space_Flat5_FlatBuffers(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    size_t totalSize = 0;
    for (size_t i = 0; i < SPACE_SIZE_SAMPLES; ++i) {
        totalSize += gen.GenerateFlatBuffersFlat5(state.range(0), state.range(1)).size();
    }
    for (auto _ : state) {
    }
    state.counters["Size(bytes)"] = benchmark::Counter(static_cast<double>(totalSize) / SPACE_SIZE_SAMPLES,
                                                       benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

void BM_Space_Flat50_FlatBuffers(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    size_t totalSize = 0;
    for (size_t i = 0; i < SPACE_SIZE_SAMPLES; ++i) {
        totalSize += gen.GenerateFlatBuffersFlat50(state.range(0), state.range(1)).size();
    }
    for (auto _ : state) {
    }
    state.counters["Size(bytes)"] = benchmark::Counter(static_cast<double>(totalSize) / SPACE_SIZE_SAMPLES,
                                                       benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

template <yaff::MessageLayout Layout>
void BM_Space_Flat5_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    size_t totalSize = 0;
    for (size_t i = 0; i < SPACE_SIZE_SAMPLES; ++i) {
        totalSize += gen.GenerateYaFFFlat5<Layout>(state.range(0), state.range(1)).Size();
    }
    for (auto _ : state) {
    }
    state.counters["Size(bytes)"] = benchmark::Counter(static_cast<double>(totalSize) / SPACE_SIZE_SAMPLES,
                                                       benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

template <yaff::MessageLayout Layout>
void BM_Space_Flat50_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    size_t totalSize = 0;
    for (size_t i = 0; i < SPACE_SIZE_SAMPLES; ++i) {
        totalSize += gen.GenerateYaFFFlat50<Layout>(state.range(0), state.range(1)).Size();
    }
    for (auto _ : state) {
    }
    state.counters["Size(bytes)"] = benchmark::Counter(static_cast<double>(totalSize) / SPACE_SIZE_SAMPLES,
                                                       benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

BENCHMARK(BM_Space_Flat5_Protobuf)
    ->Name("BM_Space_Flat_Protobuf/FieldCount:5")
    ->ArgNames({"MaxValue", "Density"})
    ->ArgsProduct({{std::numeric_limits<int64_t>::max(), 1'000'000}, {100, 75, 50, 25, 5}})
    ->Iterations(1);
BENCHMARK(BM_Space_Flat5_FlatBuffers)
    ->Name("BM_Space_Flat_FlatBuffers/FieldCount:5")
    ->ArgNames({"MaxValue", "Density"})
    ->ArgsProduct({{std::numeric_limits<int64_t>::max()}, {100, 75, 50, 25, 5}})
    ->Iterations(1);
BENCHMARK_TEMPLATE(BM_Space_Flat5_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT)
    ->Name("BM_Space_Flat_YaFF/FlatLayout/FieldCount:5")
    ->ArgNames({"MaxValue", "Density"})
    ->ArgsProduct({{std::numeric_limits<int64_t>::max()}, {100, 75, 50, 25, 5}})
    ->Iterations(1);
BENCHMARK_TEMPLATE(BM_Space_Flat5_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE)
    ->Name("BM_Space_Flat_YaFF/SparseLayout/FieldCount:5")
    ->ArgNames({"MaxValue", "Density"})
    ->ArgsProduct({{std::numeric_limits<int64_t>::max()}, {100, 75, 50, 25, 5}})
    ->Iterations(1);

BENCHMARK(BM_Space_Flat50_Protobuf)
    ->Name("BM_Space_Flat_Protobuf/FieldCount:50")
    ->ArgNames({"MaxValue", "Density"})
    ->ArgsProduct({{std::numeric_limits<int64_t>::max(), 1'000'000}, {100, 75, 50, 25, 5}})
    ->Iterations(1);
BENCHMARK(BM_Space_Flat50_FlatBuffers)
    ->Name("BM_Space_Flat_FlatBuffers/FieldCount:50")
    ->ArgNames({"MaxValue", "Density"})
    ->ArgsProduct({{std::numeric_limits<int64_t>::max()}, {100, 75, 50, 25, 5}})
    ->Iterations(1);
BENCHMARK_TEMPLATE(BM_Space_Flat50_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT)
    ->Name("BM_Space_Flat_YaFF/FlatLayout/FieldCount:50")
    ->ArgNames({"MaxValue", "Density"})
    ->ArgsProduct({{std::numeric_limits<int64_t>::max()}, {100, 75, 50, 25, 5}})
    ->Iterations(1);
BENCHMARK_TEMPLATE(BM_Space_Flat50_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE)
    ->Name("BM_Space_Flat_YaFF/SparseLayout/FieldCount:50")
    ->ArgNames({"MaxValue", "Density"})
    ->ArgsProduct({{std::numeric_limits<int64_t>::max()}, {100, 75, 50, 25, 5}})
    ->Iterations(1);
