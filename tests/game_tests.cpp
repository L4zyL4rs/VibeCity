#include "TestSupport.hpp"

#include "game/GameSession.hpp"
#include "game/Scenario.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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

void invalid_command_reports_failure()
{
    vibecity::GameSession game;

    const auto result = game.execute(vibecity::AdvanceTimeCommand{.ticks = -1});
    VIBECITY_CHECK(!result.success);
}

void starting_village_runs_through_command_layer()
{
    vibecity::GameSession game;
    const auto ids = vibecity::create_starting_village(game);

    VIBECITY_CHECK(ids.houses.size() == 3);
    VIBECITY_CHECK(!ids.woodcutter.has_value());
    VIBECITY_CHECK(!ids.farm.has_value());
    VIBECITY_CHECK(!ids.bakery.has_value());
    VIBECITY_CHECK(game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Bread) == 60);
    VIBECITY_CHECK(game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Timber) == 8);
    VIBECITY_CHECK(game.simulation().building(ids.storehouse).inventory.quantity(vibecity::ResourceId::Tools) == 2);
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
    vibecity::GameSession game;

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
    VIBECITY_CHECK(!objective_status(game, vibecity::VillageObjectiveId::HaveBakery).complete);
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

    vibecity::GameSession original;
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

    auto io = original.save_to_file(first_save);
    VIBECITY_CHECK(io.success);
    io = original.save_to_file(first_save);
    VIBECITY_CHECK(io.success);

    vibecity::GameSession loaded;
    io = loaded.load_from_file(first_save);
    VIBECITY_CHECK(io.success);
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

void invalid_save_is_rejected_without_replacing_session()
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto valid_path = directory / "vibecity-valid.vcs";
    const auto corrupt_path = directory / "vibecity-corrupt.vcs";
    const auto version_path = directory / "vibecity-version.vcs";

    vibecity::GameSession source;
    [[maybe_unused]] const auto source_ids = vibecity::create_starting_village(source);
    require(source, vibecity::AdvanceTimeCommand{.ticks = 100});
    VIBECITY_CHECK(source.save_to_file(valid_path).success);

    auto corrupt = read_bytes(valid_path);
    VIBECITY_CHECK(corrupt.size() > 32);
    corrupt.back() ^= 0xffU;
    write_bytes(corrupt_path, corrupt);

    auto version = read_bytes(valid_path);
    VIBECITY_CHECK(version.size() > 12);
    version[8] = 3;
    write_bytes(version_path, version);

    vibecity::GameSession target;
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
    vibecity::GameSession game;
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

    VIBECITY_CHECK(game.simulation().total_residents() < 25);
    VIBECITY_CHECK(!objective_status(
        game,
        vibecity::VillageObjectiveId::Stable25Residents).complete);
    VIBECITY_CHECK(total_hunger_days(game.simulation()) > 0);
}

void self_sufficient_village_reaches_25_residents()
{
    vibecity::GameSession game;
    const auto ids = vibecity::create_starting_village(game);
    VIBECITY_CHECK(ids.houses.size() == 3);

    const auto woodcutter = require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Woodcutter,
        .position = vibecity::GridPosition{10, vibecity::starting_village_building_y}
    });
    const auto farm = require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Farm,
        .position = vibecity::GridPosition{13, vibecity::starting_village_building_y}
    });
    const auto bakery = require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Bakery,
        .position = vibecity::GridPosition{16, vibecity::starting_village_building_y}
    });
    const auto first_house = require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{19, vibecity::starting_village_building_y}
    });
    const auto second_house = require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::House,
        .position = vibecity::GridPosition{21, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Woodcutter,
        .position = vibecity::GridPosition{23, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Farm,
        .position = vibecity::GridPosition{26, vibecity::starting_village_building_y}
    });
    require_building(game, vibecity::PlaceConstructionCommand{
        .target_kind = vibecity::BuildingKind::Bakery,
        .position = vibecity::GridPosition{29, vibecity::starting_village_building_y}
    });

    for (int day = 0; day < 20; ++day) {
        require(game, vibecity::AdvanceTimeCommand{.ticks = vibecity::ticks_per_day});
    }

    const auto& simulation = game.simulation();
    VIBECITY_CHECK(simulation.building(woodcutter).kind == vibecity::BuildingKind::Woodcutter);
    VIBECITY_CHECK(simulation.building(farm).kind == vibecity::BuildingKind::Farm);
    VIBECITY_CHECK(simulation.building(bakery).kind == vibecity::BuildingKind::Bakery);
    VIBECITY_CHECK(simulation.building(first_house).kind == vibecity::BuildingKind::House);
    VIBECITY_CHECK(simulation.building(second_house).kind == vibecity::BuildingKind::House);
    VIBECITY_CHECK(simulation.stats().constructed_buildings >= 5);
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
    VIBECITY_CHECK(objective_status(game, vibecity::VillageObjectiveId::Stable25Residents).complete);
    VIBECITY_CHECK(game.objectives().completed_count() == static_cast<int>(vibecity::village_objective_count));
    VIBECITY_CHECK(game.objectives().all_complete());
    VIBECITY_CHECK(game.objectives().active_status() == nullptr);
}

}

int main()
{
    command_layer_places_path_and_building();
    invalid_command_reports_failure();
    starting_village_runs_through_command_layer();
    objectives_track_starting_village_status();
    objectives_count_stable_days_and_reset_on_hunger();
    save_load_round_trip_preserves_deterministic_session();
    save_load_preserves_objective_history();
    invalid_save_is_rejected_without_replacing_session();
    external_building_definition_runs_and_persists();
    single_production_chain_cannot_reach_25_residents();
    self_sufficient_village_reaches_25_residents();

    std::cout << "game tests passed\n";
    return 0;
}
