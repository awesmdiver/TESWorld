#undef NDEBUG
#include "tesworld/Plugins.h"

#include <zlib.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace tesworld;
namespace {
using Bytes = std::vector<std::uint8_t>;
void U16(Bytes& b, std::uint16_t v) { b.push_back(static_cast<std::uint8_t>(v)); b.push_back(static_cast<std::uint8_t>(v >> 8)); }
void U32(Bytes& b, std::uint32_t v) { U16(b, static_cast<std::uint16_t>(v)); U16(b, static_cast<std::uint16_t>(v >> 16)); }
void Sig(Bytes& b, const char* s) { b.insert(b.end(), s, s + 4); }
void Sub(Bytes& b, const char* sig, const Bytes& data, bool extended = false) {
    if (extended) { Sig(b, "XXXX"); U16(b, 4); U32(b, static_cast<std::uint32_t>(data.size())); }
    Sig(b, sig); U16(b, extended ? 0 : static_cast<std::uint16_t>(data.size())); b.insert(b.end(), data.begin(), data.end());
}
void Record(Bytes& b, const char* sig, std::uint32_t id, std::uint32_t flags, const Bytes& data) {
    Sig(b, sig); U32(b, static_cast<std::uint32_t>(data.size())); U32(b, flags); U32(b, id); U32(b, 0); U16(b, 44); U16(b, 0);
    b.insert(b.end(), data.begin(), data.end());
}
struct Fixture {
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("tesworld_cell_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".esp");
    ~Fixture() { std::error_code ec; std::filesystem::remove(path, ec); }
    StaticPluginShard Read(const Bytes& cell, bool compressed, int corruption = 0) {
        Bytes payload = cell;
        if (compressed) {
            uLongf size = compressBound(static_cast<uLong>(cell.size()));
            Bytes packed(size);
            assert(compress2(packed.data(), &size, cell.data(), static_cast<uLong>(cell.size()), Z_BEST_COMPRESSION) == Z_OK);
            packed.resize(size); payload.clear(); U32(payload, static_cast<std::uint32_t>(cell.size()) + (corruption == 1 ? 1u : 0u));
            payload.insert(payload.end(), packed.begin(), packed.end());
            if (corruption == 2) payload.pop_back();
            if (corruption == 3) payload.back() ^= 0xFF;
            if (corruption >= 4) payload.insert(payload.end(), corruption == 4 ? 1u : corruption == 5 ? 5u : 10u, 0x5A); // Bytes after the stream end.
        }
        Bytes bytes; Record(bytes, "TES4", 0, 0, {}); Record(bytes, "CELL", 0x800, compressed ? 0x40000u : 0u, payload);
        { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); }
        const std::vector<LoadOrderEntry> order{ LoadOrderEntry{ .path = path, .pluginName = "Cell.esp" } };
        return PluginParser{}.ParseStaticWorldShard(order[0], 0, order);
    }
};
void CellFieldsAndCompleteInflation() {
    Fixture f;
    Bytes data; Sub(data, "DATA", { 0, 0 }); Sub(data, "XCLC", Bytes(8)); Sub(data, "XCLW", Bytes(4));
    Sub(data, "UNKN", Bytes(8192, 0x42)); // EDID after the old early-stop point.
    for (const auto& text : { std::string{}, std::string("NamedCell"), std::string(70000, 'X') }) {
        auto payload = data; Bytes name(text.begin(), text.end()); name.push_back(0); Sub(payload, "EDID", name, text.size() > 65535);
        for (bool compressed : { false, true }) {
            const auto shard = f.Read(payload, compressed);
            assert(shard.cells.size() == 1);
            assert(shard.cells[0].editorID == text);
            assert(shard.cells[0].fields == 15);
            assert(shard.stats.streamedCellEarlyStops == 0);
        }
    }
    assert((f.Read(data, true).cells[0].fields & CellInfo::FieldEditorID) == 0);
    for (int corruption = 1; corruption <= 3; ++corruption) {
        bool failed = false;
        try { (void)f.Read(data, true, corruption); } catch (const std::runtime_error&) { failed = true; }
        assert(failed);
    }
    // The record's own size bounds it: bytes after the end of the zlib stream are ignored, as the game ignores them.
    for (int corruption = 4; corruption <= 6; ++corruption) {
        const auto shard = f.Read(data, true, corruption);
        assert(shard.cells.size() == 1 && shard.cells[0].formID == FormID{ 0x800 });
        const auto plain = f.Read(data, false);
        assert(shard.cells[0].fields == plain.cells[0].fields);
        assert(shard.cells[0].gridX == plain.cells[0].gridX && shard.cells[0].gridY == plain.cells[0].gridY);
    }
    data.push_back(0); // Partial trailing subrecord must not be accepted.
    bool failed = false;
    try { (void)f.Read(data, true); } catch (const std::runtime_error&) { failed = true; }
    assert(failed);
}
void IgnoredIsNotDeleted() {
    StaticPluginShard initial;
    const FormID world{ 0x800 }, cell{ 0x801 }, land{ 0x802 }, base{ 0x803 }, placed{ 0x804 };
    initial.worlds.push_back(WorldInfo{ .formID = world });
    initial.cells.push_back(CellInfo{ .formID = cell, .worldFormID = world, .gridX = 0, .gridY = 0 });
    initial.lands.push_back(LandInfo{ .formID = land, .parentCell = cell });
    initial.baseObjects.push_back(BaseObjectInfo{ .formID = base, .kind = BaseObjectKind::Static });
    initial.placements.push_back(PlacementInfo{ .formID = placed, .parentCell = cell, .baseFormID = base });
    for (auto flags : { 0x1000u, 0x1020u, 0x20u }) {
        StaticPluginShard patch;
        for (auto [id, signature] : { std::pair{ world, MakeFourCC('W','R','L','D') }, { cell, MakeFourCC('C','E','L','L') }, { land, MakeFourCC('L','A','N','D') }, { base, MakeFourCC('S','T','A','T') }, { placed, MakeFourCC('R','E','F','R') } })
            patch.suppressors.push_back(StaticRecordSuppressor{ .formID = id, .signature = signature, .flags = flags });
        for (bool parallel : { false, true }) {
            const auto result = BuildStaticWorldSnapshot({ initial, patch }, { .parallelPlacementBucketing = parallel });
            const bool retained = (flags & 0x1000u) != 0;
            assert(result.worldsByFormID.contains(world) == retained);
            assert(result.cellsByFormID.contains(cell) == retained);
            assert(result.baseObjectsByFormID.contains(base) == retained);
            assert(result.landsByWorldspace.contains(world) == retained);
            assert((result.GetExteriorCell(world, 0, 0) != nullptr) == retained);
        }
    }
}
void CellHasOneLandSlot() {
    const FormID world{ 0x800 }, cell{ 0x801 }, old{ 0x802 }, replacement{ 0x803 };
    StaticPluginShard first;
    first.worlds.push_back(WorldInfo{ .formID = world });
    first.cells.push_back(CellInfo{ .formID = cell, .worldFormID = world, .gridX = 0, .gridY = 0 });
    first.lands.push_back(LandInfo{ .formID = old, .parentCell = cell });
    StaticPluginShard second;
    second.lands.push_back(LandInfo{ .formID = replacement, .parentCell = cell });
    const auto check = [&](const StaticPluginShard& last, std::optional<FormID> expected) {
        const auto snapshot = BuildStaticWorldSnapshot({ first, second, last });
        if (!expected) { assert(!snapshot.landsByWorldspace.contains(world)); return; }
        const auto& lands = snapshot.landsByWorldspace.at(world);
        assert(lands.size() == 1 && lands.front().formID == *expected);
    };
    StaticPluginShard omitted;
    omitted.cells = first.cells;
    check(omitted, replacement);
    StaticPluginShard ignored;
    ignored.suppressors.push_back(StaticRecordSuppressor{ .formID = replacement, .signature = MakeFourCC('L','A','N','D'), .flags = 0x1020 });
    check(ignored, replacement);
    StaticPluginShard deleteDisplaced;
    deleteDisplaced.suppressors.push_back(StaticRecordSuppressor{ .formID = old, .signature = MakeFourCC('L','A','N','D'), .flags = 0x20 });
    check(deleteDisplaced, replacement);
    StaticPluginShard deleteCurrent;
    deleteCurrent.suppressors.push_back(StaticRecordSuppressor{ .formID = replacement, .signature = MakeFourCC('L','A','N','D'), .flags = 0x20 });
    check(deleteCurrent, std::nullopt);
    StaticPluginShard reintroduced;
    reintroduced.lands = first.lands;
    check(reintroduced, old);
}
void GrassLoadingPolicy() {
    Fixture fixture;
    Bytes flags; U32(flags, 0);
    Bytes heights(sizeof(float) + LandInfo::VertexCount + 3, 0);
    Bytes colors(LandInfo::VertexCount * 3, 7);
    const auto read = [&](bool gated, bool dataFirst) {
        Bytes payload;
        if (dataFirst) Sub(payload, "DATA", flags);
        Sub(payload, "VHGT", heights); Sub(payload, "VCLR", colors);
        if (!dataFirst) Sub(payload, "DATA", flags);
        Bytes plugin; Record(plugin, "TES4", 0, 0, {}); Record(plugin, "LAND", 0x800, 0, payload);
        { std::ofstream out(fixture.path, std::ios::binary); out.write(reinterpret_cast<const char*>(plugin.data()), static_cast<std::streamsize>(plugin.size())); }
        const std::vector<LoadOrderEntry> order{ LoadOrderEntry{ .path=fixture.path, .pluginName="grass.esp" } };
        return PluginParser{}.ParseStaticWorldShard(order[0], 0, order, gated ? PluginParser::ParseMode::StaticGrassIndex : PluginParser::ParseMode::StaticWorldIndex);
    };
    assert(read(false, true).lands[0].hasHeights);
    assert(!read(true, true).lands[0].hasHeights);
    flags[0] = 3;
    const auto enabled = read(true, true);
    assert(enabled.lands[0].hasHeights && enabled.lands[0].hasVertexColors);
    assert(enabled.lands[0].vertexColors[0][0] == 7);
    assert(!read(true, false).lands[0].hasHeights); // DATA is interpreted in file order.

    StaticPluginShard first, partial;
    const FormID world{0x800}, cell{0x801};
    first.worlds.push_back(WorldInfo{ .formID=world, .editorID="Original", .defaultLandHeight=-1000.0f, .defaultWaterHeight=-14000.0f });
    first.cells.push_back(CellInfo{ .formID=cell, .worldFormID=world, .gridX=-2, .gridY=3 });
    partial.worlds.push_back(WorldInfo{ .formID=world, .flags=RecordFlagPartialForm, .editorID="Partial", .defaultWaterHeight=0.0f, .fields=WorldInfo::FieldEditorID | WorldInfo::FieldHeights });
    partial.worlds.push_back(WorldInfo{ .formID=FormID{0x900}, .flags=RecordFlagPartialForm });
    partial.cells.push_back(CellInfo{ .formID=cell, .flags=RecordFlagPartialForm, .gridX=99, .fields=CellInfo::FieldGrid });
    const auto snapshot = BuildStaticWorldSnapshot({first, partial}, { .enginePartialForms=true });
    assert(snapshot.worldsByFormID.at(world).editorID == "Original");
    assert(snapshot.worldsByFormID.at(world).defaultWaterHeight == -14000.0f);
    assert(!snapshot.worldsByFormID.contains(FormID{0x900}));
    assert(snapshot.cellsByFormID.at(cell).gridX == -2);
}

}
int main() { CellFieldsAndCompleteInflation(); IgnoredIsNotDeleted(); CellHasOneLandSlot(); GrassLoadingPolicy(); std::cout << "tesworld reader regressions passed\n"; }