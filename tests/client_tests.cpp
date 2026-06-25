#include "TestSupport.hpp"

#include "client/BuildMenu.hpp"

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
        == "NEEDS 8 TIMBER");
    VIBECITY_CHECK(vibecity::client::construction_cost_text(
            catalog->definition(vibecity::BuildingKind::Bakery))
        == "NEEDS 12 TIMBER + 1 TOOLS");
}

void build_menu_hit_testing_respects_rows_gaps_and_scroll()
{
    const auto catalog = vibecity::default_building_catalog();
    constexpr auto first_row_y = 95;
    constexpr auto first_gap_y = 173;
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

}

int main()
{
    build_menu_lists_catalog_buildings();
    build_menu_includes_custom_data_only_building();
    build_menu_formats_construction_materials();
    build_menu_hit_testing_respects_rows_gaps_and_scroll();

    std::cout << "client tests passed\n";
    return 0;
}
