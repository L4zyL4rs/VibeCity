#include "TestSupport.hpp"

#include "client/BuildMenu.hpp"
#include "client/InputController.hpp"
#include "client/Inspector.hpp"
#include "client/MapView.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

void write_text(const std::filesystem::path& path, std::string_view text)
{
    auto output = std::ofstream{path, std::ios::trunc};
    VIBECITY_CHECK(output.good());
    output << text;
    VIBECITY_CHECK(output.good());
}

void copy_default_definitions(const std::filesystem::path& destination)
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

void build_menu_lists_catalog_buildings()
{
    const auto catalog = vibecity::default_building_catalog();
    const auto kinds = vibecity::client::build_menu_kinds(*catalog);

    VIBECITY_CHECK(kinds.size() == 5);
    VIBECITY_CHECK(kinds.front() == vibecity::BuildingKind::House);
    VIBECITY_CHECK(kinds.back() == vibecity::BuildingKind::Storehouse);
    for (const auto kind : kinds) {
        VIBECITY_CHECK(!catalog->definition(kind).internal_construction_site);
    }
}

void build_menu_includes_custom_data_only_building()
{
    const auto directory =
        std::filesystem::temp_directory_path() / "vibecity-client-catalog-test";
    std::filesystem::remove_all(directory);
    copy_default_definitions(directory);
    write_text(directory / "06_charcoal_kiln.vbd", R"([building]
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
firewood = 4
)");

    const auto catalog = vibecity::BuildingCatalog::load_directory(directory);
    const auto custom_kind = catalog.find_kind("charcoal_kiln");
    VIBECITY_CHECK(custom_kind.has_value());

    const auto kinds = vibecity::client::build_menu_kinds(catalog);
    VIBECITY_CHECK(std::find(kinds.begin(), kinds.end(), *custom_kind) != kinds.end());
    VIBECITY_CHECK(vibecity::client::construction_cost_text(catalog.definition(*custom_kind))
        == "NEEDS 4 TIMBER");

    std::filesystem::remove_all(directory);
}

void build_menu_formats_construction_materials()
{
    const auto catalog = vibecity::default_building_catalog();

    VIBECITY_CHECK(vibecity::client::construction_cost_text(
            catalog->definition(vibecity::BuildingKind::House))
        == "NEEDS 12 TIMBER");
    VIBECITY_CHECK(vibecity::client::construction_cost_text(
            catalog->definition(vibecity::BuildingKind::Bakery))
        == "NEEDS 14 TIMBER + 1 TOOLS");
    VIBECITY_CHECK(vibecity::client::operation_summary_text(
            catalog->definition(vibecity::BuildingKind::Bakery))
        == "6 GRAIN + 2 FIREWOOD -> 4 BREAD / 6H");
    VIBECITY_CHECK(vibecity::client::operation_summary_text(
            catalog->definition(vibecity::BuildingKind::Farm))
        == "PRODUCES 12 GRAIN / 8H");
    VIBECITY_CHECK(vibecity::client::operation_summary_text(
            catalog->definition(vibecity::BuildingKind::Woodcutter))
        == "HARVESTS FOREST -> 1 TIMBER + 3 FIREWOOD / 6H");
}

void build_menu_hit_testing_respects_rows_gaps_and_scroll()
{
    const auto catalog = vibecity::default_building_catalog();
    constexpr auto first_row_y = 95;
    constexpr auto first_gap_y = 191;
    constexpr auto viewport_height = 800;

    VIBECITY_CHECK(vibecity::client::build_menu_kind_at(
            *catalog,
            first_row_y,
            viewport_height,
            0)
        == vibecity::BuildingKind::House);
    VIBECITY_CHECK(!vibecity::client::build_menu_kind_at(
            *catalog,
            first_gap_y,
            viewport_height,
            0)
        .has_value());
    VIBECITY_CHECK(vibecity::client::build_menu_kind_at(
            *catalog,
            first_row_y,
            viewport_height,
            vibecity::client::build_menu_scroll_step)
        == vibecity::BuildingKind::Farm);
}

void zoom_keeps_the_cursor_over_the_same_tile()
{
    auto camera = vibecity::client::Camera{};
    constexpr auto screen_x = 480;
    constexpr auto screen_y = 232;
    const auto before = vibecity::client::screen_to_map(screen_x, screen_y, camera);

    vibecity::client::zoom_camera_at(camera, screen_x, screen_y, 4);

    VIBECITY_CHECK(camera.tile_size == 24);
    VIBECITY_CHECK(vibecity::client::screen_to_map(screen_x, screen_y, camera) == before);
    const auto rect = vibecity::client::tile_rect(
        before,
        vibecity::Footprint{1, 1},
        camera);
    VIBECITY_CHECK(rect.x == screen_x);
    VIBECITY_CHECK(rect.y == screen_y);
}

void gathering_preview_counts_only_resource_that_survives_placement()
{
    auto simulation = vibecity::Simulation{};
    const auto target = vibecity::BuildingKind::Woodcutter;
    const auto tile = vibecity::GridPosition{40, 40};
    const auto& definition = simulation.definition(target);
    VIBECITY_CHECK(definition.gathering.has_value());
    const auto& gathering = *definition.gathering;

    for (const auto position : simulation.map().tiles_within_radius(
             tile,
             definition.footprint,
             gathering.radius)) {
        VIBECITY_CHECK(simulation.set_map_resource(
            position,
            vibecity::MapResourceId::Forest,
            0));
    }

    VIBECITY_CHECK(simulation.set_map_resource(
        vibecity::GridPosition{40, 40},
        vibecity::MapResourceId::Forest,
        6));
    VIBECITY_CHECK(simulation.set_map_resource(
        vibecity::GridPosition{39, 40},
        vibecity::MapResourceId::Forest,
        5));
    VIBECITY_CHECK(simulation.set_map_resource(
        vibecity::GridPosition{47, 41},
        vibecity::MapResourceId::Forest,
        4));
    VIBECITY_CHECK(simulation.set_map_resource(
        vibecity::GridPosition{47, 42},
        vibecity::MapResourceId::Forest,
        6));

    VIBECITY_CHECK(vibecity::client::gathering_resource_quantity_for_placement(
            simulation,
            target,
            tile)
        == 9);
    VIBECITY_CHECK(vibecity::client::gathering_resource_quantity_for_placement(
            simulation,
            vibecity::BuildingKind::House,
            tile)
        == 0);
}

void map_hover_text_reports_tile_contents()
{
    auto simulation = vibecity::Simulation{};
    const auto resource_tile = vibecity::GridPosition{1, 1};
    VIBECITY_CHECK(simulation.set_map_resource(
        resource_tile,
        vibecity::MapResourceId::Forest,
        3));
    VIBECITY_CHECK(
        vibecity::client::tile_inspection_text(simulation, resource_tile)
        == "tile 1,1 grass forest: 3");

    const auto path_tile = vibecity::GridPosition{2, 2};
    VIBECITY_CHECK(simulation.add_path(path_tile));
    VIBECITY_CHECK(
        vibecity::client::tile_inspection_text(simulation, path_tile)
        == "tile 2,2 grass path");

    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{3, 3});
    VIBECITY_CHECK(house == 1);
    VIBECITY_CHECK(
        vibecity::client::tile_inspection_text(
            simulation,
            vibecity::GridPosition{3, 3})
        == "tile 3,3 grass #1 House");

    const auto deposits = simulation.map().map_resource_deposits();
    const auto stone = std::find_if(
        deposits.begin(),
        deposits.end(),
        [](const vibecity::MapResourceDeposit& deposit) {
            return deposit.resource == vibecity::MapResourceId::Stone;
        });
    VIBECITY_CHECK(stone != deposits.end());
    const auto stone_text = vibecity::client::tile_inspection_text(
        simulation,
        stone->position);
    VIBECITY_CHECK(stone_text.find("rocky stone:") != std::string::npos);
}

void placement_blocker_text_reports_common_blocks()
{
    auto simulation = vibecity::Simulation{};
    VIBECITY_CHECK(vibecity::client::placement_blocker_text(
            simulation,
            vibecity::GridPosition{-1, 1},
            vibecity::Footprint{1, 1})
        == "outside map");

    const auto path_tile = vibecity::GridPosition{2, 2};
    VIBECITY_CHECK(simulation.add_path(path_tile));
    VIBECITY_CHECK(vibecity::client::placement_blocker_text(
            simulation,
            path_tile,
            vibecity::Footprint{1, 1})
        == "blocked by path");

    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{3, 3});
    VIBECITY_CHECK(house == 1);
    VIBECITY_CHECK(vibecity::client::placement_blocker_text(
            simulation,
            vibecity::GridPosition{3, 3},
            vibecity::Footprint{1, 1})
        == "blocked by #1 House");
}

void selected_logistics_lines_show_routes_and_reservations()
{
    auto simulation = vibecity::Simulation{};
    for (int x = 1; x <= 8; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 1}));
    }

    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{1, 2});
    const auto storehouse = simulation.add_building_at(
        vibecity::BuildingKind::Storehouse,
        vibecity::GridPosition{5, 2});
    simulation.set_residents(house, 1);
    VIBECITY_CHECK(
        simulation.building(storehouse).inventory.add(vibecity::ResourceId::Bread, 5));

    simulation.tick();
    VIBECITY_CHECK(!simulation.transport_jobs().empty());

    const auto house_lines = vibecity::client::selected_logistics_lines(simulation, house);
    VIBECITY_CHECK(std::find(
            house_lines.begin(),
            house_lines.end(),
            "RESERVED IN: 5 BREAD")
        != house_lines.end());
    VIBECITY_CHECK(std::find(
            house_lines.begin(),
            house_lines.end(),
            "SUPPLIERS: 1  ACTIVE IN: 5")
        != house_lines.end());
    VIBECITY_CHECK(std::any_of(
        house_lines.begin(),
        house_lines.end(),
        [](const std::string& line) {
            return line.find("IN 5 BREAD FROM #2 Storehouse PICKUP") != std::string::npos;
        }));

    const auto storehouse_lines =
        vibecity::client::selected_logistics_lines(simulation, storehouse);
    VIBECITY_CHECK(std::find(
            storehouse_lines.begin(),
            storehouse_lines.end(),
            "RESERVED OUT: 5 BREAD")
        != storehouse_lines.end());
    VIBECITY_CHECK(std::find(
            storehouse_lines.begin(),
            storehouse_lines.end(),
            "CUSTOMERS: 1  ACTIVE OUT: 5")
        != storehouse_lines.end());
    VIBECITY_CHECK(std::any_of(
        storehouse_lines.begin(),
        storehouse_lines.end(),
        [](const std::string& line) {
            return line.find("OUT 5 BREAD TO #1 House PICKUP") != std::string::npos;
        }));
}

void escape_cancels_before_clearing_selection()
{
    auto state = vibecity::client::ClientInteractionState{};
    state.mode = vibecity::client::ClientMode::Build;
    state.build_target = vibecity::BuildingKind::House;
    state.selected = 4;
    state.path_dragging = true;

    vibecity::client::cancel_interaction(state);

    VIBECITY_CHECK(state.mode == vibecity::client::ClientMode::Select);
    VIBECITY_CHECK(!state.build_target.has_value());
    VIBECITY_CHECK(state.selected == 4);
    VIBECITY_CHECK(!state.path_dragging);
    VIBECITY_CHECK(!state.quit);

    vibecity::client::cancel_interaction(state);
    VIBECITY_CHECK(!state.selected.has_value());
    VIBECITY_CHECK(!state.quit);

    state.mode = vibecity::client::ClientMode::Demolish;
    state.selected = 8;
    vibecity::client::cancel_interaction(state);
    VIBECITY_CHECK(state.mode == vibecity::client::ClientMode::Select);
    VIBECITY_CHECK(state.selected == 8);
    VIBECITY_CHECK(state.status == "demolition cancelled");
}

void transport_overlay_retains_completed_jobs_for_readability()
{
    auto simulation = vibecity::Simulation{};
    for (int x = 1; x <= 8; ++x) {
        VIBECITY_CHECK(simulation.add_path(vibecity::GridPosition{x, 1}));
    }

    const auto house = simulation.add_building_at(
        vibecity::BuildingKind::House,
        vibecity::GridPosition{1, 2});
    const auto storehouse = simulation.add_building_at(
        vibecity::BuildingKind::Storehouse,
        vibecity::GridPosition{5, 2});
    simulation.set_residents(house, 1);
    VIBECITY_CHECK(
        simulation.building(storehouse).inventory.add(vibecity::ResourceId::Bread, 5));

    simulation.tick();
    VIBECITY_CHECK(!simulation.transport_jobs().empty());

    auto overlay = vibecity::client::TransportOverlay{};
    overlay.update(simulation);
    VIBECITY_CHECK(overlay.visual_count() == 1);
    VIBECITY_CHECK(overlay.visual_count_for_building(house) == 1);
    VIBECITY_CHECK(overlay.visual_count_for_building(storehouse) == 1);
    VIBECITY_CHECK(overlay.visual_count_for_building(storehouse + 100) == 0);

    for (int attempt = 0;
         attempt < 100 && !simulation.transport_jobs().empty();
         ++attempt) {
        simulation.tick();
    }
    VIBECITY_CHECK(simulation.transport_jobs().empty());

    overlay.update(simulation);
    VIBECITY_CHECK(overlay.visual_count() == 1);
    for (int frame = 0; frame < 100; ++frame) {
        overlay.update(simulation);
    }
    VIBECITY_CHECK(overlay.visual_count() == 0);
}

}

int main()
{
    build_menu_lists_catalog_buildings();
    build_menu_includes_custom_data_only_building();
    build_menu_formats_construction_materials();
    build_menu_hit_testing_respects_rows_gaps_and_scroll();
    zoom_keeps_the_cursor_over_the_same_tile();
    gathering_preview_counts_only_resource_that_survives_placement();
    map_hover_text_reports_tile_contents();
    placement_blocker_text_reports_common_blocks();
    selected_logistics_lines_show_routes_and_reservations();
    escape_cancels_before_clearing_selection();
    transport_overlay_retains_completed_jobs_for_readability();

    std::cout << "client tests passed\n";
    return 0;
}
