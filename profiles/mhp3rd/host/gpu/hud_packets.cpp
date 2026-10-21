#include "gpu/hud_packets.hpp"

#include <limits>

namespace mhp3rd::gpu::hud {

Part quest_part(std::uint32_t return_address) {
    switch (return_address) {
    case 0x0A17399Cu: // weapon gauges
    case 0x0A173A68u: // clock
    case 0x0A173A74u: // health/stamina
    case 0x0A173B88u: // party plates and their queued names
        return Part::Status;
    case 0x0A173BF8u:
        return Part::Map;
    case 0x0A173C04u:
        return Part::Items;
    default:
        return Part::Other;
    }
}
void PacketTags::file(std::uint32_t packet, std::uint32_t words, Part part) {
    if (packet == 0u || words == 0u || words > (std::numeric_limits<std::uint32_t>::max() - packet) / 4u) return;
    const std::uint32_t end = packet + words * 4u;
    auto it = packets_.lower_bound(packet);
    if (it != packets_.begin() && std::prev(it)->second.end > packet) --it;
    while (it != packets_.end() && it->first < end) it = packets_.erase(it);
    if (part != Part::None) packets_.emplace(packet, Packet{end, part});
    if (packets_.size() > 65536u) packets_.clear();
}
Part PacketTags::at(std::uint32_t command, std::uint32_t call_return) const {
    const auto find = [&](std::uint32_t address) {
        const auto it = packets_.upper_bound(address);
        if (it == packets_.begin()) return Part::None;
        const Packet &p = std::prev(it)->second;
        return address < p.end ? p.part : Part::None;
    };
    const Part part = find(command);
    return part != Part::None ? part : find(call_return);
}
void QueueTags::update(std::uint32_t begin, std::uint32_t stride, std::uint32_t capacity, std::uint8_t &known,
    std::uint8_t count, Part part) {
    if (count < known) {
        const auto first = entries_.lower_bound(begin);
        const auto end = entries_.lower_bound(begin + stride * capacity);
        entries_.erase(first, end);
        known = 0u;
    }
    for (std::uint32_t i = known; i < count; ++i) {
        const std::uint32_t entry = begin + i * stride;
        if (part == Part::None)
            entries_.erase(entry);
        else
            entries_.insert_or_assign(entry, part);
    }
    known = count;
}
Part QueueTags::at(std::uint32_t entry) const {
    const auto it = entries_.find(entry);
    return it == entries_.end() ? Part::None : it->second;
}
void QueueTags::clear() {
    entries_.clear();
}

} // namespace mhp3rd::gpu::hud