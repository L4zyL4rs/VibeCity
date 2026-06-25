#include "core/Building.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <type_traits>

namespace vibecity {
namespace {

using Sections = std::map<std::string, std::map<std::string, std::string>>;
constexpr std::array<std::string_view, 7> allowed_sections{
    "building",
    "construction",
    "storage",
    "recipe",
    "gathering",
    "inputs",
    "outputs"
};

std::string trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string{value.substr(first, last - first + 1)};
}

[[noreturn]] void parse_error(
    const std::filesystem::path& path,
    int line,
    std::string_view message)
{
    throw std::runtime_error(
        path.string() + ":" + std::to_string(line) + ": " + std::string{message});
}

Sections parse_file(const std::filesystem::path& path)
{
    auto input = std::ifstream{path};
    if (!input) {
        throw std::runtime_error("could not open building definition: " + path.string());
    }

    auto sections = Sections{};
    auto section = std::string{};
    auto line_number = 0;
    for (auto line = std::string{}; std::getline(input, line);) {
        ++line_number;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = trim(std::string_view{line}.substr(1, line.size() - 2));
            if (section.empty()) {
                parse_error(path, line_number, "empty section name");
            }
            continue;
        }
        if (section.empty()) {
            parse_error(path, line_number, "key appears before a section");
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            parse_error(path, line_number, "expected key = value");
        }
        const auto key = trim(std::string_view{line}.substr(0, separator));
        const auto value = trim(std::string_view{line}.substr(separator + 1));
        if (key.empty() || value.empty()) {
            parse_error(path, line_number, "empty key or value");
        }
        if (!sections[section].emplace(key, value).second) {
            parse_error(path, line_number, "duplicate key");
        }
    }
    return sections;
}

std::string take_required(
    Sections& sections,
    std::string_view section,
    std::string_view key,
    const std::filesystem::path& path)
{
    auto section_found = sections.find(std::string{section});
    if (section_found == sections.end()) {
        throw std::runtime_error(path.string() + ": missing [" + std::string{section} + "] section");
    }
    auto key_found = section_found->second.find(std::string{key});
    if (key_found == section_found->second.end()) {
        throw std::runtime_error(
            path.string() + ": missing " + std::string{section} + "." + std::string{key});
    }
    auto value = std::move(key_found->second);
    section_found->second.erase(key_found);
    return value;
}

std::optional<std::string> take_optional(
    Sections& sections,
    std::string_view section,
    std::string_view key)
{
    auto section_found = sections.find(std::string{section});
    if (section_found == sections.end()) {
        return std::nullopt;
    }
    auto key_found = section_found->second.find(std::string{key});
    if (key_found == section_found->second.end()) {
        return std::nullopt;
    }
    auto value = std::move(key_found->second);
    section_found->second.erase(key_found);
    return value;
}

std::int64_t parse_integer(std::string_view value, const std::filesystem::path& path)
{
    auto result = std::int64_t{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::runtime_error(path.string() + ": invalid integer: " + std::string{value});
    }
    return result;
}

int required_int(
    Sections& sections,
    std::string_view section,
    std::string_view key,
    const std::filesystem::path& path,
    int minimum,
    int maximum)
{
    const auto value = parse_integer(take_required(sections, section, key, path), path);
    if (value < minimum || value > maximum) {
        throw std::runtime_error(path.string() + ": integer outside allowed range");
    }
    return static_cast<int>(value);
}

bool required_bool(
    Sections& sections,
    std::string_view section,
    std::string_view key,
    const std::filesystem::path& path)
{
    const auto value = take_required(sections, section, key, path);
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw std::runtime_error(path.string() + ": expected true or false");
}

std::array<int, 2> parse_pair(std::string_view value, const std::filesystem::path& path)
{
    const auto comma = value.find(',');
    if (comma == std::string_view::npos || value.find(',', comma + 1) != std::string_view::npos) {
        throw std::runtime_error(path.string() + ": expected two comma-separated integers");
    }
    const auto first = parse_integer(trim(value.substr(0, comma)), path);
    const auto second = parse_integer(trim(value.substr(comma + 1)), path);
    if (first <= 0 || first > 64 || second <= 0 || second > 64) {
        throw std::runtime_error(path.string() + ": invalid footprint");
    }
    return {static_cast<int>(first), static_cast<int>(second)};
}

std::array<std::uint8_t, 3> parse_color(std::string_view value, const std::filesystem::path& path)
{
    auto components = std::array<std::uint8_t, 3>{};
    auto start = std::size_t{0};
    for (std::size_t index = 0; index < components.size(); ++index) {
        const auto comma = index + 1 == components.size()
            ? std::string_view::npos
            : value.find(',', start);
        if ((index + 1 != components.size() && comma == std::string_view::npos)
            || (index + 1 == components.size() && value.find(',', start) != std::string_view::npos)) {
            throw std::runtime_error(path.string() + ": expected RGB color");
        }
        const auto component = parse_integer(
            trim(value.substr(start, comma == std::string_view::npos ? comma : comma - start)),
            path);
        if (component < 0 || component > 255) {
            throw std::runtime_error(path.string() + ": color component outside 0..255");
        }
        components[index] = static_cast<std::uint8_t>(component);
        start = comma == std::string_view::npos ? value.size() : comma + 1;
    }
    return components;
}

bool valid_stable_id(std::string_view id)
{
    if (id.empty() || id.front() < 'a' || id.front() > 'z') {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char character) {
        return (character >= 'a' && character <= 'z')
            || (character >= '0' && character <= '9')
            || character == '_';
    });
}

ResourceArray take_resource_section(
    Sections& sections,
    std::string_view section,
    const std::filesystem::path& path)
{
    auto result = empty_resources();
    auto found = sections.find(std::string{section});
    if (found == sections.end()) {
        return result;
    }
    for (const auto& [key, value] : found->second) {
        const auto resource = resource_id_from_string(key);
        if (!resource.has_value()) {
            throw std::runtime_error(path.string() + ": unknown resource ID: " + key);
        }
        const auto quantity = parse_integer(value, path);
        if (quantity < 0) {
            throw std::runtime_error(path.string() + ": resource amount cannot be negative");
        }
        result[resource_index(*resource)] = quantity;
    }
    found->second.clear();
    return result;
}

BuildingKind known_kind(std::string_view id)
{
    if (id == "house") {
        return BuildingKind::House;
    }
    if (id == "farm") {
        return BuildingKind::Farm;
    }
    if (id == "bakery") {
        return BuildingKind::Bakery;
    }
    if (id == "woodcutter") {
        return BuildingKind::Woodcutter;
    }
    if (id == "storehouse") {
        return BuildingKind::Storehouse;
    }
    if (id == "construction_site") {
        return BuildingKind::ConstructionSite;
    }
    return BuildingKind::Count;
}

ResourceSourcePolicy parse_source_policy(std::string_view value, const std::filesystem::path& path)
{
    if (value == "none") {
        return ResourceSourcePolicy::None;
    }
    if (value == "recipe_outputs") {
        return ResourceSourcePolicy::RecipeOutputs;
    }
    if (value == "all_stored") {
        return ResourceSourcePolicy::AllStored;
    }
    throw std::runtime_error(path.string() + ": invalid source_policy");
}

BuildingDefinition parse_definition(const std::filesystem::path& path)
{
    auto sections = parse_file(path);
    for (const auto& [section_name, values] : sections) {
        (void)values;
        if (std::find(allowed_sections.begin(), allowed_sections.end(), section_name)
            == allowed_sections.end()) {
            throw std::runtime_error(path.string() + ": unknown section [" + section_name + "]");
        }
    }

    auto definition = BuildingDefinition{};
    definition.stable_id = take_required(sections, "building", "id", path);
    definition.name = take_required(sections, "building", "name", path);
    if (!valid_stable_id(definition.stable_id) || definition.name.empty()) {
        throw std::runtime_error(path.string() + ": invalid building ID or name");
    }

    const auto footprint = parse_pair(
        take_required(sections, "building", "footprint", path),
        path);
    definition.footprint = Footprint{footprint[0], footprint[1]};
    definition.worker_slots = required_int(sections, "building", "worker_slots", path, 0, 1'000'000);
    definition.resident_capacity = required_int(sections, "building", "resident_capacity", path, 0, 1'000'000);
    definition.worker_supply = required_int(sections, "building", "worker_supply", path, 0, 1'000'000);
    definition.consumes_bread = required_bool(sections, "building", "consumes_bread", path);
    definition.requests_storage_inputs = required_bool(
        sections,
        "building",
        "requests_storage_inputs",
        path);
    definition.internal_construction_site = required_bool(
        sections,
        "building",
        "internal_construction_site",
        path);
    definition.source_policy = parse_source_policy(
        take_required(sections, "building", "source_policy", path),
        path);
    definition.map_color = parse_color(
        take_required(sections, "building", "map_color", path),
        path);
    definition.construction_labor_minutes = parse_integer(
        take_required(sections, "building", "construction_labor_minutes", path),
        path);
    if (definition.construction_labor_minutes < 0) {
        throw std::runtime_error(path.string() + ": construction labor cannot be negative");
    }

    definition.construction_materials = take_resource_section(sections, "construction", path);
    definition.storage = take_resource_section(sections, "storage", path);
    const auto inputs = take_resource_section(sections, "inputs", path);
    const auto outputs = take_resource_section(sections, "outputs", path);
    const auto cycle = take_optional(sections, "recipe", "cycle_minutes");
    const auto has_recipe_amounts = std::any_of(inputs.begin(), inputs.end(), [](Quantity amount) {
        return amount > 0;
    }) || std::any_of(outputs.begin(), outputs.end(), [](Quantity amount) {
        return amount > 0;
    });
    if (cycle.has_value() || has_recipe_amounts) {
        if (!cycle.has_value()) {
            throw std::runtime_error(path.string() + ": recipe amounts require cycle_minutes");
        }
        const auto cycle_minutes = parse_integer(*cycle, path);
        if (cycle_minutes <= 0) {
            throw std::runtime_error(path.string() + ": recipe cycle must be positive");
        }
        definition.recipe = Recipe{
            .inputs = inputs,
            .outputs = outputs,
            .cycle_minutes = cycle_minutes
        };
    }

    const auto gathering_resource = take_optional(sections, "gathering", "map_resource");
    const auto gathering_radius = take_optional(sections, "gathering", "radius");
    const auto gathering_units = take_optional(sections, "gathering", "units_per_cycle");
    const auto has_any_gathering_field = gathering_resource.has_value()
        || gathering_radius.has_value()
        || gathering_units.has_value();
    if (has_any_gathering_field) {
        if (!gathering_resource.has_value()
            || !gathering_radius.has_value()
            || !gathering_units.has_value()) {
            throw std::runtime_error(
                path.string()
                + ": gathering requires map_resource, radius, and units_per_cycle");
        }
        const auto resource = map_resource_id_from_string(*gathering_resource);
        if (!resource.has_value()) {
            throw std::runtime_error(path.string() + ": unknown map resource ID");
        }
        const auto radius = parse_integer(*gathering_radius, path);
        const auto units = parse_integer(*gathering_units, path);
        if (radius <= 0 || radius > 64 || units <= 0 || units > 1'000'000) {
            throw std::runtime_error(path.string() + ": invalid gathering rule");
        }
        definition.gathering = GatheringRule{
            .resource = *resource,
            .radius = static_cast<int>(radius),
            .units_per_cycle = units
        };
    }

    for (const auto& [section_name, values] : sections) {
        if (!values.empty()) {
            throw std::runtime_error(
                path.string() + ": unknown key " + section_name + "." + values.begin()->first);
        }
    }

    if (definition.internal_construction_site) {
        if (definition.stable_id != "construction_site"
            || definition.recipe.has_value()
            || definition.worker_slots != 0
            || definition.resident_capacity != 0
            || definition.worker_supply != 0
            || definition.construction_labor_minutes != 0) {
            throw std::runtime_error(path.string() + ": invalid internal construction site");
        }
    } else if (definition.construction_labor_minutes <= 0) {
        throw std::runtime_error(path.string() + ": buildable definition needs construction labor");
    }
    if (definition.consumes_bread
        && (definition.resident_capacity <= 0
            || definition.storage[resource_index(ResourceId::Bread)] <= 0)) {
        throw std::runtime_error(path.string() + ": bread consumer needs residents and bread storage");
    }
    if (definition.source_policy == ResourceSourcePolicy::RecipeOutputs
        && (!definition.recipe.has_value()
            || !std::any_of(
                definition.recipe->outputs.begin(),
                definition.recipe->outputs.end(),
                [](Quantity amount) { return amount > 0; }))) {
        throw std::runtime_error(path.string() + ": recipe output source needs a recipe");
    }
    if (definition.recipe.has_value()) {
        for (std::size_t index = 0; index < resource_count; ++index) {
            if (definition.recipe->inputs[index] > definition.storage[index]
                || definition.recipe->outputs[index] > definition.storage[index]) {
                throw std::runtime_error(path.string() + ": recipe amount exceeds storage capacity");
            }
        }
    }
    if (definition.gathering.has_value() && !definition.recipe.has_value()) {
        throw std::runtime_error(path.string() + ": gathering requires a recipe");
    }
    if (definition.source_policy == ResourceSourcePolicy::AllStored) {
        definition.source_mask =
            static_cast<std::uint8_t>((std::uint16_t{1} << resource_count) - 1);
    } else if (definition.source_policy == ResourceSourcePolicy::RecipeOutputs) {
        for (std::size_t index = 0; index < resource_count; ++index) {
            if (definition.recipe->outputs[index] > 0) {
                definition.source_mask |=
                    static_cast<std::uint8_t>(std::uint8_t{1} << index);
            }
        }
    }
    return definition;
}

void hash_byte(std::uint64_t& hash, std::uint8_t value)
{
    constexpr auto prime = std::uint64_t{1'099'511'628'211ULL};
    hash ^= value;
    hash *= prime;
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value)
{
    using Unsigned = std::make_unsigned_t<Integer>;
    auto remaining = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Unsigned); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(remaining & 0xffU));
        remaining >>= 8U;
    }
}

void hash_string(std::uint64_t& hash, std::string_view value)
{
    hash_integer(hash, static_cast<std::uint64_t>(value.size()));
    for (const auto character : value) {
        hash_byte(hash, static_cast<std::uint8_t>(character));
    }
}

void hash_resource_array(std::uint64_t& hash, const ResourceArray& amounts)
{
    for (const auto amount : amounts) {
        hash_integer(hash, amount);
    }
}

std::uint64_t catalog_fingerprint(const std::vector<BuildingDefinition>& definitions)
{
    constexpr auto offset_basis = std::uint64_t{14'695'981'039'346'656'037ULL};
    auto sorted = std::vector<const BuildingDefinition*>{};
    sorted.reserve(definitions.size());
    for (const auto& definition : definitions) {
        sorted.push_back(&definition);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto* left, const auto* right) {
        return left->stable_id < right->stable_id;
    });

    auto hash = offset_basis;
    for (const auto* definition : sorted) {
        hash_string(hash, definition->stable_id);
        hash_integer(hash, definition->footprint.width);
        hash_integer(hash, definition->footprint.height);
        hash_integer(hash, definition->worker_slots);
        hash_integer(hash, definition->resident_capacity);
        hash_integer(hash, definition->worker_supply);
        hash_integer(hash, static_cast<std::uint8_t>(definition->consumes_bread));
        hash_resource_array(hash, definition->construction_materials);
        hash_integer(hash, definition->construction_labor_minutes);
        hash_resource_array(hash, definition->storage);
        hash_integer(hash, static_cast<std::uint8_t>(definition->recipe.has_value()));
        if (definition->recipe.has_value()) {
            hash_resource_array(hash, definition->recipe->inputs);
            hash_resource_array(hash, definition->recipe->outputs);
            hash_integer(hash, definition->recipe->cycle_minutes);
        }
        hash_integer(hash, static_cast<std::uint8_t>(definition->gathering.has_value()));
        if (definition->gathering.has_value()) {
            hash_integer(hash, static_cast<std::uint8_t>(definition->gathering->resource));
            hash_integer(hash, definition->gathering->radius);
            hash_integer(hash, definition->gathering->units_per_cycle);
        }
        hash_integer(hash, static_cast<std::uint8_t>(definition->source_policy));
        hash_integer(hash, static_cast<std::uint8_t>(definition->requests_storage_inputs));
        hash_integer(hash, static_cast<std::uint8_t>(definition->internal_construction_site));
    }
    return hash;
}

}

BuildingCatalog BuildingCatalog::load_directory(const std::filesystem::path& directory)
{
    auto error = std::error_code{};
    if (!std::filesystem::is_directory(directory, error) || error) {
        throw std::runtime_error("building definition directory is unavailable: " + directory.string());
    }

    auto files = std::vector<std::filesystem::path>{};
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".vbd") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        throw std::runtime_error("building definition directory contains no .vbd files");
    }

    auto catalog = BuildingCatalog{};
    auto next_custom_kind = first_custom_building_kind;
    for (const auto& file : files) {
        auto definition = parse_definition(file);
        if (catalog.kinds_by_stable_id_.contains(definition.stable_id)) {
            throw std::runtime_error("duplicate building stable ID: " + definition.stable_id);
        }

        const auto fixed_kind = known_kind(definition.stable_id);
        if (fixed_kind != BuildingKind::Count) {
            definition.kind = fixed_kind;
        } else {
            if (next_custom_kind == std::numeric_limits<std::uint8_t>::max()) {
                throw std::runtime_error("too many custom building definitions");
            }
            definition.kind = static_cast<BuildingKind>(next_custom_kind++);
        }
        const auto kind_value = static_cast<std::uint8_t>(definition.kind);
        if (catalog.indices_by_kind_[kind_value] >= 0) {
            throw std::runtime_error("duplicate building kind value");
        }
        catalog.kinds_by_stable_id_.emplace(definition.stable_id, definition.kind);
        catalog.indices_by_kind_[kind_value] = static_cast<int>(catalog.definitions_.size());
        catalog.definitions_.push_back(std::move(definition));
    }

    for (const auto required : {
             "house",
             "farm",
             "bakery",
             "woodcutter",
             "storehouse",
             "construction_site"}) {
        if (!catalog.find_kind(required).has_value()) {
            throw std::runtime_error("missing required building definition: " + std::string{required});
        }
    }
    catalog.fingerprint_ = catalog_fingerprint(catalog.definitions_);
    return catalog;
}

}
