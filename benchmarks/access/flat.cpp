#include <benchmark/benchmark.h>

#include <random>

#include "flatbuffers/flat_generated.h"
#include "protoyaff/flat.pb.h"
#include "protoyaff/flat.yaff.h"

class TDataGenerator {
public:
    struct TProtobufMessage {
        std::string Buffer;
        std::vector<uint64_t> Values;
    };

    struct TFlatBuffersMessage {
        flatbuffers::DetachedBuffer Buffer;
        std::vector<uint64_t> Values;
    };

    struct TYaFFMessage {
        NYaFF::TDetachedBuffer Buffer;
        std::vector<uint64_t> Values;
    };

public:
    explicit TDataGenerator(uint64_t seed) : Rng_(seed) {
    }

    TProtobufMessage GenerateProtobufFlat10() {
        std::vector<uint64_t> values = GenerateRandomVector64(10);

        NYaFFBench::TFlat10 proto;
        proto.set_v0(values[0]);
        proto.set_v1(values[1]);
        proto.set_v2(values[2]);
        proto.set_v3(values[3]);
        proto.set_v4(values[4]);
        proto.set_v5(values[5]);
        proto.set_v6(values[6]);
        proto.set_v7(values[7]);
        proto.set_v8(values[8]);
        proto.set_v9(values[9]);

        return {
            .Buffer = proto.SerializeAsString(),
            .Values = std::move(values),
        };
    }

    TProtobufMessage GenerateProtobufFlat100() {
        std::vector<uint64_t> values = GenerateRandomVector64(100);

        NYaFFBench::TFlat100 proto;
        proto.set_v0(values[0]);
        proto.set_v1(values[1]);
        proto.set_v2(values[2]);
        proto.set_v3(values[3]);
        proto.set_v4(values[4]);
        proto.set_v5(values[5]);
        proto.set_v6(values[6]);
        proto.set_v7(values[7]);
        proto.set_v8(values[8]);
        proto.set_v9(values[9]);
        proto.set_v10(values[10]);
        proto.set_v11(values[11]);
        proto.set_v12(values[12]);
        proto.set_v13(values[13]);
        proto.set_v14(values[14]);
        proto.set_v15(values[15]);
        proto.set_v16(values[16]);
        proto.set_v17(values[17]);
        proto.set_v18(values[18]);
        proto.set_v19(values[19]);
        proto.set_v20(values[20]);
        proto.set_v21(values[21]);
        proto.set_v22(values[22]);
        proto.set_v23(values[23]);
        proto.set_v24(values[24]);
        proto.set_v25(values[25]);
        proto.set_v26(values[26]);
        proto.set_v27(values[27]);
        proto.set_v28(values[28]);
        proto.set_v29(values[29]);
        proto.set_v30(values[30]);
        proto.set_v31(values[31]);
        proto.set_v32(values[32]);
        proto.set_v33(values[33]);
        proto.set_v34(values[34]);
        proto.set_v35(values[35]);
        proto.set_v36(values[36]);
        proto.set_v37(values[37]);
        proto.set_v38(values[38]);
        proto.set_v39(values[39]);
        proto.set_v40(values[40]);
        proto.set_v41(values[41]);
        proto.set_v42(values[42]);
        proto.set_v43(values[43]);
        proto.set_v44(values[44]);
        proto.set_v45(values[45]);
        proto.set_v46(values[46]);
        proto.set_v47(values[47]);
        proto.set_v48(values[48]);
        proto.set_v49(values[49]);
        proto.set_v50(values[50]);
        proto.set_v51(values[51]);
        proto.set_v52(values[52]);
        proto.set_v53(values[53]);
        proto.set_v54(values[54]);
        proto.set_v55(values[55]);
        proto.set_v56(values[56]);
        proto.set_v57(values[57]);
        proto.set_v58(values[58]);
        proto.set_v59(values[59]);
        proto.set_v60(values[60]);
        proto.set_v61(values[61]);
        proto.set_v62(values[62]);
        proto.set_v63(values[63]);
        proto.set_v64(values[64]);
        proto.set_v65(values[65]);
        proto.set_v66(values[66]);
        proto.set_v67(values[67]);
        proto.set_v68(values[68]);
        proto.set_v69(values[69]);
        proto.set_v70(values[70]);
        proto.set_v71(values[71]);
        proto.set_v72(values[72]);
        proto.set_v73(values[73]);
        proto.set_v74(values[74]);
        proto.set_v75(values[75]);
        proto.set_v76(values[76]);
        proto.set_v77(values[77]);
        proto.set_v78(values[78]);
        proto.set_v79(values[79]);
        proto.set_v80(values[80]);
        proto.set_v81(values[81]);
        proto.set_v82(values[82]);
        proto.set_v83(values[83]);
        proto.set_v84(values[84]);
        proto.set_v85(values[85]);
        proto.set_v86(values[86]);
        proto.set_v87(values[87]);
        proto.set_v88(values[88]);
        proto.set_v89(values[89]);
        proto.set_v90(values[90]);
        proto.set_v91(values[91]);
        proto.set_v92(values[92]);
        proto.set_v93(values[93]);
        proto.set_v94(values[94]);
        proto.set_v95(values[95]);
        proto.set_v96(values[96]);
        proto.set_v97(values[97]);
        proto.set_v98(values[98]);
        proto.set_v99(values[99]);

        return {
            .Buffer = proto.SerializeAsString(),
            .Values = std::move(values),
        };
    }

    TFlatBuffersMessage GenerateFlatBuffersFlat10() {
        std::vector<uint64_t> values = GenerateRandomVector64(10);

        flatbuffers::FlatBufferBuilder fbb;
        const auto root = NFlatBuffersBench::CreateTFlat10(fbb, values[0], values[1], values[2], values[3], values[4],
                                                           values[5], values[6], values[7], values[8], values[9]);
        fbb.Finish(root);

        return {
            .Buffer = fbb.Release(),
            .Values = std::move(values),
        };
    }

    TFlatBuffersMessage GenerateFlatBuffersFlat100() {
        std::vector<uint64_t> values = GenerateRandomVector64(100);

        flatbuffers::FlatBufferBuilder fbb;
        const auto root = NFlatBuffersBench::CreateTFlat100(
            fbb, values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8],
            values[9], values[10], values[11], values[12], values[13], values[14], values[15], values[16], values[17],
            values[18], values[19], values[20], values[21], values[22], values[23], values[24], values[25], values[26],
            values[27], values[28], values[29], values[30], values[31], values[32], values[33], values[34], values[35],
            values[36], values[37], values[38], values[39], values[40], values[41], values[42], values[43], values[44],
            values[45], values[46], values[47], values[48], values[49], values[50], values[51], values[52], values[53],
            values[54], values[55], values[56], values[57], values[58], values[59], values[60], values[61], values[62],
            values[63], values[64], values[65], values[66], values[67], values[68], values[69], values[70], values[71],
            values[72], values[73], values[74], values[75], values[76], values[77], values[78], values[79], values[80],
            values[81], values[82], values[83], values[84], values[85], values[86], values[87], values[88], values[89],
            values[90], values[91], values[92], values[93], values[94], values[95], values[96], values[97], values[98],
            values[99]);
        fbb.Finish(root);

        return {
            .Buffer = fbb.Release(),
            .Values = std::move(values),
        };
    }

    template <NYaFF::EMessageLayout Layout>
    TYaFFMessage GenerateYaFFFlat10() {
        TProtobufMessage protoMessage = GenerateProtobufFlat10();

        NYaFFBench::TFlat10 proto;
        std::ignore = proto.ParseFromString(protoMessage.Buffer);

        NYaFF::TBuilder yffb;
        yffb.EnforceDynamicAlternative(Layout);
        const auto root = NProtoYaFF::NYaFFBench::CreateTFlat10(yffb, proto);
        yffb.Finish(root);

        return {
            .Buffer = yffb.Release(),
            .Values = std::move(protoMessage.Values),
        };
    }

    template <NYaFF::EMessageLayout Layout>
    TYaFFMessage GenerateYaFFFlat100() {
        TProtobufMessage protoMessage = GenerateProtobufFlat100();

        NYaFFBench::TFlat100 proto;
        std::ignore = proto.ParseFromString(protoMessage.Buffer);

        NYaFF::TBuilder yffb;
        yffb.EnforceDynamicAlternative(Layout);
        const auto root = NProtoYaFF::NYaFFBench::CreateTFlat100(yffb, proto);
        yffb.Finish(root);

        return {
            .Buffer = yffb.Release(),
            .Values = std::move(protoMessage.Values),
        };
    }

private:
    std::vector<uint64_t> GenerateRandomVector64(size_t n) {
        std::uniform_int_distribution<uint64_t> dist(0, ((1ULL << 48) - 1));

        std::vector<uint64_t> result;
        result.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            result.emplace_back(dist(Rng_));
        }
        return result;
    }

    std::mt19937 Rng_;
};

template <typename T>
uint64_t SumRawFlat10(const T& root) {
    uint64_t sum = 0;
    sum += root.template ReadValue<uint64_t>(1, 0);
    sum += root.template ReadValue<uint64_t>(2, 0);
    sum += root.template ReadValue<uint64_t>(3, 0);
    sum += root.template ReadValue<uint64_t>(4, 0);
    // Field 5 is removed;
    sum += root.template ReadValue<uint64_t>(6, 0);
    sum += root.template ReadValue<uint64_t>(7, 0);
    sum += root.template ReadValue<uint64_t>(8, 0);
    sum += root.template ReadValue<uint64_t>(9, 0);
    sum += root.template ReadValue<uint64_t>(10, 0);
    return sum;
}

template <typename T>
uint64_t SumFlat10(const T& root) {
    uint64_t sum = 0;
    sum += root.v0();
    sum += root.v1();
    sum += root.v2();
    sum += root.v3();
    sum += root.v4();
    sum += root.v5();
    sum += root.v6();
    sum += root.v7();
    sum += root.v8();
    sum += root.v9();
    return sum;
}

template <typename T>
uint64_t SumFlat100(const T& root) {
    uint64_t sum = 0;
    sum += root.v0();
    sum += root.v1();
    sum += root.v2();
    sum += root.v3();
    sum += root.v4();
    sum += root.v5();
    sum += root.v6();
    sum += root.v7();
    sum += root.v8();
    sum += root.v9();
    sum += root.v10();
    sum += root.v11();
    sum += root.v12();
    sum += root.v13();
    sum += root.v14();
    sum += root.v15();
    sum += root.v16();
    sum += root.v17();
    sum += root.v18();
    sum += root.v19();
    sum += root.v20();
    sum += root.v21();
    sum += root.v22();
    sum += root.v23();
    sum += root.v24();
    sum += root.v25();
    sum += root.v26();
    sum += root.v27();
    sum += root.v28();
    sum += root.v29();
    sum += root.v30();
    sum += root.v31();
    sum += root.v32();
    sum += root.v33();
    sum += root.v34();
    sum += root.v35();
    sum += root.v36();
    sum += root.v37();
    sum += root.v38();
    sum += root.v39();
    sum += root.v40();
    sum += root.v41();
    sum += root.v42();
    sum += root.v43();
    sum += root.v44();
    sum += root.v45();
    sum += root.v46();
    sum += root.v47();
    sum += root.v48();
    sum += root.v49();
    sum += root.v50();
    sum += root.v51();
    sum += root.v52();
    sum += root.v53();
    sum += root.v54();
    sum += root.v55();
    sum += root.v56();
    sum += root.v57();
    sum += root.v58();
    sum += root.v59();
    sum += root.v60();
    sum += root.v61();
    sum += root.v62();
    sum += root.v63();
    sum += root.v64();
    sum += root.v65();
    sum += root.v66();
    sum += root.v67();
    sum += root.v68();
    sum += root.v69();
    sum += root.v70();
    sum += root.v71();
    sum += root.v72();
    sum += root.v73();
    sum += root.v74();
    sum += root.v75();
    sum += root.v76();
    sum += root.v77();
    sum += root.v78();
    sum += root.v79();
    sum += root.v80();
    sum += root.v81();
    sum += root.v82();
    sum += root.v83();
    sum += root.v84();
    sum += root.v85();
    sum += root.v86();
    sum += root.v87();
    sum += root.v88();
    sum += root.v89();
    sum += root.v90();
    sum += root.v91();
    sum += root.v92();
    sum += root.v93();
    sum += root.v94();
    sum += root.v95();
    sum += root.v96();
    sum += root.v97();
    sum += root.v98();
    sum += root.v99();
    return sum;
}

void BM_Access_Flat10_Protobuf(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateProtobufFlat10();

    for (auto _ : state) {
        NYaFFBench::TFlat10 proto;
        std::ignore = proto.ParseFromString(msg.Buffer);
        uint64_t sum = SumFlat10(proto);
        benchmark::DoNotOptimize(sum);
    }
}

void BM_Access_Flat100_Protobuf(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateProtobufFlat100();

    for (auto _ : state) {
        NYaFFBench::TFlat100 proto;
        std::ignore = proto.ParseFromString(msg.Buffer);
        uint64_t sum = SumFlat100(proto);
        benchmark::DoNotOptimize(sum);
    }
}

void BM_Access_Flat10_FlatBuffers(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateFlatBuffersFlat10();

    for (auto _ : state) {
        const auto* root = flatbuffers::GetRoot<NFlatBuffersBench::TFlat10>(msg.Buffer.data());
        uint64_t sum = SumFlat10<NFlatBuffersBench::TFlat10>(*root);
        benchmark::DoNotOptimize(sum);
    }
}

void BM_Access_Flat100_FlatBuffers(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateFlatBuffersFlat100();

    for (auto _ : state) {
        const auto* root = flatbuffers::GetRoot<NFlatBuffersBench::TFlat100>(msg.Buffer.data());
        uint64_t sum = SumFlat100<NFlatBuffersBench::TFlat100>(*root);
        benchmark::DoNotOptimize(sum);
    }
}

template <NYaFF::EMessageLayout Layout>
void BM_Access_Flat10_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateYaFFFlat10<Layout>();

    for (auto _ : state) {
        const auto& root = NYaFF::ReadRoot<NProtoYaFF::NYaFFBench::TFlat10>(msg.Buffer.Data());
        uint64_t sum = SumFlat10<NProtoYaFF::NYaFFBench::TFlat10>(root);
        benchmark::DoNotOptimize(sum);
    }
}

struct TFlat10MetaV2 {
    static constexpr std::array<NYaFF::TFieldOffset, 11> FLAT_OFFSETS = {0, 8, 16, 24, 32, 32, 40, 48, 56, 64, 72};
    static constexpr std::array<NYaFF::TFieldId, 1> DELETED_IDS = {5};
    static constexpr std::array<bool, 10> STATIC_FLAGS = {1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
};

void BM_Access_Flat10V2_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateYaFFFlat10<NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT>();

    for (auto _ : state) {
        const auto& root = NYaFF::ReadRoot<NYaFF::TDynamicMessage<TFlat10MetaV2>>(msg.Buffer.Data());
        uint64_t sum = SumRawFlat10<NYaFF::TDynamicMessage<TFlat10MetaV2>>(root);
        benchmark::DoNotOptimize(sum);
    }
}

template <NYaFF::EMessageLayout Layout>
void BM_Access_Flat100_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateYaFFFlat100<Layout>();

    for (auto _ : state) {
        const auto& root = NYaFF::ReadRoot<NProtoYaFF::NYaFFBench::TFlat100>(msg.Buffer.Data());
        uint64_t sum = SumFlat100<NProtoYaFF::NYaFFBench::TFlat100>(root);
        benchmark::DoNotOptimize(sum);
    }
}

BENCHMARK(BM_Access_Flat10_Protobuf)->Name("BM_Access_Flat_Protobuf/FieldCount:10");
BENCHMARK(BM_Access_Flat10_FlatBuffers)->Name("BM_Access_Flat_FlatBuffers/FieldCount:10");
BENCHMARK_TEMPLATE(BM_Access_Flat10_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT)
    ->Name("BM_Access_Flat_YaFF/FlatLayout/FieldCount:10");
BENCHMARK_TEMPLATE(BM_Access_Flat10_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE)
    ->Name("BM_Access_Flat_YaFF/SparseLayout/FieldCount:10");
BENCHMARK(BM_Access_Flat10V2_YaFF)->Name("BM_Access_Flat_YaFF/FlatLayout/CompatibilityMode/FieldCount:10");

BENCHMARK(BM_Access_Flat100_Protobuf)->Name("BM_Access_Flat_Protobuf/FieldCount:100");
BENCHMARK(BM_Access_Flat100_FlatBuffers)->Name("BM_Access_Flat_FlatBuffers/FieldCount:100");
BENCHMARK_TEMPLATE(BM_Access_Flat100_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_FLAT)
    ->Name("BM_Access_Flat_YaFF/FlatLayout/FieldCount:100");
BENCHMARK_TEMPLATE(BM_Access_Flat100_YaFF, NYaFF::EMessageLayout::MESSAGE_LAYOUT_SPARSE)
    ->Name("BM_Access_Flat_YaFF/SparseLayout/FieldCount:100");
