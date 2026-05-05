#include <benchmark/benchmark.h>

#include <random>

#include "flatbuffers/hierarchical_generated.h"
#include "protoyaff/hierarchical.pb.h"
#include "protoyaff/hierarchical.yaff.h"

class TDataGenerator {
public:
    explicit TDataGenerator(uint64_t seed) : Rng_(seed) {
    }

    template <size_t Size>
    std::string GenerateProtobufMessages() {
        // This should be enough when considering the averaging of the varint length.
        static const uint64_t MAX_PROTOBUF_SIZE = 256;
        static const uint64_t LEN_PREFIX_SIZE = 4;
        const uint64_t n = (static_cast<double>(Size) / (MAX_PROTOBUF_SIZE + LEN_PREFIX_SIZE)) + 1;
        const auto& perm = GenerateCyclePermutation(n);

        std::string buffer;
        buffer.append(1, '\0');
        for (uint32_t val : perm) {
            const auto& root = GenerateProtobuf(val, MAX_PROTOBUF_SIZE + LEN_PREFIX_SIZE);
            const auto& serialized = root.SerializeAsString();
            const uint32_t size = serialized.size();

            YAFF_REQUIRE(size <= MAX_PROTOBUF_SIZE);
            buffer.append(reinterpret_cast<const char*>(&size), sizeof(size));
            buffer.append(serialized.data(), serialized.size());
            buffer.append(MAX_PROTOBUF_SIZE - serialized.size(), '\0');
        }

        return buffer;
    }

    template <size_t Size>
    std::string GenerateFlatBuffersMessages() {
        static const uint64_t FLATBUFFERS_SIZE = 304;
        const uint64_t n = (static_cast<double>(Size) / FLATBUFFERS_SIZE) + 1;
        const auto& perm = GenerateCyclePermutation(n);

        std::string buffer;
        buffer.append(1, '\0');
        for (uint32_t val : perm) {
            const std::vector<uint64_t> values = GenerateRandomVector64(24);

            const int64_t next = val * FLATBUFFERS_SIZE + 1;
            const int64_t addend = next - std::accumulate(values.begin(), values.begin() + 8, 0ULL);

            flatbuffers::FlatBufferBuilder fbb;
            const auto leaf = NFlatBuffersBench::CreateTLeaf(fbb, values[0], values[1], values[2], values[3], values[4],
                                                             values[5], values[6], values[7], addend);
            const auto intermediate =
                NFlatBuffersBench::CreateTIntermediate(fbb, values[8], values[9], values[10], values[11], values[12],
                                                       values[13], values[14], values[15], leaf);
            const auto root =
                NFlatBuffersBench::CreateTRoot(fbb, values[16], values[17], values[18], values[19], values[20],
                                               values[21], values[22], values[23], intermediate);
            fbb.Finish(root);

            YAFF_REQUIRE(fbb.GetSize() == FLATBUFFERS_SIZE);
            buffer.append(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
        }

        return buffer;
    }

    template <NYaFF::EMessageLayout Layout, size_t Size>
    std::string GenerateYaFFMessages() {
        static const uint64_t YAFF_SIZE = (Layout == NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT ? 218 : 239);
        const uint64_t n = (static_cast<double>(Size) / YAFF_SIZE) + 1;
        const auto& perm = GenerateCyclePermutation(n);

        std::string buffer;
        buffer.append(1, '\0');
        for (uint32_t val : perm) {
            const auto& protoRoot = GenerateProtobuf(val, YAFF_SIZE);

            NYaFF::TBuilder yffb;
            yffb.EnforceDynamicAlternative(Layout);
            yffb.Finish(NProtoYaFF::NYaFFBench::CreateTRoot(yffb, protoRoot));

            YAFF_REQUIRE(yffb.GetSize() == YAFF_SIZE);
            buffer.append(reinterpret_cast<const char*>(yffb.GetBufferPointer()), yffb.GetSize());
        }

        return buffer;
    }

private:
    NYaFFBench::TRoot GenerateProtobuf(uint32_t val, uint64_t size) {
        const std::vector<uint64_t> values = GenerateRandomVector64(24);
        const int64_t next = val * size + 1;
        const int64_t addend = next - std::accumulate(values.begin(), values.begin() + 8, 0ULL);

        NYaFFBench::TLeaf leaf;
        leaf.set_a(values[0]);
        leaf.set_b(values[1]);
        leaf.set_c(values[2]);
        leaf.set_d(values[3]);
        leaf.set_e(values[4]);
        leaf.set_f(values[5]);
        leaf.set_g(values[6]);
        leaf.set_h(values[7]);
        leaf.set_i(addend);

        NYaFFBench::TIntermediate intermediate;
        intermediate.set_v0(values[8]);
        intermediate.set_v1(values[9]);
        intermediate.set_v2(values[10]);
        intermediate.set_v3(values[11]);
        intermediate.set_v4(values[12]);
        intermediate.set_v5(values[13]);
        intermediate.set_v6(values[14]);
        intermediate.set_v7(values[15]);
        *intermediate.mutable_leaf() = leaf;

        NYaFFBench::TRoot root;
        root.set_i(values[16]);
        root.set_ii(values[17]);
        root.set_iii(values[18]);
        root.set_iv(values[19]);
        root.set_v(values[20]);
        root.set_vi(values[21]);
        root.set_vii(values[22]);
        root.set_viii(values[23]);
        *root.mutable_intermediate() = intermediate;

        return root;
    }

    std::vector<uint64_t> GenerateRandomVector64(size_t n) {
        std::uniform_int_distribution<uint64_t> dist(1, ((1ULL << 54) - 1));

        std::vector<uint64_t> result;
        result.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            result.emplace_back(dist(Rng_));
        }
        return result;
    }

    std::vector<uint32_t> GenerateCyclePermutation(uint32_t size) {
        std::vector<uint32_t> permutation(size);
        std::iota(permutation.begin(), permutation.end(), 0);

        for (int64_t i = size - 1; i > 0; --i) {
            std::uniform_int_distribution<uint32_t> dist(0, i - 1);
            std::swap(permutation[i], permutation[dist(Rng_)]);
        }
        return permutation;
    }

    std::mt19937 Rng_;
};

#define INTERRUPT_CHAIN(P, S)           \
    do {                                \
        uint64_t P##S = S;              \
        benchmark::DoNotOptimize(P##S); \
    } while (0)

#define TRY_INTERRUPT_CHAIN(F)          \
    if constexpr ((F)) {                \
        INTERRUPT_CHAIN(__x, __LINE__); \
    }

template <bool CacheChains, bool InterruptChain>
uint64_t SumHierarchicalFlatBuffers(const NFlatBuffersBench::TRoot* root) {
    uint64_t sum = 0;
    if constexpr (CacheChains) {
        const auto* leaf = root->intermediate()->leaf();
        sum += leaf->a();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf->b();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf->c();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf->d();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf->e();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf->f();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf->g();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf->h();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf->i();
    } else {
        sum += root->intermediate()->leaf()->a();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root->intermediate()->leaf()->b();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root->intermediate()->leaf()->c();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root->intermediate()->leaf()->d();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root->intermediate()->leaf()->e();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root->intermediate()->leaf()->f();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root->intermediate()->leaf()->g();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root->intermediate()->leaf()->h();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root->intermediate()->leaf()->i();
    }
    return sum;
}

template <typename T, bool CacheChains, bool InterruptChain>
uint64_t SumHierarchicalProtoYaFF(const T& root) {
    uint64_t sum = 0;
    if constexpr (CacheChains) {
        const auto& leaf = root.intermediate().leaf();
        sum += leaf.a();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf.b();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf.c();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf.d();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf.e();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf.f();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf.g();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf.h();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += leaf.i();
    } else {
        sum += root.intermediate().leaf().a();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root.intermediate().leaf().b();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root.intermediate().leaf().c();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root.intermediate().leaf().d();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root.intermediate().leaf().e();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root.intermediate().leaf().f();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root.intermediate().leaf().g();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root.intermediate().leaf().h();
        TRY_INTERRUPT_CHAIN(InterruptChain);
        sum += root.intermediate().leaf().i();
    }
    return sum;
}

template <uint64_t Size, bool CacheChains, bool InterruptChain>
void BM_Access_Hierarchical_Protobuf(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msgs = gen.GenerateProtobufMessages<Size>();

    volatile uint32_t next = 1;
    for (auto _ : state) {
        const uint32_t size = NYaFF::ReadValue<uint32_t>(msgs.data() + next);

        NYaFFBench::TRoot root;
        std::ignore = root.ParseFromArray(msgs.data() + next + sizeof(size), size);
        next = SumHierarchicalProtoYaFF<NYaFFBench::TRoot, CacheChains, InterruptChain>(root);
    }

    state.counters["object_size"] =
        benchmark::Counter(msgs.size(), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

template <uint64_t Size, bool CacheChains, bool InterruptChain>
void BM_Access_Hierarchical_FlatBuffers(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msgs = gen.GenerateFlatBuffersMessages<Size>();

    volatile uint32_t next = 1;
    for (auto _ : state) {
        const auto* root = flatbuffers::GetRoot<NFlatBuffersBench::TRoot>(msgs.data() + next);
        next = SumHierarchicalFlatBuffers<CacheChains, InterruptChain>(root);
    }

    state.counters["object_size"] =
        benchmark::Counter(msgs.size(), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

template <NYaFF::EMessageLayout Layout, uint64_t Size, bool CacheChains, bool InterruptChain>
void BM_Access_Hierarchical_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msgs = gen.GenerateYaFFMessages<Layout, Size>();

    volatile uint32_t next = 1;
    for (auto _ : state) {
        const auto& root = NYaFF::ReadRoot<NProtoYaFF::NYaFFBench::TRoot>(msgs.data() + next);
        next = SumHierarchicalProtoYaFF<NProtoYaFF::NYaFFBench::TRoot, CacheChains, InterruptChain>(root);
    }

    state.counters["object_size"] =
        benchmark::Counter(msgs.size(), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

// Using template args (instead of Arg) to set custom understandable names, as well as a user-friendly order in the
// report.
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Protobuf, 2048, false, false)
    ->Name("BM_Access_Hierarchical_Protobuf/HotAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Protobuf, 2048, false, true)
    ->Name("BM_Access_Hierarchical_Protobuf/HotAccess/CacheChains:false/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Protobuf, 2048, true, false)
    ->Name("BM_Access_Hierarchical_Protobuf/HotAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Protobuf, 2048, true, true)
    ->Name("BM_Access_Hierarchical_Protobuf/HotAccess/CacheChains:true/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_FlatBuffers, 2048, false, false)
    ->Name("BM_Access_Hierarchical_FlatBuffers/HotAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_FlatBuffers, 2048, false, true)
    ->Name("BM_Access_Hierarchical_FlatBuffers/HotAccess/CacheChains:false/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_FlatBuffers, 2048, true, false)
    ->Name("BM_Access_Hierarchical_FlatBuffers/HotAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_FlatBuffers, 2048, true, true)
    ->Name("BM_Access_Hierarchical_FlatBuffers/HotAccess/CacheChains:true/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT, 2048, false, false)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/HotAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT, 2048, false, true)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/HotAccess/CacheChains:false/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT, 2048, true, false)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/HotAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT, 2048, true, true)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/HotAccess/CacheChains:true/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE, 2048, false, false)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/HotAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE, 2048, false, true)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/HotAccess/CacheChains:false/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE, 2048, true, false)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/HotAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE, 2048, true, true)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/HotAccess/CacheChains:true/Modification:true");

BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Protobuf, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_Protobuf/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Protobuf, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_Protobuf/ColdAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_FlatBuffers, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_FlatBuffers/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_FlatBuffers, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_FlatBuffers/ColdAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/ColdAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/ColdAccess/CacheChains:true/Modification:false");
