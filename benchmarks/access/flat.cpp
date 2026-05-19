#include <benchmark/benchmark.h>

#include <random>

#include "flatbuffers/flat_generated.h"
#include "protoyaff/flat.pb.h"
#include "protoyaff/flat.yaff.h"

namespace benchmark_access_raw {

#define YAFF_RAW_FIELD(i)   \
    uint64_t V##i = 0;      \
    uint64_t v##i() const { \
        return V##i;        \
    }

// N.B.: YAFF_LAYOUT_BEGIN is only needed to legalize aliasing
// for structures and has no effect on the implementation.
//
// When implementing a protocol on raw C++ structures,
// you will still need to do something similar to mitigate the UB.

// clang-format off
YAFF_LAYOUT_BEGIN(Flat10) {
    YAFF_RAW_FIELD(0) YAFF_RAW_FIELD(1) YAFF_RAW_FIELD(2) YAFF_RAW_FIELD(3) YAFF_RAW_FIELD(4)
    YAFF_RAW_FIELD(5) YAFF_RAW_FIELD(6) YAFF_RAW_FIELD(7) YAFF_RAW_FIELD(8) YAFF_RAW_FIELD(9)
};
YAFF_LAYOUT_END

YAFF_LAYOUT_BEGIN(Flat100) {
    YAFF_RAW_FIELD(0)  YAFF_RAW_FIELD(1)  YAFF_RAW_FIELD(2)  YAFF_RAW_FIELD(3)  YAFF_RAW_FIELD(4)
    YAFF_RAW_FIELD(5)  YAFF_RAW_FIELD(6)  YAFF_RAW_FIELD(7)  YAFF_RAW_FIELD(8)  YAFF_RAW_FIELD(9)
    YAFF_RAW_FIELD(10) YAFF_RAW_FIELD(11) YAFF_RAW_FIELD(12) YAFF_RAW_FIELD(13) YAFF_RAW_FIELD(14)
    YAFF_RAW_FIELD(15) YAFF_RAW_FIELD(16) YAFF_RAW_FIELD(17) YAFF_RAW_FIELD(18) YAFF_RAW_FIELD(19)
    YAFF_RAW_FIELD(20) YAFF_RAW_FIELD(21) YAFF_RAW_FIELD(22) YAFF_RAW_FIELD(23) YAFF_RAW_FIELD(24)
    YAFF_RAW_FIELD(25) YAFF_RAW_FIELD(26) YAFF_RAW_FIELD(27) YAFF_RAW_FIELD(28) YAFF_RAW_FIELD(29)
    YAFF_RAW_FIELD(30) YAFF_RAW_FIELD(31) YAFF_RAW_FIELD(32) YAFF_RAW_FIELD(33) YAFF_RAW_FIELD(34)
    YAFF_RAW_FIELD(35) YAFF_RAW_FIELD(36) YAFF_RAW_FIELD(37) YAFF_RAW_FIELD(38) YAFF_RAW_FIELD(39)
    YAFF_RAW_FIELD(40) YAFF_RAW_FIELD(41) YAFF_RAW_FIELD(42) YAFF_RAW_FIELD(43) YAFF_RAW_FIELD(44)
    YAFF_RAW_FIELD(45) YAFF_RAW_FIELD(46) YAFF_RAW_FIELD(47) YAFF_RAW_FIELD(48) YAFF_RAW_FIELD(49)
    YAFF_RAW_FIELD(50) YAFF_RAW_FIELD(51) YAFF_RAW_FIELD(52) YAFF_RAW_FIELD(53) YAFF_RAW_FIELD(54)
    YAFF_RAW_FIELD(55) YAFF_RAW_FIELD(56) YAFF_RAW_FIELD(57) YAFF_RAW_FIELD(58) YAFF_RAW_FIELD(59)
    YAFF_RAW_FIELD(60) YAFF_RAW_FIELD(61) YAFF_RAW_FIELD(62) YAFF_RAW_FIELD(63) YAFF_RAW_FIELD(64)
    YAFF_RAW_FIELD(65) YAFF_RAW_FIELD(66) YAFF_RAW_FIELD(67) YAFF_RAW_FIELD(68) YAFF_RAW_FIELD(69)
    YAFF_RAW_FIELD(70) YAFF_RAW_FIELD(71) YAFF_RAW_FIELD(72) YAFF_RAW_FIELD(73) YAFF_RAW_FIELD(74)
    YAFF_RAW_FIELD(75) YAFF_RAW_FIELD(76) YAFF_RAW_FIELD(77) YAFF_RAW_FIELD(78) YAFF_RAW_FIELD(79)
    YAFF_RAW_FIELD(80) YAFF_RAW_FIELD(81) YAFF_RAW_FIELD(82) YAFF_RAW_FIELD(83) YAFF_RAW_FIELD(84)
    YAFF_RAW_FIELD(85) YAFF_RAW_FIELD(86) YAFF_RAW_FIELD(87) YAFF_RAW_FIELD(88) YAFF_RAW_FIELD(89)
    YAFF_RAW_FIELD(90) YAFF_RAW_FIELD(91) YAFF_RAW_FIELD(92) YAFF_RAW_FIELD(93) YAFF_RAW_FIELD(94)
    YAFF_RAW_FIELD(95) YAFF_RAW_FIELD(96) YAFF_RAW_FIELD(97) YAFF_RAW_FIELD(98) YAFF_RAW_FIELD(99)
};
YAFF_LAYOUT_END
// clang-format on

#undef YAFF_RAW_FIELD

}  // namespace benchmark_access_raw

class TDataGenerator {
public:
    struct ProtobufMessage {
        std::string Buffer;
        std::vector<uint64_t> Values;
    };

    struct FlatBuffersMessage {
        flatbuffers::DetachedBuffer Buffer;
        std::vector<uint64_t> Values;
    };

    struct YaFFMessage {
        yaff::DetachedBuffer Buffer;
        std::vector<uint64_t> Values;
    };

    struct RawMessage {
        std::string Buffer;
        std::vector<uint64_t> Values;
    };

public:
    explicit TDataGenerator(uint64_t seed) : Rng_(seed) {
    }

    ProtobufMessage GenerateProtobufFlat10() {
        std::vector<uint64_t> values = GenerateRandomVector64(10);

        benchmark_access::Flat10 proto;
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

    ProtobufMessage GenerateProtobufFlat100() {
        std::vector<uint64_t> values = GenerateRandomVector64(100);

        benchmark_access::Flat100 proto;
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

    FlatBuffersMessage GenerateFlatBuffersFlat10() {
        std::vector<uint64_t> values = GenerateRandomVector64(10);

        flatbuffers::FlatBufferBuilder fbb;
        const auto root = NFlatBuffersBench::CreateFlat10(fbb, values[0], values[1], values[2], values[3], values[4],
                                                          values[5], values[6], values[7], values[8], values[9]);
        fbb.Finish(root);

        return {
            .Buffer = fbb.Release(),
            .Values = std::move(values),
        };
    }

    FlatBuffersMessage GenerateFlatBuffersFlat100() {
        std::vector<uint64_t> values = GenerateRandomVector64(100);

        flatbuffers::FlatBufferBuilder fbb;
        const auto root = NFlatBuffersBench::CreateFlat100(
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

    template <yaff::MessageLayout Layout>
    YaFFMessage GenerateYaFFFlat10() {
        ProtobufMessage protoMessage = GenerateProtobufFlat10();

        benchmark_access::Flat10 proto;
        std::ignore = proto.ParseFromString(protoMessage.Buffer);

        yaff::Serializer ys;
        ys.EnforceDynamicAlternative(Layout);
        const auto root = protoyaff::benchmark_access::SerializeFlat10(ys, proto);
        ys.Finish(root);

        return {
            .Buffer = ys.Release(),
            .Values = std::move(protoMessage.Values),
        };
    }

    template <yaff::MessageLayout Layout>
    YaFFMessage GenerateYaFFFlat100() {
        ProtobufMessage protoMessage = GenerateProtobufFlat100();

        benchmark_access::Flat100 proto;
        std::ignore = proto.ParseFromString(protoMessage.Buffer);

        yaff::Serializer ys;
        ys.EnforceDynamicAlternative(Layout);
        const auto root = protoyaff::benchmark_access::SerializeFlat100(ys, proto);
        ys.Finish(root);

        return {
            .Buffer = ys.Release(),
            .Values = std::move(protoMessage.Values),
        };
    }

    RawMessage GenerateRawFlat10() {
        std::vector<uint64_t> values = GenerateRandomVector64(10);

        benchmark_access_raw::Flat10 raw;
        raw.V0 = values[0];
        raw.V1 = values[1];
        raw.V2 = values[2];
        raw.V3 = values[3];
        raw.V4 = values[4];
        raw.V5 = values[5];
        raw.V6 = values[6];
        raw.V7 = values[7];
        raw.V8 = values[8];
        raw.V9 = values[9];

        std::string buffer(reinterpret_cast<const char*>(&raw), sizeof(raw));
        return {
            .Buffer = std::move(buffer),
            .Values = std::move(values),
        };
    }

    RawMessage GenerateRawFlat100() {
        std::vector<uint64_t> values = GenerateRandomVector64(100);

        benchmark_access_raw::Flat100 raw;
        raw.V0 = values[0];
        raw.V1 = values[1];
        raw.V2 = values[2];
        raw.V3 = values[3];
        raw.V4 = values[4];
        raw.V5 = values[5];
        raw.V6 = values[6];
        raw.V7 = values[7];
        raw.V8 = values[8];
        raw.V9 = values[9];
        raw.V10 = values[10];
        raw.V11 = values[11];
        raw.V12 = values[12];
        raw.V13 = values[13];
        raw.V14 = values[14];
        raw.V15 = values[15];
        raw.V16 = values[16];
        raw.V17 = values[17];
        raw.V18 = values[18];
        raw.V19 = values[19];
        raw.V20 = values[20];
        raw.V21 = values[21];
        raw.V22 = values[22];
        raw.V23 = values[23];
        raw.V24 = values[24];
        raw.V25 = values[25];
        raw.V26 = values[26];
        raw.V27 = values[27];
        raw.V28 = values[28];
        raw.V29 = values[29];
        raw.V30 = values[30];
        raw.V31 = values[31];
        raw.V32 = values[32];
        raw.V33 = values[33];
        raw.V34 = values[34];
        raw.V35 = values[35];
        raw.V36 = values[36];
        raw.V37 = values[37];
        raw.V38 = values[38];
        raw.V39 = values[39];
        raw.V40 = values[40];
        raw.V41 = values[41];
        raw.V42 = values[42];
        raw.V43 = values[43];
        raw.V44 = values[44];
        raw.V45 = values[45];
        raw.V46 = values[46];
        raw.V47 = values[47];
        raw.V48 = values[48];
        raw.V49 = values[49];
        raw.V50 = values[50];
        raw.V51 = values[51];
        raw.V52 = values[52];
        raw.V53 = values[53];
        raw.V54 = values[54];
        raw.V55 = values[55];
        raw.V56 = values[56];
        raw.V57 = values[57];
        raw.V58 = values[58];
        raw.V59 = values[59];
        raw.V60 = values[60];
        raw.V61 = values[61];
        raw.V62 = values[62];
        raw.V63 = values[63];
        raw.V64 = values[64];
        raw.V65 = values[65];
        raw.V66 = values[66];
        raw.V67 = values[67];
        raw.V68 = values[68];
        raw.V69 = values[69];
        raw.V70 = values[70];
        raw.V71 = values[71];
        raw.V72 = values[72];
        raw.V73 = values[73];
        raw.V74 = values[74];
        raw.V75 = values[75];
        raw.V76 = values[76];
        raw.V77 = values[77];
        raw.V78 = values[78];
        raw.V79 = values[79];
        raw.V80 = values[80];
        raw.V81 = values[81];
        raw.V82 = values[82];
        raw.V83 = values[83];
        raw.V84 = values[84];
        raw.V85 = values[85];
        raw.V86 = values[86];
        raw.V87 = values[87];
        raw.V88 = values[88];
        raw.V89 = values[89];
        raw.V90 = values[90];
        raw.V91 = values[91];
        raw.V92 = values[92];
        raw.V93 = values[93];
        raw.V94 = values[94];
        raw.V95 = values[95];
        raw.V96 = values[96];
        raw.V97 = values[97];
        raw.V98 = values[98];
        raw.V99 = values[99];

        std::string buffer(reinterpret_cast<const char*>(&raw), sizeof(raw));
        return {
            .Buffer = std::move(buffer),
            .Values = std::move(values),
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

void BM_Access_Flat10_Raw(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateRawFlat10();

    for (auto _ : state) {
        const auto* root = reinterpret_cast<const benchmark_access_raw::Flat10*>(msg.Buffer.data());
        uint64_t sum = SumFlat10<benchmark_access_raw::Flat10>(*root);
        benchmark::DoNotOptimize(sum);
    }
}
void BM_Access_Flat100_Raw(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateRawFlat100();

    for (auto _ : state) {
        const auto* root = reinterpret_cast<const benchmark_access_raw::Flat100*>(msg.Buffer.data());
        uint64_t sum = SumFlat100<benchmark_access_raw::Flat100>(*root);
        benchmark::DoNotOptimize(sum);
    }
}

void BM_Access_Flat10_Protobuf(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateProtobufFlat10();

    for (auto _ : state) {
        benchmark_access::Flat10 proto;
        std::ignore = proto.ParseFromString(msg.Buffer);
        uint64_t sum = SumFlat10(proto);
        benchmark::DoNotOptimize(sum);
    }
}

void BM_Access_Flat100_Protobuf(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateProtobufFlat100();

    for (auto _ : state) {
        benchmark_access::Flat100 proto;
        std::ignore = proto.ParseFromString(msg.Buffer);
        uint64_t sum = SumFlat100(proto);
        benchmark::DoNotOptimize(sum);
    }
}

void BM_Access_Flat10_FlatBuffers(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateFlatBuffersFlat10();

    for (auto _ : state) {
        const auto* root = flatbuffers::GetRoot<NFlatBuffersBench::Flat10>(msg.Buffer.data());
        uint64_t sum = SumFlat10<NFlatBuffersBench::Flat10>(*root);
        benchmark::DoNotOptimize(sum);
    }
}

void BM_Access_Flat100_FlatBuffers(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateFlatBuffersFlat100();

    for (auto _ : state) {
        const auto* root = flatbuffers::GetRoot<NFlatBuffersBench::Flat100>(msg.Buffer.data());
        uint64_t sum = SumFlat100<NFlatBuffersBench::Flat100>(*root);
        benchmark::DoNotOptimize(sum);
    }
}

template <yaff::MessageLayout Layout>
void BM_Access_Flat10_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateYaFFFlat10<Layout>();

    for (auto _ : state) {
        const auto& root = yaff::ReadRoot<protoyaff::benchmark_access::Flat10>(msg.Buffer.Data());
        uint64_t sum = SumFlat10<protoyaff::benchmark_access::Flat10>(root);
        benchmark::DoNotOptimize(sum);
    }
}

struct Flat10MetaV2 {
    static constexpr std::array<yaff::FieldOffset, 11> FLAT_OFFSETS = {0, 8, 16, 24, 32, 32, 40, 48, 56, 64, 72};
    static constexpr std::array<yaff::FieldId, 1> DELETED_IDS = {5};
    static constexpr std::array<bool, 10> STATIC_FLAGS = {1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
};

void BM_Access_Flat10V2_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateYaFFFlat10<yaff::MessageLayout::MESSAGE_LAYOUT_FLAT>();

    for (auto _ : state) {
        const auto& root = yaff::ReadRoot<yaff::DynamicMessage<Flat10MetaV2>>(msg.Buffer.Data());
        uint64_t sum = SumRawFlat10<yaff::DynamicMessage<Flat10MetaV2>>(root);
        benchmark::DoNotOptimize(sum);
    }
}

template <yaff::MessageLayout Layout>
void BM_Access_Flat100_YaFF(benchmark::State& state) {
    auto gen = TDataGenerator(std::random_device{}());
    const auto msg = gen.GenerateYaFFFlat100<Layout>();

    for (auto _ : state) {
        const auto& root = yaff::ReadRoot<protoyaff::benchmark_access::Flat100>(msg.Buffer.Data());
        uint64_t sum = SumFlat100<protoyaff::benchmark_access::Flat100>(root);
        benchmark::DoNotOptimize(sum);
    }
}

BENCHMARK(BM_Access_Flat10_Raw)->Name("BM_Access_Flat_Raw/FieldCount:10");
BENCHMARK(BM_Access_Flat10_Protobuf)->Name("BM_Access_Flat_Protobuf/FieldCount:10");
BENCHMARK(BM_Access_Flat10_FlatBuffers)->Name("BM_Access_Flat_FlatBuffers/FieldCount:10");
BENCHMARK_TEMPLATE(BM_Access_Flat10_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT)
    ->Name("BM_Access_Flat_YaFF/FlatLayout/FieldCount:10");
BENCHMARK_TEMPLATE(BM_Access_Flat10_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE)
    ->Name("BM_Access_Flat_YaFF/SparseLayout/FieldCount:10");
BENCHMARK(BM_Access_Flat10V2_YaFF)->Name("BM_Access_Flat_YaFF/FlatLayout/CompatibilityMode/FieldCount:10");

BENCHMARK(BM_Access_Flat100_Raw)->Name("BM_Access_Flat_Raw/FieldCount:100");
BENCHMARK(BM_Access_Flat100_Protobuf)->Name("BM_Access_Flat_Protobuf/FieldCount:100");
BENCHMARK(BM_Access_Flat100_FlatBuffers)->Name("BM_Access_Flat_FlatBuffers/FieldCount:100");
BENCHMARK_TEMPLATE(BM_Access_Flat100_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT)
    ->Name("BM_Access_Flat_YaFF/FlatLayout/FieldCount:100");
BENCHMARK_TEMPLATE(BM_Access_Flat100_YaFF, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE)
    ->Name("BM_Access_Flat_YaFF/SparseLayout/FieldCount:100");
