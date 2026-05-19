#include <benchmark/benchmark.h>

#include <random>

#include "flatbuffers/hierarchical_generated.h"
#include "protoyaff/hierarchical.pb.h"
#include "protoyaff/hierarchical.yaff.h"

namespace benchmark_access_raw {

#define YAFF_RAW_FIELD(N, n) \
    uint64_t N = 0;          \
    uint64_t n() const {     \
        return N;            \
    }

template <typename T>
using Inline = T;
template <typename T>
using Pointer = const T*;

template <typename T>
const T& DeRef(const T& x) {
    return x;
}

template <typename T>
const T& DeRef(const T* x) {
    return *x;
}

template <typename T>
T* ChildAddress(T& slot, void*) {
    return &slot;
}

template <typename T>
T* ChildAddress(const T*&, void* fallback) {
    return reinterpret_cast<T*>(fallback);
}

template <typename T>
void LinkChild(T&, T*) {
}

template <typename T>
void LinkChild(const T*& slot, T* child) {
    slot = child;
}

// N.B.: YAFF_LAYOUT_BEGIN is only needed to legalize aliasing
// for structures and has no effect on the implementation.
//
// When implementing a protocol on raw C++ structures,
// you will still need to do something similar to mitigate the UB.

// clang-format off
YAFF_LAYOUT_BEGIN(Leaf) {
    YAFF_RAW_FIELD(A, a)
    YAFF_RAW_FIELD(B, b)
    YAFF_RAW_FIELD(C, c)
    YAFF_RAW_FIELD(D, d)
    YAFF_RAW_FIELD(E, e)
    YAFF_RAW_FIELD(F, f)
    YAFF_RAW_FIELD(G, g)
    YAFF_RAW_FIELD(H, h)
    YAFF_RAW_FIELD(I, i)
};
YAFF_LAYOUT_END

template <template <typename> class H>
YAFF_LAYOUT_BEGIN(Intermediate) {
    YAFF_RAW_FIELD(V0, v0)
    YAFF_RAW_FIELD(V1, v1)
    YAFF_RAW_FIELD(V2, v2)
    YAFF_RAW_FIELD(V3, v3)
    YAFF_RAW_FIELD(V4, v4)
    YAFF_RAW_FIELD(V5, v5)
    YAFF_RAW_FIELD(V6, v6)
    YAFF_RAW_FIELD(V7, v7)

    H<Leaf> Leaf_;
    const Leaf& leaf() const {
        return DeRef(Leaf_);
    }
};
YAFF_LAYOUT_END

template <template <typename> class H>
YAFF_LAYOUT_BEGIN(Root) {
    YAFF_RAW_FIELD(I, i)
    YAFF_RAW_FIELD(Ii, ii)
    YAFF_RAW_FIELD(Iii, iii)
    YAFF_RAW_FIELD(Iv, iv)
    YAFF_RAW_FIELD(V, v)
    YAFF_RAW_FIELD(Vi, vi)
    YAFF_RAW_FIELD(Vii, vii)
    YAFF_RAW_FIELD(Viii, viii)

    H<Intermediate<H>> Intermediate_;
    const Intermediate<H>& intermediate() const {
        return DeRef(Intermediate_);
    }
};
YAFF_LAYOUT_END
// clang-format on

#undef YAFF_RAW_FIELD

}  // namespace benchmark_access_raw

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
            const auto leaf = NFlatBuffersBench::CreateLeaf(fbb, values[0], values[1], values[2], values[3], values[4],
                                                            values[5], values[6], values[7], addend);
            const auto intermediate =
                NFlatBuffersBench::CreateIntermediate(fbb, values[8], values[9], values[10], values[11], values[12],
                                                      values[13], values[14], values[15], leaf);
            const auto root =
                NFlatBuffersBench::CreateRoot(fbb, values[16], values[17], values[18], values[19], values[20],
                                              values[21], values[22], values[23], intermediate);
            fbb.Finish(root);

            YAFF_REQUIRE(fbb.GetSize() == FLATBUFFERS_SIZE);
            buffer.append(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
        }

        return buffer;
    }

    template <yaff::MessageLayout Layout, size_t Size>
    std::string GenerateYaFFMessages() {
        static const uint64_t YAFF_SIZE = (Layout == yaff::MessageLayout::MESSAGE_LAYOUT_FLAT ? 227 : 239);
        const uint64_t n = (static_cast<double>(Size) / YAFF_SIZE) + 1;
        const auto& perm = GenerateCyclePermutation(n);

        std::string buffer;
        buffer.append(1, '\0');
        for (uint32_t val : perm) {
            const auto& protoRoot = GenerateProtobuf(val, YAFF_SIZE);

            yaff::Serializer ys;
            ys.EnforceDynamicAlternative(Layout);
            ys.Finish(protoyaff::benchmark_access::SerializeRoot(ys, protoRoot));

            YAFF_REQUIRE(ys.Size() == YAFF_SIZE);
            buffer.append(reinterpret_cast<const char*>(ys.Data()), ys.Size());
        }

        return buffer;
    }

    template <template <typename> class H, size_t Size>
    std::string GenerateRawMessages() {
        using Root = benchmark_access_raw::Root<H>;
        using Intermediate = benchmark_access_raw::Intermediate<H>;
        using Leaf = benchmark_access_raw::Leaf;

        static const uint64_t ADD_SIZE = (std::is_same_v<H<Leaf>, Leaf> ? 0 : sizeof(Intermediate) + sizeof(Leaf));
        static const uint64_t RAW_SIZE = sizeof(Root) + ADD_SIZE;
        const uint64_t n = (static_cast<double>(Size) / RAW_SIZE) + 1;
        const auto& perm = GenerateCyclePermutation(n);

        std::string buffer;
        buffer.resize(n * RAW_SIZE + 1, '\0');
        for (size_t k = 0; k < perm.size(); ++k) {
            const uint32_t val = perm[k];
            const std::vector<uint64_t> values = GenerateRandomVector64(24);

            const int64_t next = val * RAW_SIZE + 1;
            const int64_t addend = next - std::accumulate(values.begin(), values.begin() + 8, 0ULL);

            char* rootBytes = buffer.data() + k * RAW_SIZE + 1;
            auto* root = new (rootBytes) Root();

            const auto intermediateOffset = rootBytes + sizeof(Root);
            auto* intermediate = benchmark_access_raw::ChildAddress(root->Intermediate_, intermediateOffset);
            new (intermediate) Intermediate();

            const auto leafOffset = intermediateOffset + sizeof(Intermediate);
            auto* leaf = benchmark_access_raw::ChildAddress(intermediate->Leaf_, leafOffset);
            new (leaf) Leaf();

            benchmark_access_raw::LinkChild(root->Intermediate_, intermediate);
            benchmark_access_raw::LinkChild(intermediate->Leaf_, leaf);

            leaf->A = values[0];
            leaf->B = values[1];
            leaf->C = values[2];
            leaf->D = values[3];
            leaf->E = values[4];
            leaf->F = values[5];
            leaf->G = values[6];
            leaf->H = values[7];
            leaf->I = addend;

            intermediate->V0 = values[8];
            intermediate->V1 = values[9];
            intermediate->V2 = values[10];
            intermediate->V3 = values[11];
            intermediate->V4 = values[12];
            intermediate->V5 = values[13];
            intermediate->V6 = values[14];
            intermediate->V7 = values[15];

            root->I = values[16];
            root->Ii = values[17];
            root->Iii = values[18];
            root->Iv = values[19];
            root->V = values[20];
            root->Vi = values[21];
            root->Vii = values[22];
            root->Viii = values[23];
        }

        return buffer;
    }

private:
    benchmark_access::Root GenerateProtobuf(uint32_t val, uint64_t size) {
        const std::vector<uint64_t> values = GenerateRandomVector64(24);
        const int64_t next = val * size + 1;
        const int64_t addend = next - std::accumulate(values.begin(), values.begin() + 8, 0ULL);

        benchmark_access::Leaf leaf;
        leaf.set_a(values[0]);
        leaf.set_b(values[1]);
        leaf.set_c(values[2]);
        leaf.set_d(values[3]);
        leaf.set_e(values[4]);
        leaf.set_f(values[5]);
        leaf.set_g(values[6]);
        leaf.set_h(values[7]);
        leaf.set_i(addend);

        benchmark_access::Intermediate intermediate;
        intermediate.set_v0(values[8]);
        intermediate.set_v1(values[9]);
        intermediate.set_v2(values[10]);
        intermediate.set_v3(values[11]);
        intermediate.set_v4(values[12]);
        intermediate.set_v5(values[13]);
        intermediate.set_v6(values[14]);
        intermediate.set_v7(values[15]);
        *intermediate.mutable_leaf() = leaf;

        benchmark_access::Root root;
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
uint64_t SumHierarchicalFlatBuffers(const NFlatBuffersBench::Root* root) {
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

template <template <typename> class H, uint64_t Size, bool CacheChains, bool InterruptChain>
void BM_Access_Hierarchical_Raw(benchmark::State& state) {
    using Root = benchmark_access_raw::Root<H>;

    auto gen = TDataGenerator(std::random_device{}());
    const auto msgs = gen.GenerateRawMessages<H, Size>();

    volatile uint32_t next = 1;
    for (auto _ : state) {
        const auto* root = reinterpret_cast<const Root*>(msgs.data() + next);
        next = SumHierarchicalProtoYaFF<Root, CacheChains, InterruptChain>(*root);
    }

    state.counters["object_size"] =
        benchmark::Counter(msgs.size(), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

template <uint64_t Size, bool CacheChains, bool InterruptChain>
void BM_Access_Hierarchical_Protobuf(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msgs = gen.GenerateProtobufMessages<Size>();

    volatile uint32_t next = 1;
    for (auto _ : state) {
        const uint32_t size = yaff::ReadValue<uint32_t>(msgs.data() + next);

        benchmark_access::Root root;
        std::ignore = root.ParseFromArray(msgs.data() + next + sizeof(size), size);
        next = SumHierarchicalProtoYaFF<benchmark_access::Root, CacheChains, InterruptChain>(root);
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
        const auto* root = flatbuffers::GetRoot<NFlatBuffersBench::Root>(msgs.data() + next);
        next = SumHierarchicalFlatBuffers<CacheChains, InterruptChain>(root);
    }

    state.counters["object_size"] =
        benchmark::Counter(msgs.size(), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

template <yaff::MessageLayout Layout, uint64_t Size, bool CacheChains, bool InterruptChain>
void BM_Access_Hierarchical_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msgs = gen.GenerateYaFFMessages<Layout, Size>();

    volatile uint32_t next = 1;
    for (auto _ : state) {
        const auto& root = yaff::ReadRoot<protoyaff::benchmark_access::Root>(msgs.data() + next);
        next = SumHierarchicalProtoYaFF<protoyaff::benchmark_access::Root, CacheChains, InterruptChain>(root);
    }

    state.counters["object_size"] =
        benchmark::Counter(msgs.size(), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

// Using template args (instead of Arg) to set custom understandable names, as well as a user-friendly order in the
// report.
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Inline, 2048, false, false)
    ->Name("BM_Access_Hierarchical_Raw_Inline/HotAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Inline, 2048, false, true)
    ->Name("BM_Access_Hierarchical_Raw_Inline/HotAccess/CacheChains:false/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Inline, 2048, true, false)
    ->Name("BM_Access_Hierarchical_Raw_Inline/HotAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Inline, 2048, true, true)
    ->Name("BM_Access_Hierarchical_Raw_Inline/HotAccess/CacheChains:true/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Pointer, 2048, false, false)
    ->Name("BM_Access_Hierarchical_Raw_Pointer/HotAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Pointer, 2048, false, true)
    ->Name("BM_Access_Hierarchical_Raw_Pointer/HotAccess/CacheChains:false/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Pointer, 2048, true, false)
    ->Name("BM_Access_Hierarchical_Raw_Pointer/HotAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Pointer, 2048, true, true)
    ->Name("BM_Access_Hierarchical_Raw_Pointer/HotAccess/CacheChains:true/Modification:true");
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
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT, 2048, false, false)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/HotAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT, 2048, false, true)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/HotAccess/CacheChains:false/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT, 2048, true, false)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/HotAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT, 2048, true, true)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/HotAccess/CacheChains:true/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE, 2048, false, false)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/HotAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE, 2048, false, true)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/HotAccess/CacheChains:false/Modification:true");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE, 2048, true, false)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/HotAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE, 2048, true, true)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/HotAccess/CacheChains:true/Modification:true");

BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Inline, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_Raw_Inline/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Inline, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_Raw_Inline/ColdAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Pointer, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_Raw_Pointer/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Raw, benchmark_access_raw::Pointer, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_Raw_Pointer/ColdAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Protobuf, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_Protobuf/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_Protobuf, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_Protobuf/ColdAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_FlatBuffers, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_FlatBuffers/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_FlatBuffers, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_FlatBuffers/ColdAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_YaFF/FlatLayout/ColdAccess/CacheChains:true/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE, 536'870'912, false, false)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/ColdAccess/CacheChains:false/Modification:false");
BENCHMARK_TEMPLATE(BM_Access_Hierarchical_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE, 536'870'912, true, false)
    ->Name("BM_Access_Hierarchical_YaFF/SparseLayout/ColdAccess/CacheChains:true/Modification:false");
