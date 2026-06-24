#include "core/Inventory.hpp"

#include <stdexcept>

namespace vibecity {

Inventory::Inventory() = default;

Inventory::Inventory(const ResourceArray& capacities)
    : capacities_(capacities)
{
}

void Inventory::set_capacity(ResourceId resource, Quantity capacity)
{
    if (capacity < 0) {
        capacity = 0;
    }
    capacities_[resource_index(resource)] = capacity;
    if (quantities_[resource_index(resource)] > capacity) {
        quantities_[resource_index(resource)] = capacity;
    }
}

Quantity Inventory::quantity(ResourceId resource) const
{
    return quantities_[resource_index(resource)];
}

Quantity Inventory::capacity(ResourceId resource) const
{
    return capacities_[resource_index(resource)];
}

Quantity Inventory::reserved_outgoing(ResourceId resource) const
{
    return reserved_outgoing_[resource_index(resource)];
}

Quantity Inventory::reserved_incoming(ResourceId resource) const
{
    return reserved_incoming_[resource_index(resource)];
}

Quantity Inventory::available(ResourceId resource) const
{
    return quantity(resource) - reserved_outgoing(resource);
}

Quantity Inventory::free_capacity(ResourceId resource) const
{
    return capacity(resource) - quantity(resource) - reserved_incoming(resource);
}

bool Inventory::can_add(ResourceId resource, Quantity quantity) const
{
    return quantity >= 0 && free_capacity(resource) >= quantity;
}

bool Inventory::can_remove(ResourceId resource, Quantity quantity) const
{
    return quantity >= 0 && available(resource) >= quantity;
}

bool Inventory::can_reserve_outgoing(ResourceId resource, Quantity quantity) const
{
    return can_remove(resource, quantity);
}

bool Inventory::can_reserve_incoming(ResourceId resource, Quantity quantity) const
{
    return can_add(resource, quantity);
}

bool Inventory::add(ResourceId resource, Quantity quantity)
{
    if (!can_add(resource, quantity)) {
        return false;
    }
    quantities_[resource_index(resource)] += quantity;
    return true;
}

bool Inventory::remove(ResourceId resource, Quantity quantity)
{
    if (!can_remove(resource, quantity)) {
        return false;
    }
    quantities_[resource_index(resource)] -= quantity;
    return true;
}

bool Inventory::reserve_outgoing(ResourceId resource, Quantity quantity)
{
    if (!can_reserve_outgoing(resource, quantity)) {
        return false;
    }
    reserved_outgoing_[resource_index(resource)] += quantity;
    return true;
}

bool Inventory::release_outgoing(ResourceId resource, Quantity quantity)
{
    if (quantity < 0 || reserved_outgoing(resource) < quantity) {
        return false;
    }
    reserved_outgoing_[resource_index(resource)] -= quantity;
    return true;
}

bool Inventory::consume_reserved_outgoing(ResourceId resource, Quantity quantity)
{
    if (quantity < 0 || reserved_outgoing(resource) < quantity || this->quantity(resource) < quantity) {
        return false;
    }
    reserved_outgoing_[resource_index(resource)] -= quantity;
    quantities_[resource_index(resource)] -= quantity;
    return true;
}

bool Inventory::reserve_incoming(ResourceId resource, Quantity quantity)
{
    if (!can_reserve_incoming(resource, quantity)) {
        return false;
    }
    reserved_incoming_[resource_index(resource)] += quantity;
    return true;
}

bool Inventory::release_incoming(ResourceId resource, Quantity quantity)
{
    if (quantity < 0 || reserved_incoming(resource) < quantity) {
        return false;
    }
    reserved_incoming_[resource_index(resource)] -= quantity;
    return true;
}

bool Inventory::commit_reserved_incoming(ResourceId resource, Quantity quantity)
{
    if (quantity < 0 || reserved_incoming(resource) < quantity) {
        return false;
    }
    reserved_incoming_[resource_index(resource)] -= quantity;
    if (!can_add(resource, quantity)) {
        reserved_incoming_[resource_index(resource)] += quantity;
        return false;
    }
    quantities_[resource_index(resource)] += quantity;
    return true;
}

const ResourceArray& Inventory::quantities() const
{
    return quantities_;
}

const ResourceArray& Inventory::capacities() const
{
    return capacities_;
}

InventoryState Inventory::state() const
{
    return InventoryState{
        .quantities = quantities_,
        .capacities = capacities_,
        .reserved_outgoing = reserved_outgoing_,
        .reserved_incoming = reserved_incoming_
    };
}

Inventory Inventory::from_state(const InventoryState& state)
{
    for (std::size_t index = 0; index < resource_count; ++index) {
        const auto capacity = state.capacities[index];
        const auto quantity = state.quantities[index];
        const auto outgoing = state.reserved_outgoing[index];
        const auto incoming = state.reserved_incoming[index];
        if (capacity < 0
            || quantity < 0
            || quantity > capacity
            || outgoing < 0
            || outgoing > quantity
            || incoming < 0
            || incoming > capacity - quantity) {
            throw std::invalid_argument("invalid inventory state");
        }
    }

    auto inventory = Inventory{};
    inventory.quantities_ = state.quantities;
    inventory.capacities_ = state.capacities;
    inventory.reserved_outgoing_ = state.reserved_outgoing;
    inventory.reserved_incoming_ = state.reserved_incoming;
    return inventory;
}

}
