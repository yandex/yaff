#include <benchmark/benchmark.h>

#include <array>
#include <cstring>
#include <optional>
#include <random>
#include <tuple>
#include <type_traits>
#include <vector>

#include "flatbuffers/flat_generated.h"
#include "protoyaff/flat.pb.h"
#include "protoyaff/flat.yaff.h"

namespace benchmark_serialize_raw {

#define YAFF_RAW_FIELD(i) uint64_t V##i = 0;

// N.B.: YAFF_LAYOUT_BEGIN is only needed to legalize aliasing
// for structures and has no effect on the implementation.
//
// When implementing a protocol on raw C++ structures,
// you will still need to do something similar to mitigate the UB.

// clang-format off
YAFF_LAYOUT_BEGIN(Flat100) {
    YAFF_RAW_FIELD(0)   YAFF_RAW_FIELD(1)   YAFF_RAW_FIELD(2)   YAFF_RAW_FIELD(3)   YAFF_RAW_FIELD(4)
    YAFF_RAW_FIELD(5)   YAFF_RAW_FIELD(6)   YAFF_RAW_FIELD(7)   YAFF_RAW_FIELD(8)   YAFF_RAW_FIELD(9)
    YAFF_RAW_FIELD(10)  YAFF_RAW_FIELD(11)  YAFF_RAW_FIELD(12)  YAFF_RAW_FIELD(13)  YAFF_RAW_FIELD(14)
    YAFF_RAW_FIELD(15)  YAFF_RAW_FIELD(16)  YAFF_RAW_FIELD(17)  YAFF_RAW_FIELD(18)  YAFF_RAW_FIELD(19)
    YAFF_RAW_FIELD(20)  YAFF_RAW_FIELD(21)  YAFF_RAW_FIELD(22)  YAFF_RAW_FIELD(23)  YAFF_RAW_FIELD(24)
    YAFF_RAW_FIELD(25)  YAFF_RAW_FIELD(26)  YAFF_RAW_FIELD(27)  YAFF_RAW_FIELD(28)  YAFF_RAW_FIELD(29)
    YAFF_RAW_FIELD(30)  YAFF_RAW_FIELD(31)  YAFF_RAW_FIELD(32)  YAFF_RAW_FIELD(33)  YAFF_RAW_FIELD(34)
    YAFF_RAW_FIELD(35)  YAFF_RAW_FIELD(36)  YAFF_RAW_FIELD(37)  YAFF_RAW_FIELD(38)  YAFF_RAW_FIELD(39)
    YAFF_RAW_FIELD(40)  YAFF_RAW_FIELD(41)  YAFF_RAW_FIELD(42)  YAFF_RAW_FIELD(43)  YAFF_RAW_FIELD(44)
    YAFF_RAW_FIELD(45)  YAFF_RAW_FIELD(46)  YAFF_RAW_FIELD(47)  YAFF_RAW_FIELD(48)  YAFF_RAW_FIELD(49)
    YAFF_RAW_FIELD(50)  YAFF_RAW_FIELD(51)  YAFF_RAW_FIELD(52)  YAFF_RAW_FIELD(53)  YAFF_RAW_FIELD(54)
    YAFF_RAW_FIELD(55)  YAFF_RAW_FIELD(56)  YAFF_RAW_FIELD(57)  YAFF_RAW_FIELD(58)  YAFF_RAW_FIELD(59)
    YAFF_RAW_FIELD(60)  YAFF_RAW_FIELD(61)  YAFF_RAW_FIELD(62)  YAFF_RAW_FIELD(63)  YAFF_RAW_FIELD(64)
    YAFF_RAW_FIELD(65)  YAFF_RAW_FIELD(66)  YAFF_RAW_FIELD(67)  YAFF_RAW_FIELD(68)  YAFF_RAW_FIELD(69)
    YAFF_RAW_FIELD(70)  YAFF_RAW_FIELD(71)  YAFF_RAW_FIELD(72)  YAFF_RAW_FIELD(73)  YAFF_RAW_FIELD(74)
    YAFF_RAW_FIELD(75)  YAFF_RAW_FIELD(76)  YAFF_RAW_FIELD(77)  YAFF_RAW_FIELD(78)  YAFF_RAW_FIELD(79)
    YAFF_RAW_FIELD(80)  YAFF_RAW_FIELD(81)  YAFF_RAW_FIELD(82)  YAFF_RAW_FIELD(83)  YAFF_RAW_FIELD(84)
    YAFF_RAW_FIELD(85)  YAFF_RAW_FIELD(86)  YAFF_RAW_FIELD(87)  YAFF_RAW_FIELD(88)  YAFF_RAW_FIELD(89)
    YAFF_RAW_FIELD(90)  YAFF_RAW_FIELD(91)  YAFF_RAW_FIELD(92)  YAFF_RAW_FIELD(93)  YAFF_RAW_FIELD(94)
    YAFF_RAW_FIELD(95)  YAFF_RAW_FIELD(96)  YAFF_RAW_FIELD(97)  YAFF_RAW_FIELD(98)  YAFF_RAW_FIELD(99)
};
YAFF_LAYOUT_END
// clang-format on

#undef YAFF_RAW_FIELD

}  // namespace benchmark_serialize_raw

inline constexpr size_t FIELD_COUNT = 100;

class DataGenerator {
public:
    explicit DataGenerator(uint64_t seed) : Rng_(seed) {
    }

    template <template <typename> typename W>
    std::array<W<uint64_t>, FIELD_COUNT> GenerateFields(uint64_t density, uint64_t defaults) {
        std::array<W<uint64_t>, FIELD_COUNT> values;
        for (auto& value : values) {
            value = FieldCast<W<uint64_t>>::Cast(GenerateField(density, defaults));
        }
        return values;
    }

private:
    inline static const uint64_t MAX_DENSITY = 100;

    template <typename T, typename = void>
    struct FieldCast;

    template <typename D>
    struct FieldCast<std::optional<uint64_t>, D> {
        static std::optional<uint64_t> Cast(std::optional<uint64_t> field) {
            return field;
        }
    };

    template <typename D>
    struct FieldCast<uint64_t, D> {
        static uint64_t Cast(std::optional<uint64_t> field) {
            return field.value_or(0);
        }
    };

    std::optional<uint64_t> GenerateField(uint64_t density, uint64_t defaults) {
        if (!WithChance(density)) {
            return std::nullopt;
        }
        return (WithChance(defaults) ? 0 : GenerateValue());
    }

    uint64_t GenerateValue() {
        std::uniform_int_distribution<uint64_t> dist(1, ((1ULL << 48) - 1));
        return dist(Rng_);
    }

    bool WithChance(uint64_t percent) {
        std::uniform_int_distribution<uint64_t> dist(1, MAX_DENSITY);
        return (dist(Rng_) <= percent);
    }

    std::mt19937 Rng_;
};

template <typename P>
void SetFields(P& proto, const std::array<std::optional<uint64_t>, FIELD_COUNT>& fields) {
    if (fields[0]) {
        proto.set_v0(*fields[0]);
    }
    if (fields[1]) {
        proto.set_v1(*fields[1]);
    }
    if (fields[2]) {
        proto.set_v2(*fields[2]);
    }
    if (fields[3]) {
        proto.set_v3(*fields[3]);
    }
    if (fields[4]) {
        proto.set_v4(*fields[4]);
    }
    if (fields[5]) {
        proto.set_v5(*fields[5]);
    }
    if (fields[6]) {
        proto.set_v6(*fields[6]);
    }
    if (fields[7]) {
        proto.set_v7(*fields[7]);
    }
    if (fields[8]) {
        proto.set_v8(*fields[8]);
    }
    if (fields[9]) {
        proto.set_v9(*fields[9]);
    }
    if (fields[10]) {
        proto.set_v10(*fields[10]);
    }
    if (fields[11]) {
        proto.set_v11(*fields[11]);
    }
    if (fields[12]) {
        proto.set_v12(*fields[12]);
    }
    if (fields[13]) {
        proto.set_v13(*fields[13]);
    }
    if (fields[14]) {
        proto.set_v14(*fields[14]);
    }
    if (fields[15]) {
        proto.set_v15(*fields[15]);
    }
    if (fields[16]) {
        proto.set_v16(*fields[16]);
    }
    if (fields[17]) {
        proto.set_v17(*fields[17]);
    }
    if (fields[18]) {
        proto.set_v18(*fields[18]);
    }
    if (fields[19]) {
        proto.set_v19(*fields[19]);
    }
    if (fields[20]) {
        proto.set_v20(*fields[20]);
    }
    if (fields[21]) {
        proto.set_v21(*fields[21]);
    }
    if (fields[22]) {
        proto.set_v22(*fields[22]);
    }
    if (fields[23]) {
        proto.set_v23(*fields[23]);
    }
    if (fields[24]) {
        proto.set_v24(*fields[24]);
    }
    if (fields[25]) {
        proto.set_v25(*fields[25]);
    }
    if (fields[26]) {
        proto.set_v26(*fields[26]);
    }
    if (fields[27]) {
        proto.set_v27(*fields[27]);
    }
    if (fields[28]) {
        proto.set_v28(*fields[28]);
    }
    if (fields[29]) {
        proto.set_v29(*fields[29]);
    }
    if (fields[30]) {
        proto.set_v30(*fields[30]);
    }
    if (fields[31]) {
        proto.set_v31(*fields[31]);
    }
    if (fields[32]) {
        proto.set_v32(*fields[32]);
    }
    if (fields[33]) {
        proto.set_v33(*fields[33]);
    }
    if (fields[34]) {
        proto.set_v34(*fields[34]);
    }
    if (fields[35]) {
        proto.set_v35(*fields[35]);
    }
    if (fields[36]) {
        proto.set_v36(*fields[36]);
    }
    if (fields[37]) {
        proto.set_v37(*fields[37]);
    }
    if (fields[38]) {
        proto.set_v38(*fields[38]);
    }
    if (fields[39]) {
        proto.set_v39(*fields[39]);
    }
    if (fields[40]) {
        proto.set_v40(*fields[40]);
    }
    if (fields[41]) {
        proto.set_v41(*fields[41]);
    }
    if (fields[42]) {
        proto.set_v42(*fields[42]);
    }
    if (fields[43]) {
        proto.set_v43(*fields[43]);
    }
    if (fields[44]) {
        proto.set_v44(*fields[44]);
    }
    if (fields[45]) {
        proto.set_v45(*fields[45]);
    }
    if (fields[46]) {
        proto.set_v46(*fields[46]);
    }
    if (fields[47]) {
        proto.set_v47(*fields[47]);
    }
    if (fields[48]) {
        proto.set_v48(*fields[48]);
    }
    if (fields[49]) {
        proto.set_v49(*fields[49]);
    }
    if (fields[50]) {
        proto.set_v50(*fields[50]);
    }
    if (fields[51]) {
        proto.set_v51(*fields[51]);
    }
    if (fields[52]) {
        proto.set_v52(*fields[52]);
    }
    if (fields[53]) {
        proto.set_v53(*fields[53]);
    }
    if (fields[54]) {
        proto.set_v54(*fields[54]);
    }
    if (fields[55]) {
        proto.set_v55(*fields[55]);
    }
    if (fields[56]) {
        proto.set_v56(*fields[56]);
    }
    if (fields[57]) {
        proto.set_v57(*fields[57]);
    }
    if (fields[58]) {
        proto.set_v58(*fields[58]);
    }
    if (fields[59]) {
        proto.set_v59(*fields[59]);
    }
    if (fields[60]) {
        proto.set_v60(*fields[60]);
    }
    if (fields[61]) {
        proto.set_v61(*fields[61]);
    }
    if (fields[62]) {
        proto.set_v62(*fields[62]);
    }
    if (fields[63]) {
        proto.set_v63(*fields[63]);
    }
    if (fields[64]) {
        proto.set_v64(*fields[64]);
    }
    if (fields[65]) {
        proto.set_v65(*fields[65]);
    }
    if (fields[66]) {
        proto.set_v66(*fields[66]);
    }
    if (fields[67]) {
        proto.set_v67(*fields[67]);
    }
    if (fields[68]) {
        proto.set_v68(*fields[68]);
    }
    if (fields[69]) {
        proto.set_v69(*fields[69]);
    }
    if (fields[70]) {
        proto.set_v70(*fields[70]);
    }
    if (fields[71]) {
        proto.set_v71(*fields[71]);
    }
    if (fields[72]) {
        proto.set_v72(*fields[72]);
    }
    if (fields[73]) {
        proto.set_v73(*fields[73]);
    }
    if (fields[74]) {
        proto.set_v74(*fields[74]);
    }
    if (fields[75]) {
        proto.set_v75(*fields[75]);
    }
    if (fields[76]) {
        proto.set_v76(*fields[76]);
    }
    if (fields[77]) {
        proto.set_v77(*fields[77]);
    }
    if (fields[78]) {
        proto.set_v78(*fields[78]);
    }
    if (fields[79]) {
        proto.set_v79(*fields[79]);
    }
    if (fields[80]) {
        proto.set_v80(*fields[80]);
    }
    if (fields[81]) {
        proto.set_v81(*fields[81]);
    }
    if (fields[82]) {
        proto.set_v82(*fields[82]);
    }
    if (fields[83]) {
        proto.set_v83(*fields[83]);
    }
    if (fields[84]) {
        proto.set_v84(*fields[84]);
    }
    if (fields[85]) {
        proto.set_v85(*fields[85]);
    }
    if (fields[86]) {
        proto.set_v86(*fields[86]);
    }
    if (fields[87]) {
        proto.set_v87(*fields[87]);
    }
    if (fields[88]) {
        proto.set_v88(*fields[88]);
    }
    if (fields[89]) {
        proto.set_v89(*fields[89]);
    }
    if (fields[90]) {
        proto.set_v90(*fields[90]);
    }
    if (fields[91]) {
        proto.set_v91(*fields[91]);
    }
    if (fields[92]) {
        proto.set_v92(*fields[92]);
    }
    if (fields[93]) {
        proto.set_v93(*fields[93]);
    }
    if (fields[94]) {
        proto.set_v94(*fields[94]);
    }
    if (fields[95]) {
        proto.set_v95(*fields[95]);
    }
    if (fields[96]) {
        proto.set_v96(*fields[96]);
    }
    if (fields[97]) {
        proto.set_v97(*fields[97]);
    }
    if (fields[98]) {
        proto.set_v98(*fields[98]);
    }
    if (fields[99]) {
        proto.set_v99(*fields[99]);
    }
}

void BM_Serialize_Raw(benchmark::State& state) {
    auto gen = DataGenerator(std::random_device{}());
    const auto plain = gen.GenerateFields<std::type_identity_t>(100, 0);

    std::array<std::byte, sizeof(benchmark_serialize_raw::Flat100)> out;
    for (auto _ : state) {
        benchmark_serialize_raw::Flat100 raw;
        raw.V0 = plain[0];
        raw.V1 = plain[1];
        raw.V2 = plain[2];
        raw.V3 = plain[3];
        raw.V4 = plain[4];
        raw.V5 = plain[5];
        raw.V6 = plain[6];
        raw.V7 = plain[7];
        raw.V8 = plain[8];
        raw.V9 = plain[9];
        raw.V10 = plain[10];
        raw.V11 = plain[11];
        raw.V12 = plain[12];
        raw.V13 = plain[13];
        raw.V14 = plain[14];
        raw.V15 = plain[15];
        raw.V16 = plain[16];
        raw.V17 = plain[17];
        raw.V18 = plain[18];
        raw.V19 = plain[19];
        raw.V20 = plain[20];
        raw.V21 = plain[21];
        raw.V22 = plain[22];
        raw.V23 = plain[23];
        raw.V24 = plain[24];
        raw.V25 = plain[25];
        raw.V26 = plain[26];
        raw.V27 = plain[27];
        raw.V28 = plain[28];
        raw.V29 = plain[29];
        raw.V30 = plain[30];
        raw.V31 = plain[31];
        raw.V32 = plain[32];
        raw.V33 = plain[33];
        raw.V34 = plain[34];
        raw.V35 = plain[35];
        raw.V36 = plain[36];
        raw.V37 = plain[37];
        raw.V38 = plain[38];
        raw.V39 = plain[39];
        raw.V40 = plain[40];
        raw.V41 = plain[41];
        raw.V42 = plain[42];
        raw.V43 = plain[43];
        raw.V44 = plain[44];
        raw.V45 = plain[45];
        raw.V46 = plain[46];
        raw.V47 = plain[47];
        raw.V48 = plain[48];
        raw.V49 = plain[49];
        raw.V50 = plain[50];
        raw.V51 = plain[51];
        raw.V52 = plain[52];
        raw.V53 = plain[53];
        raw.V54 = plain[54];
        raw.V55 = plain[55];
        raw.V56 = plain[56];
        raw.V57 = plain[57];
        raw.V58 = plain[58];
        raw.V59 = plain[59];
        raw.V60 = plain[60];
        raw.V61 = plain[61];
        raw.V62 = plain[62];
        raw.V63 = plain[63];
        raw.V64 = plain[64];
        raw.V65 = plain[65];
        raw.V66 = plain[66];
        raw.V67 = plain[67];
        raw.V68 = plain[68];
        raw.V69 = plain[69];
        raw.V70 = plain[70];
        raw.V71 = plain[71];
        raw.V72 = plain[72];
        raw.V73 = plain[73];
        raw.V74 = plain[74];
        raw.V75 = plain[75];
        raw.V76 = plain[76];
        raw.V77 = plain[77];
        raw.V78 = plain[78];
        raw.V79 = plain[79];
        raw.V80 = plain[80];
        raw.V81 = plain[81];
        raw.V82 = plain[82];
        raw.V83 = plain[83];
        raw.V84 = plain[84];
        raw.V85 = plain[85];
        raw.V86 = plain[86];
        raw.V87 = plain[87];
        raw.V88 = plain[88];
        raw.V89 = plain[89];
        raw.V90 = plain[90];
        raw.V91 = plain[91];
        raw.V92 = plain[92];
        raw.V93 = plain[93];
        raw.V94 = plain[94];
        raw.V95 = plain[95];
        raw.V96 = plain[96];
        raw.V97 = plain[97];
        raw.V98 = plain[98];
        raw.V99 = plain[99];
        memcpy(out.data(), &raw, sizeof(raw));
        benchmark::DoNotOptimize(out.data());
    }
}

template <typename P>
void BM_Serialize_Protobuf(benchmark::State& state) {
    auto gen = DataGenerator(std::random_device{}());
    const auto fields = gen.GenerateFields<std::optional>(state.range(0), state.range(1));

    P proto;
    std::array<char, 4096> out;
    for (auto _ : state) {
        proto.Clear();
        SetFields(proto, fields);
        std::ignore = proto.SerializeToArray(out.data(), out.size());
        benchmark::DoNotOptimize(out.data());
    }
}

struct FlatBuffersExplicit {
    template <typename T>
    using Arg = std::optional<T>;

    template <typename... As>
    auto operator()(flatbuffers::FlatBufferBuilder& fbb, const As&... as) const {
        return NFlatBuffersBench::CreateFlat100E(fbb, as...);
    }
};

struct FlatBuffersImplicit {
    template <typename T>
    using Arg = std::type_identity_t<T>;

    template <typename... As>
    auto operator()(flatbuffers::FlatBufferBuilder& fbb, const As&... as) const {
        return NFlatBuffersBench::CreateFlat100(fbb, as...);
    }
};

template <typename C>
void BM_Serialize_FlatBuffers(benchmark::State& state) {
    auto gen = DataGenerator(std::random_device{}());
    const auto arguments = gen.GenerateFields<C::template Arg>(state.range(0), state.range(1));

    flatbuffers::FlatBufferBuilder fbb(4096);
    fbb.ForceDefaults(state.range(1) > 0);
    for (auto _ : state) {
        fbb.Clear();
        fbb.Finish(std::apply([&](const auto&... values) { return C{}(fbb, values...); }, arguments));
        benchmark::DoNotOptimize(fbb.GetBufferPointer());
    }
}

struct YaFFExplicit {
    template <typename T>
    using Arg = std::optional<T>;

    template <typename... As>
    auto operator()(yaff::Serializer& ys, const As&... as) const {
        return protoyaff::benchmark_serialize::SerializeFlat100E(ys, as...);
    }
};

struct YaFFImplicit {
    template <typename T>
    using Arg = std::type_identity_t<T>;

    template <typename... As>
    auto operator()(yaff::Serializer& ys, const As&... as) const {
        return protoyaff::benchmark_serialize::SerializeFlat100(ys, as...);
    }
};

template <typename S, yaff::MessageLayout Layout>
void BM_Serialize_YaFF(benchmark::State& state) {
    auto gen = DataGenerator(std::random_device{}());
    const auto arguments = gen.GenerateFields<S::template Arg>(state.range(0), state.range(1));

    yaff::Serializer ys(64 * 1024);
    ys.EnforceDynamicAlternative(Layout);
    for (auto _ : state) {
        ys.Clear();
        ys.Finish(std::apply([&](const auto&... values) { return S{}(ys, values...); }, arguments));
        benchmark::DoNotOptimize(ys.Data());
    }
}

BENCHMARK(BM_Serialize_Raw)->Name("BM_Serialize_Flat_Raw/FieldCount:100");

BENCHMARK_TEMPLATE(BM_Serialize_Protobuf, benchmark_serialize::Flat100E)
    ->Name("BM_Serialize_Flat_Protobuf/Explicit/FieldCount:100")
    ->ArgNames({"Density", "Defaults"})
    ->ArgsProduct({{100, 50, 5}, {0}})
    ->Args({100, 50});
BENCHMARK_TEMPLATE(BM_Serialize_Protobuf, benchmark_serialize::Flat100)
    ->Name("BM_Serialize_Flat_Protobuf/Implicit/FieldCount:100")
    ->ArgNames({"Density", "Defaults"})
    ->ArgsProduct({{100, 50, 5}, {0}});

BENCHMARK_TEMPLATE(BM_Serialize_FlatBuffers, FlatBuffersExplicit)
    ->Name("BM_Serialize_Flat_FlatBuffers/Explicit/FieldCount:100")
    ->ArgNames({"Density", "Defaults"})
    ->ArgsProduct({{100, 50, 5}, {0}})
    ->Args({100, 50});
BENCHMARK_TEMPLATE(BM_Serialize_FlatBuffers, FlatBuffersImplicit)
    ->Name("BM_Serialize_Flat_FlatBuffers/Implicit/FieldCount:100")
    ->ArgNames({"Density", "Defaults"})
    ->ArgsProduct({{100, 50, 5}, {0}});

BENCHMARK_TEMPLATE(BM_Serialize_YaFF, YaFFExplicit, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT)
    ->Name("BM_Serialize_Flat_YaFF/FlatLayout/Explicit/FieldCount:100")
    ->ArgNames({"Density", "Defaults"})
    ->ArgsProduct({{100, 50, 5}, {0}})
    ->Args({100, 50});
BENCHMARK_TEMPLATE(BM_Serialize_YaFF, YaFFImplicit, yaff::MessageLayout::MESSAGE_LAYOUT_FLAT)
    ->Name("BM_Serialize_Flat_YaFF/FlatLayout/Implicit/FieldCount:100")
    ->ArgNames({"Density", "Defaults"})
    ->ArgsProduct({{100, 50, 5}, {0}});
BENCHMARK_TEMPLATE(BM_Serialize_YaFF, YaFFExplicit, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE)
    ->Name("BM_Serialize_Flat_YaFF/SparseLayout/Explicit/FieldCount:100")
    ->ArgNames({"Density", "Defaults"})
    ->ArgsProduct({{100, 50, 5}, {0}})
    ->Args({100, 50});
BENCHMARK_TEMPLATE(BM_Serialize_YaFF, YaFFImplicit, yaff::MessageLayout::MESSAGE_LAYOUT_SPARSE)
    ->Name("BM_Serialize_Flat_YaFF/SparseLayout/Implicit/FieldCount:100")
    ->ArgNames({"Density", "Defaults"})
    ->ArgsProduct({{100, 50, 5}, {0}});
