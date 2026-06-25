#include "game/GameSession.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace vibecity {
namespace {

constexpr std::array<std::uint8_t, 8> save_magic{
    'V', 'I', 'B', 'E', 'C', 'I', 'T', 'Y'
};
constexpr std::uint32_t save_version = 2;
constexpr std::size_t save_header_size = save_magic.size() + sizeof(std::uint32_t)
    + sizeof(std::uint64_t) + sizeof(std::uint64_t);
constexpr std::uint64_t max_save_bytes = 64 * 1024 * 1024;
constexpr std::uint32_t max_saved_buildings = 100'000;
constexpr std::uint32_t max_saved_jobs = 1'000'000;
constexpr std::int32_t max_map_dimension = 4'096;
constexpr std::int64_t max_map_tiles = 4'194'304;
constexpr std::uint32_t max_stable_id_bytes = 128;

class ByteWriter {
public:
    void u8(std::uint8_t value)
    {
        bytes_.push_back(value);
    }

    void boolean(bool value)
    {
        u8(value ? 1 : 0);
    }

    void u32(std::uint32_t value)
    {
        unsigned_integer(value);
    }

    void i32(std::int32_t value)
    {
        u32(std::bit_cast<std::uint32_t>(value));
    }

    void u64(std::uint64_t value)
    {
        unsigned_integer(value);
    }

    void i64(std::int64_t value)
    {
        u64(std::bit_cast<std::uint64_t>(value));
    }

    void append(const std::vector<std::uint8_t>& bytes)
    {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    void string(std::string_view value)
    {
        if (value.size() > max_stable_id_bytes) {
            throw std::runtime_error("stable ID is too long to save");
        }
        u32(static_cast<std::uint32_t>(value.size()));
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const
    {
        return bytes_;
    }

private:
    template <typename Unsigned>
    void unsigned_integer(Unsigned value)
    {
        static_assert(std::is_unsigned_v<Unsigned>);
        for (std::size_t index = 0; index < sizeof(Unsigned); ++index) {
            u8(static_cast<std::uint8_t>(value & 0xffU));
            value >>= 8U;
        }
    }

    std::vector<std::uint8_t> bytes_;
};

class ByteReader {
public:
    explicit ByteReader(const std::vector<std::uint8_t>& bytes, std::size_t offset = 0)
        : bytes_(bytes)
        , offset_(offset)
    {
    }

    [[nodiscard]] std::uint8_t u8()
    {
        require(sizeof(std::uint8_t));
        return bytes_[offset_++];
    }

    [[nodiscard]] bool boolean()
    {
        const auto value = u8();
        if (value > 1) {
            throw std::runtime_error("invalid boolean in save");
        }
        return value != 0;
    }

    [[nodiscard]] std::uint32_t u32()
    {
        return unsigned_integer<std::uint32_t>();
    }

    [[nodiscard]] std::int32_t i32()
    {
        return std::bit_cast<std::int32_t>(u32());
    }

    [[nodiscard]] std::uint64_t u64()
    {
        return unsigned_integer<std::uint64_t>();
    }

    [[nodiscard]] std::int64_t i64()
    {
        return std::bit_cast<std::int64_t>(u64());
    }

    [[nodiscard]] std::string string()
    {
        const auto size = u32();
        if (size > max_stable_id_bytes) {
            throw std::runtime_error("stable ID is too long in save");
        }
        require(size);
        auto result = std::string{
            reinterpret_cast<const char*>(bytes_.data() + offset_),
            size
        };
        offset_ += size;
        return result;
    }

    [[nodiscard]] std::size_t offset() const
    {
        return offset_;
    }

    [[nodiscard]] std::size_t remaining() const
    {
        return bytes_.size() - offset_;
    }

    void require_finished() const
    {
        if (offset_ != bytes_.size()) {
            throw std::runtime_error("save contains trailing data");
        }
    }

private:
    template <typename Unsigned>
    [[nodiscard]] Unsigned unsigned_integer()
    {
        static_assert(std::is_unsigned_v<Unsigned>);
        require(sizeof(Unsigned));
        auto result = Unsigned{0};
        for (std::size_t index = 0; index < sizeof(Unsigned); ++index) {
            result |= static_cast<Unsigned>(bytes_[offset_++]) << (index * 8U);
        }
        return result;
    }

    void require(std::size_t count) const
    {
        if (count > remaining()) {
            throw std::runtime_error("save ended unexpectedly");
        }
    }

    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_ = 0;
};

std::uint64_t checksum(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    constexpr auto offset_basis = std::uint64_t{14'695'981'039'346'656'037ULL};
    constexpr auto prime = std::uint64_t{1'099'511'628'211ULL};
    auto result = offset_basis;
    for (auto index = offset; index < bytes.size(); ++index) {
        result ^= bytes[index];
        result *= prime;
    }
    return result;
}

template <typename Enum>
std::uint8_t enum_value(Enum value)
{
    return static_cast<std::uint8_t>(value);
}

template <typename Enum>
Enum read_enum(ByteReader& reader, std::uint8_t exclusive_maximum, const char* message)
{
    const auto value = reader.u8();
    if (value >= exclusive_maximum) {
        throw std::runtime_error(message);
    }
    return static_cast<Enum>(value);
}

void write_resource_array(ByteWriter& writer, const ResourceArray& amounts)
{
    for (const auto amount : amounts) {
        writer.i64(amount);
    }
}

ResourceArray read_resource_array(ByteReader& reader)
{
    auto amounts = ResourceArray{};
    for (auto& amount : amounts) {
        amount = reader.i64();
    }
    return amounts;
}

void write_inventory(ByteWriter& writer, const Inventory& inventory)
{
    const auto state = inventory.state();
    write_resource_array(writer, state.quantities);
    write_resource_array(writer, state.capacities);
    write_resource_array(writer, state.reserved_outgoing);
    write_resource_array(writer, state.reserved_incoming);
}

Inventory read_inventory(ByteReader& reader)
{
    return Inventory::from_state(InventoryState{
        .quantities = read_resource_array(reader),
        .capacities = read_resource_array(reader),
        .reserved_outgoing = read_resource_array(reader),
        .reserved_incoming = read_resource_array(reader)
    });
}

void write_building(
    ByteWriter& writer,
    const BuildingInstance& building,
    const BuildingCatalog& catalog)
{
    writer.u32(building.id);
    writer.string(catalog.stable_id(building.kind));
    writer.boolean(building.position.has_value());
    if (building.position.has_value()) {
        writer.i32(building.position->x);
        writer.i32(building.position->y);
    }
    write_inventory(writer, building.inventory);
    writer.i32(building.residents);
    writer.i32(building.assigned_workers);
    writer.i32(building.assigned_builders);
    writer.i64(building.recipe_progress);
    writer.i32(building.hunger_days);
    writer.boolean(building.construction_target.has_value());
    if (building.construction_target.has_value()) {
        writer.string(catalog.stable_id(*building.construction_target));
    }
    writer.i64(building.construction_labor_required);
    writer.i64(building.construction_labor_completed);
    writer.u8(enum_value(building.blocking_reason));
}

BuildingInstance read_building(ByteReader& reader, const BuildingCatalog& catalog)
{
    auto building = BuildingInstance{};
    building.id = reader.u32();
    const auto kind = catalog.find_kind(reader.string());
    if (!kind.has_value()) {
        throw std::runtime_error("unknown building stable ID in save");
    }
    building.kind = *kind;
    if (reader.boolean()) {
        building.position = GridPosition{
            .x = reader.i32(),
            .y = reader.i32()
        };
    }
    building.inventory = read_inventory(reader);
    building.residents = reader.i32();
    building.assigned_workers = reader.i32();
    building.assigned_builders = reader.i32();
    building.recipe_progress = reader.i64();
    building.hunger_days = reader.i32();
    if (reader.boolean()) {
        const auto target = catalog.find_kind(reader.string());
        if (!target.has_value()) {
            throw std::runtime_error("unknown construction target stable ID in save");
        }
        building.construction_target = *target;
    }
    building.construction_labor_required = reader.i64();
    building.construction_labor_completed = reader.i64();
    building.blocking_reason = read_enum<BlockingReason>(
        reader,
        enum_value(BlockingReason::WaitingForBuilderLabor) + 1,
        "invalid blocking reason in save");
    return building;
}

void write_transport_job(ByteWriter& writer, const TransportJob& job)
{
    writer.u32(job.id);
    writer.string(resource_name(job.resource));
    writer.i64(job.quantity);
    writer.u32(job.source);
    writer.u32(job.destination);
    writer.u8(enum_value(job.state));
    writer.i64(job.ticks_remaining);
    writer.i64(job.leg_ticks_total);
    writer.i64(job.delivery_ticks);
}

TransportJob read_transport_job(ByteReader& reader)
{
    const auto id = reader.u32();
    const auto resource = resource_id_from_string(reader.string());
    if (!resource.has_value()) {
        throw std::runtime_error("unknown resource stable ID in save");
    }
    return TransportJob{
        .id = id,
        .resource = *resource,
        .quantity = reader.i64(),
        .source = reader.u32(),
        .destination = reader.u32(),
        .state = read_enum<TransportJobState>(
            reader,
            enum_value(TransportJobState::Failed) + 1,
            "invalid transport state in save"),
        .ticks_remaining = reader.i64(),
        .leg_ticks_total = reader.i64(),
        .delivery_ticks = reader.i64()
    };
}

void write_simulation(ByteWriter& writer, const Simulation& simulation)
{
    const auto state = simulation.state();
    const auto& catalog = simulation.building_catalog();
    writer.i32(state.map_width);
    writer.i32(state.map_height);
    writer.i64(state.current_tick);
    writer.u32(state.next_building_id);
    writer.u32(state.next_transport_job_id);
    writer.i32(state.next_auto_building_x);
    writer.boolean(state.worker_assignment_dirty);
    writer.i32(state.idle_workers);
    write_resource_array(writer, state.stats.produced);
    write_resource_array(writer, state.stats.consumed);
    write_resource_array(writer, state.stats.transported);
    writer.i32(state.stats.constructed_buildings);

    if (state.paths.size() > std::numeric_limits<std::uint32_t>::max()
        || state.buildings.size() > max_saved_buildings
        || state.transport_jobs.size() > max_saved_jobs) {
        throw std::runtime_error("game state is too large to save");
    }

    writer.u32(static_cast<std::uint32_t>(state.paths.size()));
    for (const auto path : state.paths) {
        writer.i32(path.x);
        writer.i32(path.y);
    }

    writer.u32(static_cast<std::uint32_t>(state.buildings.size()));
    for (const auto& building : state.buildings) {
        write_building(writer, building, catalog);
    }

    writer.u32(static_cast<std::uint32_t>(state.transport_jobs.size()));
    for (const auto& job : state.transport_jobs) {
        write_transport_job(writer, job);
    }
}

SimulationState read_simulation(ByteReader& reader, const BuildingCatalog& catalog)
{
    auto state = SimulationState{};
    state.map_width = reader.i32();
    state.map_height = reader.i32();
    state.current_tick = reader.i64();
    state.next_building_id = reader.u32();
    state.next_transport_job_id = reader.u32();
    state.next_auto_building_x = reader.i32();
    state.worker_assignment_dirty = reader.boolean();
    state.idle_workers = reader.i32();
    state.stats.produced = read_resource_array(reader);
    state.stats.consumed = read_resource_array(reader);
    state.stats.transported = read_resource_array(reader);
    state.stats.constructed_buildings = reader.i32();

    const auto path_count = reader.u32();
    const auto map_tiles = static_cast<std::int64_t>(state.map_width) * state.map_height;
    if (state.map_width <= 0
        || state.map_height <= 0
        || state.map_width > max_map_dimension
        || state.map_height > max_map_dimension
        || map_tiles > max_map_tiles
        || path_count > static_cast<std::uint64_t>(map_tiles)) {
        throw std::runtime_error("invalid path count in save");
    }
    state.paths.reserve(path_count);
    for (auto index = std::uint32_t{0}; index < path_count; ++index) {
        state.paths.push_back(GridPosition{
            .x = reader.i32(),
            .y = reader.i32()
        });
    }

    const auto building_count = reader.u32();
    if (building_count > max_saved_buildings) {
        throw std::runtime_error("too many buildings in save");
    }
    state.buildings.reserve(building_count);
    for (auto index = std::uint32_t{0}; index < building_count; ++index) {
        state.buildings.push_back(read_building(reader, catalog));
    }

    const auto job_count = reader.u32();
    if (job_count > max_saved_jobs) {
        throw std::runtime_error("too many transport jobs in save");
    }
    state.transport_jobs.reserve(job_count);
    for (auto index = std::uint32_t{0}; index < job_count; ++index) {
        state.transport_jobs.push_back(read_transport_job(reader));
    }
    return state;
}

void write_objectives(ByteWriter& writer, const VillageObjectiveTrackerState& state)
{
    writer.boolean(state.initialized);
    writer.i32(state.last_day);
    writer.i32(state.last_hunger_days);
    writer.i32(state.stable_days_at_25_residents);
}

VillageObjectiveTrackerState read_objectives(ByteReader& reader)
{
    return VillageObjectiveTrackerState{
        .initialized = reader.boolean(),
        .last_day = reader.i32(),
        .last_hunger_days = reader.i32(),
        .stable_days_at_25_residents = reader.i32()
    };
}

std::vector<std::uint8_t> encode_game(
    const Simulation& simulation,
    const VillageObjectiveTracker& objectives)
{
    auto payload = ByteWriter{};
    payload.u64(simulation.building_catalog().fingerprint());
    write_simulation(payload, simulation);
    write_objectives(payload, objectives.state());

    auto file = ByteWriter{};
    for (const auto byte : save_magic) {
        file.u8(byte);
    }
    file.u32(save_version);
    file.u64(payload.bytes().size());
    file.u64(checksum(payload.bytes(), 0));
    file.append(payload.bytes());
    if (file.bytes().size() > max_save_bytes) {
        throw std::runtime_error("save exceeds maximum supported size");
    }
    return file.bytes();
}

std::pair<Simulation, VillageObjectiveTracker> decode_game(
    const std::vector<std::uint8_t>& bytes,
    std::shared_ptr<const BuildingCatalog> catalog)
{
    if (catalog == nullptr) {
        throw std::runtime_error("cannot load without a building catalog");
    }
    if (bytes.size() < save_header_size || bytes.size() > max_save_bytes) {
        throw std::runtime_error("invalid save file size");
    }

    auto header = ByteReader{bytes};
    for (const auto expected : save_magic) {
        if (header.u8() != expected) {
            throw std::runtime_error("not a VibeCity save file");
        }
    }
    const auto version = header.u32();
    if (version != save_version) {
        throw std::runtime_error("unsupported save version");
    }
    const auto payload_size = header.u64();
    const auto expected_checksum = header.u64();
    if (payload_size != bytes.size() - save_header_size) {
        throw std::runtime_error("save payload size does not match file");
    }
    if (checksum(bytes, save_header_size) != expected_checksum) {
        throw std::runtime_error("save checksum mismatch");
    }

    auto payload = ByteReader{bytes, save_header_size};
    if (payload.u64() != catalog->fingerprint()) {
        throw std::runtime_error("save uses incompatible building definitions");
    }
    auto simulation = Simulation::from_state(read_simulation(payload, *catalog), catalog);
    const auto objective_state = read_objectives(payload);
    payload.require_finished();

    auto objectives = VillageObjectiveTracker{};
    objectives.restore(objective_state, simulation);
    return {std::move(simulation), std::move(objectives)};
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path)
{
    auto input = std::ifstream{path, std::ios::binary | std::ios::ate};
    if (!input) {
        throw std::runtime_error("save file could not be opened");
    }

    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > max_save_bytes) {
        throw std::runtime_error("save file is too large");
    }
    auto bytes = std::vector<std::uint8_t>(static_cast<std::size_t>(end));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!input) {
        throw std::runtime_error("save file could not be read");
    }
    return bytes;
}

void write_file_atomically(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    auto temporary = path;
    temporary += ".tmp";

    try {
        {
            auto output = std::ofstream{temporary, std::ios::binary | std::ios::trunc};
            if (!output) {
                throw std::runtime_error("temporary save file could not be opened");
            }
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            output.flush();
            output.close();
            if (!output) {
                throw std::runtime_error("temporary save file could not be written");
            }
        }

        auto error = std::error_code{};
        std::filesystem::rename(temporary, path, error);
        if (error) {
            throw std::runtime_error("save file could not replace previous save");
        }
    } catch (...) {
        auto ignored = std::error_code{};
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

}

SessionIoResult GameSession::save_to_file(const std::filesystem::path& path) const
{
    try {
        write_file_atomically(path, encode_game(simulation_, objectives_));
        return SessionIoResult{
            .success = true,
            .message = "game saved to " + path.string()
        };
    } catch (const std::exception& exception) {
        return SessionIoResult{
            .success = false,
            .message = exception.what()
        };
    }
}

SessionIoResult GameSession::load_from_file(const std::filesystem::path& path)
{
    try {
        auto [simulation, objectives] = decode_game(
            read_file(path),
            simulation_.building_catalog_ptr());
        simulation_ = std::move(simulation);
        objectives_ = std::move(objectives);
        return SessionIoResult{
            .success = true,
            .message = "game loaded from " + path.string()
        };
    } catch (const std::exception& exception) {
        return SessionIoResult{
            .success = false,
            .message = exception.what()
        };
    }
}

}
