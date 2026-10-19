#include "game/audio_settings.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace mhp3rd::game::sound {
namespace {
// NPJB-40001: observed in option_task's level handler (0x0A05E7C8),
// the engine setters (0x088C6374/0x088C6388), and the current-music
// update (0x088C8634 -> 0x088DD4B8 -> 0x088EFF44). No guest code is patched.
constexpr std::uint32_t kOptionsPointer = 0x08AB3640u;
constexpr std::uint32_t kOptions = 0x5FBD4u;
constexpr std::uint32_t kEnginePointer = 0x09FBEB8Cu;
constexpr std::uint32_t kSfxLevel = 0x827D50u;
constexpr std::uint32_t kBgmLevel = 0x827D54u;
constexpr std::uint32_t kMusicPointer = 0x09FC87D0u;

bool range(const Ram &ram, std::uint32_t base, std::uint32_t offset, std::size_t bytes) {
    return base >= 0x08800000u && (base & 3u) == 0u &&
        static_cast<std::uint64_t>(base) + offset + bytes <= std::numeric_limits<std::uint32_t>::max() &&
        ram.contains(base + offset, bytes);
}

struct State {
    std::uint32_t options;
    std::uint32_t engine;
    Levels levels;
};

std::optional<State> state(const Ram &ram) {
    // Check the supported executable's two level stores and its 7.0 divisor
    // before interpreting profile-specific memory. Other executables fail closed.
    if (!ram.contains(0x088C637Cu, 4u) || !ram.contains(0x088C6390u, 4u) ||
        !ram.contains(0x0896CE50u, 4u) || ram.load32(0x088C637Cu) != 0xAC857D50u ||
        ram.load32(0x088C6390u) != 0xAC857D54u || ram.load32(0x0896CE50u) != 0x40E00000u ||
        !ram.contains(kOptionsPointer, 4u) || !ram.contains(kEnginePointer, 4u)) return std::nullopt;
    const auto block = ram.load32(kOptionsPointer);
    const auto engine = ram.load32(kEnginePointer);
    if (!range(ram, block, kOptions, 4u) || !range(ram, engine, kSfxLevel, 8u)) return std::nullopt;
    const auto options = block + kOptions;
    const Levels levels{ram.load8(options + 2u), ram.load8(options + 3u)};
    if (levels.bgm > kMaxLevel || levels.sfx > kMaxLevel ||
        ram.load32(engine + kBgmLevel) > kMaxLevel || ram.load32(engine + kSfxLevel) > kMaxLevel)
        return std::nullopt;
    return State{options, engine, levels};
}
} // namespace

std::optional<Levels> read(const Ram &ram) {
    const auto current = state(ram);
    return current ? std::optional<Levels>(current->levels) : std::nullopt;
}

bool set(Ram &ram, Channel channel, int level) {
    if (level < 0 || level > kMaxLevel || (channel != Channel::Bgm && channel != Channel::Sfx)) return false;
    const auto current = state(ram);
    if (!current) return false;
    std::uint32_t music{};
    std::uint32_t stream{};
    if (channel == Channel::Bgm) {
        if (!ram.contains(kMusicPointer, 4u)) return false;
        music = ram.load32(kMusicPointer);
        if (music != 0u) {
            if (!range(ram, music, 0u, 0x10u)) return false;
            stream = ram.load32(music);
            if (stream != 0u && !range(ram, stream, 0u, 0x74u)) return false;
        }
    }
    const auto value = static_cast<std::uint32_t>(level);
    ram.store8(current->options + (channel == Channel::Bgm ? 2u : 3u), static_cast<std::uint8_t>(value));
    ram.store32(current->engine + (channel == Channel::Bgm ? kBgmLevel : kSfxLevel), value);
    if (stream != 0u) {
        // The game's music gain is 0..127; its PCM gain is 0..0x8000.
        const auto gain = 127u * value / static_cast<std::uint32_t>(kMaxLevel);
        ram.store16(music + 0xCu, static_cast<std::uint16_t>(gain));
        ram.store32(stream + 0x6Cu, gain);
        ram.store32(stream + 0x70u, gain * 0x8000u / 127u);
    }
    return true;
}

void trace(const Ram &ram) {
    static const bool enabled = std::getenv("MHP3RD_TRACE_GAME_AUDIO") != nullptr;
    if (!enabled) return;
    static std::optional<Levels> previous;
    const auto levels = read(ram);
    if (levels == previous) return;
    previous = levels;
    if (levels)
        std::cout << "[game-audio] BGM=" << levels->bgm << "/7 SFX=" << levels->sfx << "/7" << std::endl;
    else
        std::cout << "[game-audio] game audio options unavailable" << std::endl;
}

} // namespace mhp3rd::game::sound
