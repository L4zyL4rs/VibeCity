#include "TestSupport.hpp"

#include "game/GameSession.hpp"
#include "game/Scenario.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

vibecity::CommandResult require(vibecity::GameSession& game, const vibecity::GameCommand& command)
{
    auto result = game.execute(command);
    VIBECITY_CHECK(result.success);
    return result;
}

vibecity::BuildingId require_building(vibecity::GameSession& game, const vibecity::GameCommand& command)
{
    auto result = require(game, command);
    VIBECITY_CHECK(result.building.has_value());
    return *result.building;
}

int total_hunger_days(const vibecity::Simulation& simulation)
{
    auto hunger_days = 0;
    for (const auto& building : simulation.buildings()) {
        hunger_days += building.hunger_days;
    }
    return hunger_days;
}

const vibecity::VillageObjectiveStatus& objective_status(const vibecity::GameSession& game, vibecity::VillageObjectiveId id)
{
    return game.objectives().statuses()[static_cast<std::size_t>(id)];
}

vibecity::GameSession starting_village_session()
{
    return vibecity::GameSession{
        vibecity::starting_village_world_generation_settings()
    };
}

vibecity::GridPosition first_buildable_house_site_on(
    const vibecity::Simulation& simulation,
    vibecity::TerrainId terrain)
{
    for (int y = 0; y < simulation.map().height(); ++y) {
        for (int x = 0; x < simulation.map().width(); ++x) {
            const auto position = vibecity::GridPosition{x, y};
            if (simulation.map().terrain_at(position) == terrain
                && simulation.can_place_building_at(vibecity::BuildingKind::House, position)) {
                return position;
            }
        }
    }
    throw std::runtime_error("could not find buildable house site");
}

bool inside_footprint(
    vibecity::GridPosition tile,
    vibecity::GridPosition position,
    vibecity::Footprint footprint)
{
    return tile.x >= position.x
        && tile.y >= position.y
        && tile.x < position.x + footprint.width
        && tile.y < position.y + footprint.height;
}

vibecity::Quantity gatherable_resource_quantity(
    const vibecity::Simulation& simulation,
    vibecity::BuildingKind kind,
    vibecity::GridPosition position)
{
    const auto& definition = simulation.definition(kind);
    VIBECITY_CHECK(definition.gathering.has_value());

    auto quantity = vibecity::Quantity{0};
    const auto& gathering = *definition.gathering;
    for (const auto tile : simulation.map().tiles_within_radius(
             position,
             definition.footprint,
             gathering.radius)) {
        if (inside_footprint(tile, position, definition.footprint)) {
            continue;
        }
        if (simulation.map().map_resource_at(tile) == gathering.resource) {
            quantity += simulation.map().map_resource_quantity(tile);
        }
    }
    return quantity;
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path)
{
    auto input = std::ifstream{path, std::ios::binary | std::ios::ate};
    VIBECITY_CHECK(input.good());
    const auto size = input.tellg();
    VIBECITY_CHECK(size >= 0);
    auto bytes = std::vector<std::uint8_t>(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    VIBECITY_CHECK(input.good());
    return bytes;
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    auto output = std::ofstream{path, std::ios::binary | std::ios::trunc};
    VIBECITY_CHECK(output.good());
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    VIBECITY_CHECK(output.good());
}

void write_text(const std::filesystem::path& path, std::string_view text)
{
    auto output = std::ofstream{path, std::ios::trunc};
    VIBECITY_CHECK(output.good());
    output << text;
    VIBECITY_CHECK(output.good());
}

void copy_default_building_definitions(const std::filesystem::path& destination)
{
    std::filesystem::create_directories(destination);
    for (const auto& entry : std::filesystem::directory_iterator("data/buildings")) {
        if (entry.is_regular_file()) {
            std::filesystem::copy_file(
                entry.path(),
                destination / entry.path().filename(),
                std::filesystem::copy_options::overwrite_existing);
        }
    }
}

std::string charcoal_kiln_definition(int firewood_output)
{
    return std::string{R"([building]
id = charcoal_kiln
name = Charcoal Kiln
footprint = 2, 2
worker_slots = 1
resident_capacity = 0
worker_supply = 0
consumes_bread = false
requests_storage_inputs = false
internal_construction_site = false
source_policy = recipe_outputs
map_color = 92, 104, 76
construction_labor_minutes = 1440

[construction]
timber = 4

[storage]
timber = 10
firewood = 20

[recipe]
cycle_minutes = 120

[inputs]
timber = 2

[outputs]
firewood = )"} + std::to_string(firewood_output) + "\n";
}

void command_layer_places_path_and_building()
{
    vibecity::GameSession game;

    auto result = game.execute(vibecity::PlacePathCommand{.position = vibecity::GridPosition{1, 0}});
    VIBECITY_CHECK(result.success);

    result = game.execute(vibecity::PlaceBuildingCommand{
        .kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{1, 1}
    });
    VIBECITY_CHECK(result.success);
    VIBECITY_CHECK(result.building.has_value());
    VIBECITY_CHECK(game.simulation().building(*result.building).kind == vibecity::BuildingKind::House);
}

void command_layer_removes_path_and_demolishes_building()
{
    vibecity::GameSession game;

    auto result = game.execute(vibecity::PlacePathCommand{.position = vibecity::GridPosition{1, 0}});
    VIBECITY_CHECK(result.success);
    result = game.execute(vibecity::RemovePathCommand{.position = vibecity::GridPosition{1, 0}});
    VIBECITY_CHECK(result.success);
    VIBECITY_CHECK(!game.simulation().map().has_path(vibecity::GridPosition{1, 0}));
    result = game.execute(vibecity::RemovePathCommand{.position = vibecity::GridPosition{1, 0}});
    VIBECITY_CHECK(!result.success);

    const auto house = require_building(game, vibecity::PlaceBuildingCommand{
        .kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{1, 1}
    });
    result = game.execute(vibecity::DemolishBuildingCommand{.building = house});
    VIBECITY_CHECK(result.success);
    VIBECITY_CHECK(game.simulation().map().can_place_building(
        vibecity::GridPosition{1, 1},
        vibecity::Footprint{1, 1}));
    result = game.execute(vibecity::DemolishBuildingCommand{.building = house});
    VIBECITY_CHECK(!result.success);

    auto threw = false;
    try {
        [[maybe_unused]] const auto& removed = game.simulation().building(house);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    VIBECITY_CHECK(threw);
}

void invalid_command_reports_failure()
{
    vibecity::GameSession game;

    const auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = -1});
    VIBECITY_CHECK(!result.success);
}

void command_layer_rejects_locked_construction()
{
    vibecity::GameSession game;
    const auto potter = game.simulation().building_catalog().find_kind("potter");
    VIBECITY_CHECK(potter.has_value());

    auto result = game.execute(vibecity::PlaceConstructionCommand{
        .target_kind = *potter,
        .position = vibecity::GridPosition{1, 1}
    });
    VIBECITY_CHECK(!result.success);

    game.simulation().grant_capability(vibecity::CapabilityId::Pottery);
    result = game.execute(vibecity::PlaceConstructionCommand{
        .target_kind = *potter,
        .position = vibecity::GridPosition{1, 1}
    });
    VIBECITY_CHECK(result.success);
    VIBECITY_CHECK(result.building.has_value());
}

void starting_village_world_generation_supports_reference_route()
{
    const auto game = starting_village_session();
    const auto& simulation = game.simulation();

    VIBECITY_CHECK(simulation.map().terrain_at(vibecity::GridPosition{37, 12})
        == vibecity::TerrainId::ShallowWater);
    VIBECITY_CHECK(simulation.map().terrain_at(vibecity::GridPosition{38, 12})
        == vibecity::TerrainId::ShallowWater);

    VIBECITY_CHECK(simulation.can_place_building_at(
        vibecity::BuildingKind::Farm,
        vibecity::GridPosition{13, vibecity::starting_village_building_y}));
    VIBECITY_CHECK(simulation.can_place_building_at(
        vibecity::BuildingKind::Farm,
        vibecity::GridPosition{26, vibecity::starting_village_building_y}));

    const auto quarry_kind = simulation.building_catalog().find_kind("quarry");
    VIBECITY_CHECK(quarry_kind.has_value());
    VIBECITY_CHECK(simulation.can_place_building_at(
        *quarry_kind,
        vibecity::GridPosition{19, 10}));

    VIBECITY_CHECK(gatherable_resource_quantity(
            simulation,
            vibecity::BuildingKind::Woodcutter,
            vibecity::GridPosition{10, vibecity::starting_village_building_y})
        >= 60);
    VIBECITY_CHECK(gatherable_resource_quantity(
            simulation,
            vibecity::BuildingKind::Woodcutter,
            vibecity::GridPosition{23, vibecity::starting_village_building_y})
        >= 60);
    VIBECITY_CHECK(gatherable_resource_quantity(
            simulation,
            *quarry_kind,
            vibecity::GridPosition{19, 10})
        >= 24);

    const auto brickyard_kind = simulation.building_catalog().find_kind("brickyard");
    VIBECITY_CHECK(brickyard_kind.has_value());
    VIBECITY_CHECK(gatherable_resource_quantity(
            simulation,
            *brickyard_kind,
            vibecity::GridPosition{38, vibecity::starting_village_building_y})
        >= 30);
}

void starting_village_runs_through_command_layer()
{
    auto game = starting_village_session();
    const auto ids = vibecity::create_starting_village(game);

    VIBECITY_CHECK(ids.houses.size() == 3);
    VIBECITY_CHECK(!ids.woodcutter.has_value());
    VIBECITY_CHECK(!ids.farm.has_value());
    VIBECITY_CHECK(!ids.bakery.has_value());
    VIBECITY_CHECK(game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Bread) == 60);
    VIBECITY_CHECK(game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Timber) == 8);
    VIBECITY_CHECK(game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Tools) == 5);
    VIBECITY_CHECK(game.simulation().definition(vibecity::BuildingKind::Woodcutter)
            .construction_materials
        == vibecity::empty_resources());
    VIBECITY_CHECK(game.simulation().definition(vibecity::BuildingKind::House)
            .construction_materials[vibecity::resource_index(vibecity::ResourceId::Timber)]
        > game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Timber));

    auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = 2 * vibecity::ticks_per_day});
    VIBECITY_CHECK(result.success);
    VIBECITY_CHECK(game.simulation().stats().constructed_buildings == 0);
}

void objectives_track_starting_village_status()
{
    auto game = starting_village_session();

    VIBECITY_CHECK(game.objectives().completed_count() == 0);
    VIBECITY_CHECK(!game.objectives().all_complete());
    VIBECITY_CHECK(game.objectives().active_status() != nullptr);
    VIBECITY_CHECK(game.objectives().active_status()->id == vibecity::VillageObjectiveId::HaveWoodcutter);
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::HaveWoodcutter).complete);

    const auto ids = vibecity::create_starting_village(game);
    VIBECITY_CHECK(ids.houses.size() == 3);

    VIBECITY_CHECK(game.objectives().completed_count() == 1);
    VIBECITY_CHECK(!game.objectives().all_complete());
    VIBECITY_CHECK(game.objectives().active_status() != nullptr);
    VIBECITY_CHECK(game.objectives().active_status()->id == vibecity::VillageObjectiveId::HaveWoodcutter);
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::HaveWoodcutter).complete);
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::HaveFarm).complete);
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::HaveQuarry).complete);
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::HaveBakery).complete);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::HaveTwoStorehouses).current == 1);
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::HaveTwoStorehouses).complete);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Reach15Residents).complete);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Reach25Residents).current == 15);
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::Reach25Residents).complete);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Stable25Residents).current == 0);
}

void objectives_count_stable_days_and_reset_on_hunger()
{
    vibecity::GameSession game;

    for (int house_index = 0; house_index < 5; ++house_index) {
        const auto house = require_building(game, vibecity::PlaceBuildingCommand{
            .kind = vibecity::BuildingKind::House,
            .position = vibecity::GridPosition{house_index + 1, 1}
        });
        require(game, vibecity::SetResidentsCommand{.building = house, .residents = 5});
        require(game, vibecity::AddInventoryCommand{
            .building = house,
            .resource = vibecity::ResourceId::Bread,
            .quantity = 10
        });
    }

    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Reach25Residents).complete);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Stable25Residents).current == 0);

    require(game, vibecity::AdvanceTimeCommand{.ticks = 2 * vibecity::ticks_per_day});

    VIBECITY_CHECK(game.objectives().stable_days_at_25_residents() == 2);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Stable25Residents).current == 2);
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::Stable25Residents).complete);

    require(game, vibecity::AdvanceTimeCommand{.ticks = vibecity::ticks_per_day});

    VIBECITY_CHECK(total_hunger_days(game.simulation()) == 5);
    VIBECITY_CHECK(game.objectives().stable_days_at_25_residents() == 0);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Stable25Residents).current == 0);
}

void save_load_round_trip_preserves_deterministic_session()
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto first_save = directory / "vibecity-roundtrip-first.vcs";
    const auto loaded_save = directory / "vibecity-roundtrip-loaded.vcs";
    const auto continued_original = directory / "vibecity-roundtrip-original-continued.vcs";
    const auto continued_loaded = directory / "vibecity-roundtrip-loaded-continued.vcs";

    auto original = starting_village_session();
    const auto ids = vibecity::create_starting_village(original);
    VIBECITY_CHECK(ids.houses.size() == 3);
    require_building(original, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Woodcutter,
        .position = vibecity::GridPosition{10, vibecity::starting_village_building_y}
    });
    require_building(original, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Farm,
        .position = vibecity::GridPosition{13, vibecity::starting_village_building_y}
    });
    require_building(original, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Bakery,
        .position = vibecity::GridPosition{16, vibecity::starting_village_building_y}
    });
    require(original, vibecity::AdvanceTimeCommand{.ticks = 1});
    VIBECITY_CHECK(!original.simulation().transport_jobs().empty());
    const auto forest = original.simulation().map().map_resource_deposits().front();
    VIBECITY_CHECK(original.simulation().set_map_resource(
        forest.position,
        forest.resource,
        2));
    const auto terrain = original.simulation().map().terrain_tiles().front();

    auto io = original.save_to_file(first_save);
    VIBECITY_CHECK(io.success);
    io = original.save_to_file(first_save);
    VIBECITY_CHECK(io.success);

    vibecity::GameSession loaded;
    io = loaded.load_from_file(first_save);
    VIBECITY_CHECK(io.success);
    VIBECITY_CHECK(loaded.simulation().map().terrain_at(terrain.position) == terrain.terrain);
    VIBECITY_CHECK(loaded.simulation().map().map_resource_quantity(forest.position) == 2);
    io = loaded.save_to_file(loaded_save);
    VIBECITY_CHECK(io.success);
    VIBECITY_CHECK(read_bytes(first_save) == read_bytes(loaded_save));

    const auto original_house = require_building(original, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{19, vibecity::starting_village_building_y}
    });
    const auto loaded_house = require_building(loaded, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{19, vibecity::starting_village_building_y}
    });
    VIBECITY_CHECK(original_house == loaded_house);

    require(original, vibecity::AdvanceTimeCommand{.ticks = 4'000});
    require(loaded, vibecity::AdvanceTimeCommand{.ticks = 4'000});
    VIBECITY_CHECK(original.save_to_file(continued_original).success);
    VIBECITY_CHECK(loaded.save_to_file(continued_loaded).success);
    VIBECITY_CHECK(read_bytes(continued_original) == read_bytes(continued_loaded));

    std::filesystem::remove(first_save);
    std::filesystem::remove(loaded_save);
    std::filesystem::remove(continued_original);
    std::filesystem::remove(continued_loaded);
}

void save_load_preserves_objective_history()
{
    const auto path = std::filesystem::temp_directory_path() / "vibecity-objectives.vcs";
    vibecity::GameSession game;

    for (int house_index = 0; house_index < 5; ++house_index) {
        const auto house = require_building(game, vibecity::PlaceBuildingCommand{
            .kind = vibecity::BuildingKind::House,
            .position = vibecity::GridPosition{house_index + 1, 1}
        });
        require(game, vibecity::SetResidentsCommand{.building = house, .residents = 5});
        require(game, vibecity::AddInventoryCommand{
            .building = house,
            .resource = vibecity::ResourceId::Bread,
            .quantity = 10
        });
    }
    require(game, vibecity::AdvanceTimeCommand{.ticks = 2 * vibecity::ticks_per_day});
    VIBECITY_CHECK(game.objectives().stable_days_at_25_residents() == 2);
    VIBECITY_CHECK(game.save_to_file(path).success);

    vibecity::GameSession loaded;
    VIBECITY_CHECK(loaded.load_from_file(path).success);
    VIBECITY_CHECK(loaded.objectives().stable_days_at_25_residents() == 2);
    VIBECITY_CHECK(objective_status(loaded, vibecity::VillageObjectiveId::Stable25Residents).current == 2);

    std::filesystem::remove(path);
}

void save_load_preserves_demolished_buildings()
{
    const auto path = std::filesystem::temp_directory_path() / "vibecity-demolition.vcs";
    vibecity::GameSession game;

    const auto first_house = require_building(game, vibecity::PlaceBuildingCommand{
        .kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{1, 1}
    });
    require(game, vibecity::DemolishBuildingCommand{.building = first_house});
    const auto second_house = require_building(game, vibecity::PlaceBuildingCommand{
        .kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{1, 1}
    });
    VIBECITY_CHECK(second_house == first_house + 1);
    VIBECITY_CHECK(game.save_to_file(path).success);

    vibecity::GameSession loaded;
    VIBECITY_CHECK(loaded.load_from_file(path).success);
    VIBECITY_CHECK(!loaded.simulation().buildings()[static_cast<std::size_t>(first_house - 1)].active);
    VIBECITY_CHECK(loaded.simulation().buildings()[static_cast<std::size_t>(second_house - 1)].active);
    VIBECITY_CHECK(loaded.simulation().building(second_house).kind == vibecity::BuildingKind::House);
    VIBECITY_CHECK(!loaded.simulation().map().can_place_building(
        vibecity::GridPosition{1, 1},
        vibecity::Footprint{1, 1}));

    auto threw = false;
    try {
        [[maybe_unused]] const auto& removed = loaded.simulation().building(first_house);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    VIBECITY_CHECK(threw);

    std::filesystem::remove(path);
}

void save_load_preserves_terrain_adjusted_construction_site()
{
    const auto path = std::filesystem::temp_directory_path() / "vibecity-terrain-construction.vcs";
    vibecity::GameSession game;

    const auto rocky_site = first_buildable_house_site_on(
        game.simulation(),
        vibecity::TerrainId::Rocky);
    const auto site = require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = rocky_site
    });
    VIBECITY_CHECK(game.simulation().building(site).inventory.capacity(vibecity::ResourceId::Stone) == 1);
    VIBECITY_CHECK(game.save_to_file(path).success);

    vibecity::GameSession loaded;
    VIBECITY_CHECK(loaded.load_from_file(path).success);
    VIBECITY_CHECK(loaded.simulation().building(site).kind == vibecity::BuildingKind::ConstructionSite);
    VIBECITY_CHECK(loaded.simulation().building(site).inventory.capacity(vibecity::ResourceId::Timber) == 12);
    VIBECITY_CHECK(loaded.simulation().building(site).inventory.capacity(vibecity::ResourceId::Stone) == 1);

    std::filesystem::remove(path);
}

void invalid_save_is_rejected_without_replacing_session()
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto valid_path = directory / "vibecity-valid.vcs";
    const auto corrupt_path = directory / "vibecity-corrupt.vcs";
    const auto version_path = directory / "vibecity-version.vcs";

    auto source = starting_village_session();
    [[maybe_unused]] const auto source_ids = vibecity::create_starting_village(source);
    require(source, vibecity::AdvanceTimeCommand{.ticks = 100});
    VIBECITY_CHECK(source.save_to_file(valid_path).success);

    auto corrupt = read_bytes(valid_path);
    VIBECITY_CHECK(corrupt.size() > 32);
    corrupt.back() ^= 0xffU;
    write_bytes(corrupt_path, corrupt);

    auto version = read_bytes(valid_path);
    VIBECITY_CHECK(version.size() > 12);
    version[8] = 10;
    write_bytes(version_path, version);

    auto target = starting_village_session();
    const auto target_ids = vibecity::create_starting_village(target);
    const auto tick_before = target.simulation().current_tick();
    const auto buildings_before = target.simulation().buildings().size();
    const auto storehouse_bread_before = target.simulation()
        .building(target_ids.storehouse)
        .inventory.quantity(vibecity::ResourceId::Bread);

    auto io = target.load_from_file(corrupt_path);
    VIBECITY_CHECK(!io.success);
    VIBECITY_CHECK(target.simulation().current_tick() == tick_before);
    VIBECITY_CHECK(target.simulation().buildings().size() == buildings_before);
    VIBECITY_CHECK(target.simulation().building(target_ids.storehouse)
            .inventory.quantity(vibecity::ResourceId::Bread)
        == storehouse_bread_before);

    io = target.load_from_file(version_path);
    VIBECITY_CHECK(!io.success);
    VIBECITY_CHECK(target.simulation().current_tick() == tick_before);

    std::filesystem::remove(valid_path);
    std::filesystem::remove(corrupt_path);
    std::filesystem::remove(version_path);
}

void save_load_preserves_capabilities()
{
    const auto path = std::filesystem::temp_directory_path() / "vibecity-capabilities.vcs";
    vibecity::GameSession game;
    game.simulation().grant_capability(vibecity::CapabilityId::Pottery);
    VIBECITY_CHECK(game.save_to_file(path).success);

    vibecity::GameSession loaded;
    VIBECITY_CHECK(loaded.load_from_file(path).success);
    VIBECITY_CHECK(loaded.simulation().has_capability(vibecity::CapabilityId::Pottery));
    VIBECITY_CHECK(!loaded.simulation().has_capability(vibecity::CapabilityId::Brickmaking));

    std::filesystem::remove(path);
}

void external_building_definition_runs_and_persists()
{
    const auto root = std::filesystem::temp_directory_path() / "vibecity-building-catalog-test";
    const auto definitions = root / "definitions";
    const auto changed_definitions = root / "changed-definitions";
    const auto malformed_definitions = root / "malformed-definitions";
    const auto save_path = root / "custom-building.vcs";
    std::filesystem::remove_all(root);

    copy_default_building_definitions(definitions);
    write_text(definitions / "06_charcoal_kiln.vbd", charcoal_kiln_definition(4));
    auto catalog = std::make_shared<const vibecity::BuildingCatalog>(
        vibecity::BuildingCatalog::load_directory(definitions));
    const auto kiln_kind = catalog->find_kind("charcoal_kiln");
    VIBECITY_CHECK(kiln_kind.has_value());
    VIBECITY_CHECK(static_cast<std::uint8_t>(*kiln_kind) >= vibecity::first_custom_building_kind);
    VIBECITY_CHECK(catalog->definition(*kiln_kind).name == "Charcoal Kiln");

    vibecity::GameSession game{catalog};
    const auto house = require_building(game, vibecity::PlaceBuildingCommand{
        .kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{1, 1}
    });
    const auto kiln = require_building(game, vibecity::PlaceBuildingCommand{
        .kind = *kiln_kind,
        .position = vibecity::GridPosition{4, 1}
    });
    for (int x = 1; x <= 5; ++x) {
        require(game, vibecity::PlacePathCommand{
            .position = vibecity::GridPosition{x, 0}
        });
    }
    require(game, vibecity::SetResidentsCommand{.building = house, .residents = 1});
    require(game, vibecity::AddInventoryCommand{
        .building = kiln,
        .resource = vibecity::ResourceId::Timber,
        .quantity = 2
    });
    require(game, vibecity::AdvanceTimeCommand{.ticks = 120});
    VIBECITY_CHECK(game.simulation().building(kiln).inventory.quantity(vibecity::ResourceId::Firewood) == 4);
    VIBECITY_CHECK(game.save_to_file(save_path).success);

    vibecity::GameSession loaded{catalog};
    VIBECITY_CHECK(loaded.load_from_file(save_path).success);
    VIBECITY_CHECK(loaded.simulation().building_catalog().stable_id(
            loaded.simulation().building(kiln).kind)
        == "charcoal_kiln");
    VIBECITY_CHECK(loaded.simulation().building(kiln).inventory.quantity(vibecity::ResourceId::Firewood) == 4);

    copy_default_building_definitions(changed_definitions);
    write_text(changed_definitions / "06_charcoal_kiln.vbd", charcoal_kiln_definition(5));
    auto changed_catalog = std::make_shared<const vibecity::BuildingCatalog>(
        vibecity::BuildingCatalog::load_directory(changed_definitions));
    vibecity::GameSession incompatible{changed_catalog};
    VIBECITY_CHECK(!incompatible.load_from_file(save_path).success);

    copy_default_building_definitions(malformed_definitions);
    write_text(
        malformed_definitions / "06_invalid.vbd",
        charcoal_kiln_definition(4) + "\n[storage]\nunobtainium = 1\n");
    auto rejected = false;
    try {
        [[maybe_unused]] const auto malformed =
            vibecity::BuildingCatalog::load_directory(malformed_definitions);
    } catch (const std::exception&) {
        rejected = true;
    }
    VIBECITY_CHECK(rejected);

    std::filesystem::remove_all(root);
}

void single_production_chain_cannot_reach_25_residents()
{
    auto game = starting_village_session();
    [[maybe_unused]] const auto ids = vibecity::create_starting_village(game);

    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Woodcutter,
        .position = vibecity::GridPosition{10, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Farm,
        .position = vibecity::GridPosition{13, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Bakery,
        .position = vibecity::GridPosition{16, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{19, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{21, vibecity::starting_village_building_y}
    });

    require(game, vibecity::AdvanceTimeCommand{.ticks = 20 * vibecity::ticks_per_day});

    VIBECITY_CHECK(game.simulation().total_residents() == 16);
    VIBECITY_CHECK(game.simulation().stored_bread() == 16);
    VIBECITY_CHECK(game.simulation().bread_required_for_population_growth() == 17);
    VIBECITY_CHECK(
        game.simulation().population_growth_blocker()
        == vibecity::PopulationGrowthBlocker::NotEnoughBread);
    VIBECITY_CHECK(!objective_status(
        game,
        vibecity::VillageObjectiveId::Stable25Residents).complete);
    VIBECITY_CHECK(total_hunger_days(game.simulation()) > 0);
}

void self_sufficient_village_reaches_25_residents()
{
    auto game = starting_village_session();
    const auto ids = vibecity::create_starting_village(game);
    VIBECITY_CHECK(ids.houses.size() == 3);
    const auto milestone = vibecity::queue_reference_village_milestone(game);
    const auto quarry_kind = game.simulation().building_catalog().find_kind("quarry");
    VIBECITY_CHECK(quarry_kind.has_value());

    for (int day = 0; day < 30; ++day) {
        require(game, vibecity::AdvanceTimeCommand{.ticks = vibecity::ticks_per_day});
    }

    const auto& simulation = game.simulation();
    VIBECITY_CHECK(simulation.building(milestone.woodcutter).kind == vibecity::BuildingKind::Woodcutter);
    VIBECITY_CHECK(simulation.building(milestone.farm).kind == vibecity::BuildingKind::Farm);
    VIBECITY_CHECK(simulation.building(milestone.bakery).kind == vibecity::BuildingKind::Bakery);
    VIBECITY_CHECK(simulation.building(milestone.quarry).kind == *quarry_kind);
    VIBECITY_CHECK(simulation.building(milestone.first_house).kind == vibecity::BuildingKind::House);
    VIBECITY_CHECK(simulation.building(milestone.second_house).kind == vibecity::BuildingKind::House);
    VIBECITY_CHECK(simulation.building(milestone.second_storehouse).kind == vibecity::BuildingKind::Storehouse);
    VIBECITY_CHECK(simulation.stats().constructed_buildings >= 8);
    VIBECITY_CHECK(simulation.stats().produced[vibecity::resource_index(vibecity::ResourceId::Stone)] >= 12);
    VIBECITY_CHECK(simulation.stats().transported[vibecity::resource_index(vibecity::ResourceId::Stone)] >= 12);
    VIBECITY_CHECK(simulation.total_residents() == 25);
    VIBECITY_CHECK(simulation.total_housing_capacity() == 25);
    VIBECITY_CHECK(simulation.free_housing_capacity() == 0);
    VIBECITY_CHECK(simulation.daily_bread_need() == 25);
    VIBECITY_CHECK(simulation.population_growth_blocker() == vibecity::PopulationGrowthBlocker::NoHousing);
    VIBECITY_CHECK(total_hunger_days(simulation) == 0);
    VIBECITY_CHECK(simulation.stored_bread() >= 25);
    VIBECITY_CHECK(simulation.bread_days_remaining() >= 1);
    const auto& bakery_definition = simulation.definition(vibecity::BuildingKind::Bakery);
    VIBECITY_CHECK(bakery_definition.recipe.has_value());
    VIBECITY_CHECK(
        bakery_definition.recipe->outputs[vibecity::resource_index(vibecity::ResourceId::Bread)]
            * vibecity::ticks_per_day / bakery_definition.recipe->cycle_minutes
        == 16);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Reach25Residents).complete);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::HaveQuarry).complete);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::HaveTwoStorehouses).complete);
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Stable25Residents).complete);
    VIBECITY_CHECK(game.objectives().completed_count() == static_cast<int>(vibecity::village_objective_count));
    VIBECITY_CHECK(game.objectives().all_complete());
    VIBECITY_CHECK(game.objectives().active_status() == nullptr);
}

}

int main()
{
    command_layer_places_path_and_building();
    command_layer_removes_path_and_demolishes_building();
    invalid_command_reports_failure();
    command_layer_rejects_locked_construction();
    starting_village_world_generation_supports_reference_route();
    starting_village_runs_through_command_layer();
    objectives_track_starting_village_status();
    objectives_count_stable_days_and_reset_on_hunger();
    save_load_round_trip_preserves_deterministic_session();
    save_load_preserves_objective_history();
    save_load_preserves_demolished_buildings();
    save_load_preserves_terrain_adjusted_construction_site();
    invalid_save_is_rejected_without_replacing_session();
    save_load_preserves_capabilities();
    external_building_definition_runs_and_persists();
    single_production_chain_cannot_reach_25_residents();
    self_sufficient_village_reaches_25_residents();

    std::cout << "game tests passed\n";
    return 0;
}
