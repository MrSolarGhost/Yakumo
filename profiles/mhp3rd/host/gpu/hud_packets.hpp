#pragma once

#include "gpu/hud_layout.hpp"

#include <cstdint>
#include <map>

namespace mhp3rd::gpu::hud {

// Semantic origin inside the checked NPJB-40001 quest HUD function, paired
// with rendered packets in a quest trace.
[[nodiscard]] Part quest_part(std::uint32_t return_address);

class PacketTags {
public:
    // Overwriting even a non-HUD packet invalidates every overlapping old tag.
    void file(std::uint32_t packet, std::uint32_t words, Part part);
    [[nodiscard]] Part at(std::uint32_t command, std::uint32_t call_return = 0u) const;

private:
    struct Packet {
        std::uint32_t end;
        Part part;
    };
    std::map<std::uint32_t, Packet> packets_;
};

// Glyphs/icons are emitted after their original caller returns. Ownership
// lasts until a queue is emptied and its entries are reused.
class QueueTags {
public:
    void update(std::uint32_t begin, std::uint32_t stride, std::uint32_t capacity, std::uint8_t &known,
        std::uint8_t count, Part part);
    [[nodiscard]] Part at(std::uint32_t entry) const;
    void clear();

private:
    std::map<std::uint32_t, Part> entries_;
};

} // namespace mhp3rd::gpu::hud