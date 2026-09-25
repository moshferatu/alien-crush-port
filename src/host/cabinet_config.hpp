// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cabinet_input.hpp"
#include "cabinet_nudge.hpp"
#include "pad_bindings.hpp"
#include "alien_crush/input/plunger.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// portable-data/cabinet.ini, in Devil's Crush's format (its
// src/cabinet/display_config.cpp): `key=value` lines, a `version`, values
// percent-escaped, a repeated key or an unreadable value rejects the file, and
// unknown keys are ignored and dropped the next time it is written. The keys
// are Devil's Crush's wherever the two games share a setting, so one file's
// window, key, nudge and plunger lines read the same in either; the score
// window is `dmd`, as the display that shows the score is there. What only
// Alien Crush has -- a rotation for every window, and a cabinet board's raw
// buttons -- is added in the same style.
//
// The game writes the whole file on a cabinet session's exit, and the launcher
// (src/launcher/launcher.cpp) whenever it saves. Files from before this
// format -- the older cabinet.ini with its `window.*` and `input.buttons`
// lines, and the separate cabinet-windows.txt, cabinet-inputs.txt and
// cabinet-plunger.txt -- are still read, and the first write replaces them.
namespace alien_crush::host {
inline constexpr std::uint32_t kCabinetConfigVersion=1;

struct DisplayRect {
    int x=0,y=0,width=0,height=0;
    bool valid() const {return width>0 && height>0;}
    bool operator==(const DisplayRect&) const=default;
};
// The monitor's own orientation when the window was saved, as SDL numbers it.
enum class DisplayRotation : std::uint8_t {unknown=0,landscape=1,landscape_flipped=2,portrait=3,portrait_flipped=4};
DisplayRotation display_rotation(SDL_DisplayOrientation);

// One window. A geometry of no size is a window never placed, which opens
// where the game first puts it.
struct WindowPlacement {
    std::string display_identity;
    DisplayRect geometry;
    DisplayRotation rotation=DisplayRotation::unknown;
    bool fullscreen=false;
    // How far the picture is turned clockwise (R), 0/90/180/270; separate
    // from the monitor's orientation above.
    int artwork_rotation=0;
    bool operator==(const WindowPlacement&) const=default;
};
// The cabinet's windows in the game's order, and the prefix each has in the file.
enum CabinetWindow : std::size_t {playfield_window=0,backglass_window=1,score_window=2};
inline constexpr std::size_t kCabinetWindowCount=3;
inline constexpr std::array<const char*,kCabinetWindowCount> kCabinetWindowFields{"playfield","backglass","dmd"};

// Keys are SDL scancode names ("Left Shift", "Return", "X"), so the file stays
// hand-editable. An input bound to no keys is unbound; one not in the file
// takes the default.
struct InputBindings {
    std::array<std::vector<std::string>,kPadButtonCount> keys;
    FlipperInputs flippers;
    // Keys that send button I, and only I, while the ball is seated in the
    // shooter, so that a key bound to Run launches there instead of pausing.
    std::vector<std::string> launch_keys;
    input::PlungerBinding plunger;
    NudgeBinding nudge;
    // A cabinet board's own buttons (Alien Crush only).
    CabinetButtons buttons;
    const std::vector<std::string>& operator[](PadButton b) const {return keys[static_cast<std::size_t>(b)];}
    std::vector<std::string>& operator[](PadButton b) {return keys[static_cast<std::size_t>(b)];}
    bool operator==(const InputBindings&) const=default;
};
// Left Shift and Left work the left flipper; Right Shift and Right the right;
// Return confirms, starts and pauses, and launches from the shooter; X nudges;
// Backspace is Select; Up and Down move through menus.
InputBindings default_input_bindings();
// The board's buttons under `input.buttons.`, in CabinetButtons' order.
inline constexpr std::array<const char*,9> kCabinetButtonFields{
    "left_flipper","right_flipper","nudge","run","select","up","down","right","launch"};

// The settings a player chooses once rather than per run. Each was a flag
// first, and the flag still beats the file. They apply to normal play only;
// replays and --frames runs keep the flags' defaults.
struct PlaySettings {
    // game.rom, UTF-8. Empty: Alien Crush.pce beside the executable. A
    // relative path is taken from the executable's folder.
    std::string rom;
    // game.cabinet: three windows (--cabinet), or one (--no-cabinet).
    bool cabinet=false;
    // audio.music_voice_overlay (--music-voice-overlay / --no-music-voice-overlay).
    bool music_voice_overlay=true;
    // video.smoothing (--smooth / --no-smooth); F7 changes it, and a cabinet
    // session remembers.
    bool smoothing=true;
    // input.nudge_multiplier (--nudge-multiplier), 0 to 16.
    double nudge_multiplier=1;
    bool operator==(const PlaySettings&) const=default;
};

struct CabinetConfig {
    std::uint32_t format_version=kCabinetConfigVersion;
    std::array<WindowPlacement,kCabinetWindowCount> windows;
    InputBindings input=default_input_bindings();
    PlaySettings play;
    bool operator==(const CabinetConfig&) const=default;
};

// Empty when a value cannot be written (a rotation off the quarter turns, a
// multiplier past 16).
std::vector<std::byte> encode_cabinet_config(const CabinetConfig&);
// Throws std::runtime_error naming what is wrong.
CabinetConfig decode_cabinet_config(std::span<const std::byte>);

enum class ConfigLoadStatus {loaded,not_found,invalid,io_error};
struct ConfigLoadResult {
    ConfigLoadStatus status=ConfigLoadStatus::io_error;
    CabinetConfig config;
    std::string message;
    // Read from the older format; the next write replaces it.
    bool legacy=false;
    bool loaded() const {return status==ConfigLoadStatus::loaded;}
};
// `data`/cabinet.ini, or the older files beside it. Not found gives the defaults.
ConfigLoadResult load_cabinet_config(const std::filesystem::path& data);
// Throws on failure. Written through a temporary file, so a reader never sees half.
void save_cabinet_config(const std::filesystem::path& path,const CabinetConfig&);
}
