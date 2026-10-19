#include "game/audio_settings.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {
using namespace mhp3rd::game;
int failures{};
void expect(bool ok, const char *what) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

constexpr std::uint32_t kBlock = 0x08C00000u;
constexpr std::uint32_t kBgmOption = kBlock + 0x5FBD6u;
constexpr std::uint32_t kSfxOption = kBlock + 0x5FBD7u;
constexpr std::uint32_t kEngine = 0x09000000u;
constexpr std::uint32_t kBgmEngine = kEngine + 0x827D54u;
constexpr std::uint32_t kSfxEngine = kEngine + 0x827D50u;
constexpr std::uint32_t kMusic = 0x09900000u;
constexpr std::uint32_t kStream = 0x09901000u;
struct Fixture {
    BufferRam ram{0x08800000u, 32u * 1024u * 1024u};
    Fixture() {
        ram.store32(0x088C637Cu, 0xAC857D50u);
        ram.store32(0x088C6390u, 0xAC857D54u);
        ram.store32(0x0896CE50u, 0x40E00000u);
        ram.store32(0x08AB3640u, kBlock);
        ram.store32(0x09FBEB8Cu, kEngine);
        ram.store32(0x09FC87D0u, kMusic);
        ram.store32(kMusic, kStream);
        ram.store8(kBgmOption, 7);
        ram.store8(kSfxOption, 7);
        ram.store32(kBgmEngine, 7);
        ram.store32(kSfxEngine, 7);
        ram.store16(kMusic + 0xCu, 127);
        ram.store16(kStream + 0xCu, 0x5555u);
        ram.store32(kStream + 0x6Cu, 127);
        ram.store32(kStream + 0x70u, 0x8000u);
    }
};

void levels_and_gain() {
    Fixture f;
    expect(sound::read(f.ram) == sound::Levels{7, 7}, "read original full game volume");
    // Expected gains from the game's eight-step scale, including truncation.
    constexpr std::array<unsigned, 8> gains{0, 18, 36, 54, 72, 90, 108, 127};
    constexpr std::array<unsigned, 8> pcm{0, 4644, 9288, 13932, 18577, 23221, 27865, 32768};
    for (int level = 0; level <= 7; ++level) {
        expect(sound::set(f.ram, sound::Channel::Bgm, level), "each BGM level is accepted");
        expect(f.ram.load8(kBgmOption) == level && f.ram.load32(kBgmEngine) == level,
            "BGM updates both game option and engine");
        expect(f.ram.load16(kMusic + 0xCu) == gains[level] && f.ram.load16(kStream + 0xCu) == 0x5555u &&
                f.ram.load32(kStream + 0x6Cu) == gains[level] && f.ram.load32(kStream + 0x70u) == pcm[level],
            "BGM changes current stream gain, including exact silence and full scale");
        expect(f.ram.load8(kSfxOption) == 7 && f.ram.load32(kSfxEngine) == 7, "BGM leaves SFX independent");
    }
    expect(sound::set(f.ram, sound::Channel::Bgm, 3), "set independent BGM fixture");
    for (int level = 7; level >= 0; --level) {
        expect(sound::set(f.ram, sound::Channel::Sfx, level), "each SFX level is accepted");
        expect(sound::read(f.ram) == sound::Levels{3, level} && f.ram.load32(kSfxEngine) == level,
            "SFX updates game option and engine without changing BGM");
        expect(f.ram.load32(kStream + 0x6Cu) == 54 && f.ram.load32(kStream + 0x70u) == 13932,
            "SFX leaves live music gain alone");
    }
    f.ram.store8(kBgmOption, 2);
    f.ram.store8(kSfxOption, 6);
    expect(sound::read(f.ram) == sound::Levels{2, 6}, "original game menu changes are immediately visible");
    for (int level : {-1, 8, 255}) {
        expect(!sound::set(f.ram, sound::Channel::Bgm, level) && !sound::set(f.ram, sound::Channel::Sfx, level),
            "invalid levels rejected rather than written or wrapped");
    }
    expect(!sound::set(f.ram, static_cast<sound::Channel>(99), 2), "invalid channel rejected");
    expect(sound::read(f.ram) == sound::Levels{2, 6}, "rejected edits preserve options");
    f.ram.store32(0x09FC87D0u, 0);
    expect(sound::set(f.ram, sound::Channel::Bgm, 4), "BGM changes before a music manager exists");
    f.ram.store32(0x09FC87D0u, kMusic);
    f.ram.store32(kMusic, 0);
    expect(sound::set(f.ram, sound::Channel::Bgm, 0), "BGM changes while no music stream is playing");
}

void invalid_memory() {
    BufferRam empty{0u, 16u};
    expect(!sound::read(empty) && !sound::set(empty, sound::Channel::Bgm, 1), "unattached game is unavailable");
    Fixture f;
    for (auto slot : {0x08AB3640u, 0x09FBEB8Cu}) {
        const auto good = f.ram.load32(slot);
        for (auto pointer : {0u, 4u, 0xFFFFFFFFu, 0xFFF00000u, 0x0C000000u, 0x09000001u}) {
            f.ram.store32(slot, pointer);
            expect(!sound::read(f.ram) && !sound::set(f.ram, sound::Channel::Sfx, 1),
                "null, unaligned, wrapped and unbacked option/engine pointers fail closed");
            expect(f.ram.load8(kSfxOption) == 7 && f.ram.load32(kSfxEngine) == 7, "bad pointer cannot partially edit");
        }
        f.ram.store32(slot, good);
    }
    for (auto slot : {0x088C637Cu, 0x088C6390u, 0x0896CE50u}) {
        const auto good = f.ram.load32(slot);
        f.ram.store32(slot, 0);
        expect(!sound::read(f.ram) && !sound::set(f.ram, sound::Channel::Bgm, 2), "unsupported executable rejected");
        f.ram.store32(slot, good);
    }
    for (auto slot : {kBgmOption, kSfxOption}) {
        f.ram.store8(slot, 8);
        expect(!sound::read(f.ram) && !sound::set(f.ram, sound::Channel::Bgm, 2), "invalid saved game level rejected");
        f.ram.store8(slot, 7);
    }
    for (auto slot : {kBgmEngine, kSfxEngine}) {
        f.ram.store32(slot, 0xFFFFFFFFu);
        expect(!sound::read(f.ram) && !sound::set(f.ram, sound::Channel::Sfx, 2), "invalid live engine level rejected");
        f.ram.store32(slot, 7);
    }
    for (auto slot : {0x09FC87D0u, kMusic}) {
        const auto good = f.ram.load32(slot);
        for (auto pointer : {4u, 0xFFFFFFFFu, 0x0C000000u, 0x09901001u}) {
            f.ram.store32(slot, pointer);
            expect(!sound::set(f.ram, sound::Channel::Bgm, 0), "invalid live music pointer rejects BGM change");
            expect(
                f.ram.load8(kBgmOption) == 7 && f.ram.load32(kBgmEngine) == 7 && f.ram.load32(kStream + 0x70u) == 32768,
                "failed music update leaves options, engine and current stream intact");
        }
        f.ram.store32(slot, good);
    }
}

void trace_changes() {
#if defined(_WIN32)
    _putenv_s("MHP3RD_TRACE_GAME_AUDIO", "1");
#else
    setenv("MHP3RD_TRACE_GAME_AUDIO", "1", 1);
#endif
    Fixture f;
    std::ostringstream output;
    auto *previous = std::cout.rdbuf(output.rdbuf());
    sound::trace(f.ram);
    sound::trace(f.ram);
    f.ram.store8(kBgmOption, 0);
    sound::trace(f.ram);
    f.ram.store32(0x09FBEB8Cu, 0);
    sound::trace(f.ram);
    sound::trace(f.ram);
    std::cout.rdbuf(previous);
    expect(output.str() ==
            "[game-audio] BGM=7/7 SFX=7/7\n[game-audio] BGM=0/7 SFX=7/7\n"
            "[game-audio] game audio options unavailable\n",
        "diagnostic logs transitions once and includes no game or player content");
}
} // namespace

int main() {
    levels_and_gain();
    invalid_memory();
    trace_changes();
    std::cout << (failures ? "FAIL" : "PASS") << ": game audio settings (" << failures << " failures)\n";
    return failures ? 1 : 0;
}
