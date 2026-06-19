#pragma once

#include "core/Resource.hpp"

namespace vibecity {

class Inventory {
public:
    Inventory();
    explicit Inventory(const ResourceArray& capacities);

    void set_capacity(ResourceId resource, Quantity capacity);

    [[nodiscard]] Quantity quantity(ResourceId resource) const;
    [[nodiscard]] Quantity capacity(ResourceId resource) const;
    [[nodiscard]] Quantity reserved_outgoing(ResourceId resource) const;
    [[nodiscard]] Quantity reserved_incoming(ResourceId resource) const;
    [[nodiscard]] Quantity available(ResourceId resource) const;
    [[nodiscard]] Quantity free_capacity(ResourceId resource) const;

    [[nodiscard]] bool can_add(ResourceId resource, Quantity quantity) const;
    [[nodiscard]] bool can_remove(ResourceId resource, Quantity quantity) const;
    [[nodiscard]] bool can_reserve_outgoing(ResourceId resource, Quantity quantity) const;
    [[nodiscard]] bool can_reserve_incoming(ResourceId resource, Quantity quantity) const;

    bool add(ResourceId resource, Quantity quantity);
    bool remove(ResourceId resource, Quantity quantity);

    bool reserve_outgoing(ResourceId resource, Quantity quantity);
    bool release_outgoing(ResourceId resource, Quantity quantity);
    bool consume_reserved_outgoing(ResourceId resource, Quantity quantity);

    bool reserve_incoming(ResourceId resource, Quantity quantity);
    bool release_incoming(ResourceId resource, Quantity quantity);
    bool commit_reserved_incoming(ResourceId resource, Quantity quantity);

    [[nodiscard]] const ResourceArray& quantities() const;
    [[nodiscard]] const ResourceArray& capacities() const;

private:
    ResourceArray quantities_{};
    ResourceArray capacities_{};
    ResourceArray reserved_outgoing_{};
    ResourceArray reserved_incoming_{};
};

}
