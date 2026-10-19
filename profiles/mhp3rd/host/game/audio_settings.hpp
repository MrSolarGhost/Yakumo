#pragma once

#include "game/guest_ram.hpp"

#include <optional>

namespace mhp3rd::game::sound {

// The original Options menu uses eight levels, including silence.
inline constexpr int kMaxLevel = 7;
enum class Channel { Bgm, Sfx };
struct Levels {
    int bgm{};
    int sfx{};
    bool operator==(const Levels &) const = default;
};

// These are the game's own options, not settings.ini overrides. Unavailable
// until its option block and audio engine exist. Called between guest frames.
[[nodiscard]] std::optional<Levels> read(const Ram &ram);
// Updates both the option saved by the game and the live engine. Rejects
// invalid levels or memory without changing anything. BGM also updates the
// current music stream, as the original Options menu does.
[[nodiscard]] bool set(Ram &ram, Channel channel, int level);
// MHP3RD_TRACE_GAME_AUDIO: report changes seen at frame boundaries.
void trace(const Ram &ram);

} // namespace mhp3rd::game::sound
