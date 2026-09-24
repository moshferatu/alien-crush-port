// SPDX-License-Identifier: GPL-3.0-or-later

// alien-crush-launcher: `portable-data/cabinet.ini` with a face on it, and a
// Play button.
//
// Everything here is a setting the file already has -- the play settings
// (`src/host/play_settings.hpp`), the cabinet's windows (`src/host/cabinet.cpp`),
// its buttons, nudge board and plunger (`src/host/cabinet_input.cpp`,
// `cabinet_nudge.cpp`, `cabinet_plunger.cpp`) -- and nothing else: the
// launcher is for the player who would rather not open a text file to turn
// smoothing off or map a flipper button. It reads the file the game would
// read, shows it a tab at a time, writes it back where it was, and starts the
// game from the folder the file is in, so the two cannot find different ones.
//
// ## The file
//
// `portable-data/cabinet.ini` beside the launcher, which is where the game
// looks when it is started from beside it. Read with the game's own parsers,
// so what is shown is what the game would play, and written with the game's
// own `write_cabinet_settings`; fields the launcher does not know go back as
// they came, and so does where each window is. Settings the game still takes
// from its older separate files (`cabinet-inputs.txt`, `cabinet-plunger.txt`,
// `cabinet-windows.txt`) are shown from there, as the game would read them,
// and a save moves them into cabinet.ini, as the game's own exit does. A file
// the game cannot read is shown as the defaults and a save replaces it.
// Nothing is written until Save or Play.
//
// Play saves and starts the game, and the launcher waits out of sight until
// the game closes, then comes back and reads the file again: a cabinet
// session writes it on the way out, with where its windows were and what F7
// and R were left at.
//
// ## The screen
//
// SDL, as the game is, drawn by hand on black with the system's font through
// stb_truetype, at the display's scale. It is worked by the mouse, the
// keyboard or a controller alike:
//
//   * Up and Down move between rows, Left and Right change a value, and past
//     the last row are the buttons. Tab, or L1 and R1, turn the tabs.
//   * A setting with a fixed list of values is a dropdown: Enter, South or a
//     click opens it, Up and Down choose, Enter, South or a click takes the
//     choice, and Escape, East or a click elsewhere closes it unchanged.
//   * A cabinet button is set by pressing it. Enter (or South), or Detect,
//     waits for the next button pressed on a joystick and takes its number,
//     and the board's name too if none is set yet; a key, a click or five
//     seconds stop waiting. Backspace (or West) turns the row off. Its number
//     can also be chosen from its list.
//   * A path, a name or a number is typed after Enter or a click. The ROM can
//     also be chosen with Browse, or dropped on the window.
//   * Ctrl+S saves; Ctrl+Enter, F5 or Start plays; Escape quits at once,
//     unsaved changes or not.
//
// For checking it without a window: `--press Tab,Down,Right,Ctrl+S` hands it
// keys, by their SDL names, as if they had been pressed, and
// `--screenshot <file.png>` then draws it once into the file and leaves, the
// window never shown.
//
// ## The icon and the heading
//
// Both are the title screen's logo, decoded from the player's ROM by
// `tools/make_launcher_art.cpp` when this is built. Without a ROM at build
// time the launcher builds without an icon, and its heading is text.

#include "cabinet_input.hpp"
#include "cabinet_nudge.hpp"
#include "cabinet_plunger.hpp"
#include "cabinet_settings.hpp"
#include "play_settings.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/rom_image.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef AC_LAUNCHER_LOGO
#include <cstdint>
#include "ac_launcher_logo.h"
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include "stb_truetype.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using alien_crush::host::CabinetSettings;

constexpr const char* kTitle = "Alien Crush";
constexpr const wchar_t* kGameExe = L"alien-crush.exe";
constexpr const char* kRomDefault = "Alien Crush.pce";
const std::filesystem::path kDataRelative = "portable-data";
const std::filesystem::path kConfigRelative = kDataRelative / "cabinet.ini";

// The cabinet's windows, in the order the file and the game have them, and
// the size the game opens each at before it has been placed.
constexpr int kWindows = 3;
constexpr std::array<const char*, kWindows> kRoles = {"playfield", "backglass", "score"};
constexpr std::array<const char*, kWindows> kWindowNames = {"Playfield", "Backglass", "Score"};
constexpr std::array<int, kWindows> kDefaultW = {540, 640, 640};
constexpr std::array<int, kWindows> kDefaultH = {720, 360, 200};

// One window's line in the file: the monitor by name, where the window is
// and how big, and whether it is fullscreen and how far it is turned.
struct Placement {
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;
    bool fullscreen = false;
    int rotation = 0;
};

// What the launcher shows: every field the game reads from the file, as the
// game's own parsers read them.
struct Config {
    alien_crush::host::PlaySettings play;
    alien_crush::host::NudgeBinding nudge;
    alien_crush::input::PlungerBinding plunger;
    alien_crush::host::CabinetButtons buttons;
    // Whether the windows have been placed. Until they are, the game opens
    // them on the first monitors in turn, and the file says nothing of them.
    bool placed = false;
    std::array<Placement, kWindows> windows{};
};

// --- text and paths --------------------------------------------------------

std::filesystem::path path_of_utf8(const std::string& text)
{
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::string utf8_of(const std::filesystem::path& path)
{
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

std::string utf8_of(const wchar_t* wide)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) {
        return {};
    }
    std::string out(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), n, nullptr, nullptr);
    return out;
}

// The next code point of UTF-8 `s` from `i`, which is moved past it. A byte
// that does not start one is taken as Latin-1, so nothing is lost for being
// malformed.
unsigned next_code_point(std::string_view s, std::size_t& i)
{
    const auto b = static_cast<unsigned char>(s[i]);
    const int extra = b >= 0xF0 ? 3 : b >= 0xE0 ? 2 : b >= 0xC0 ? 1 : 0;
    if (extra == 0) {
        ++i;
        return b;
    }
    unsigned cp = b & (0x3Fu >> static_cast<unsigned>(extra));
    std::size_t j = i + 1;
    for (int k = 0; k < extra; ++k, ++j) {
        if (j >= s.size() || (static_cast<unsigned char>(s[j]) & 0xC0u) != 0x80u) {
            ++i;
            return b;
        }
        cp = cp << 6u | (static_cast<unsigned char>(s[j]) & 0x3Fu);
    }
    i = j;
    return cp;
}

std::size_t previous_boundary(std::string_view s, std::size_t i)
{
    if (i == 0) {
        return 0;
    }
    --i;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0u) == 0x80u) {
        --i;
    }
    return i;
}

std::size_t next_boundary(std::string_view s, std::size_t i)
{
    if (i >= s.size()) {
        return s.size();
    }
    next_code_point(s, i);
    return i;
}

std::string trimmed(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

bool same_ignoring_case(std::string_view a, std::string_view b)
{
    return a.size() == b.size()
        && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

// --- what there is to set ----------------------------------------------------

enum Kind { K_HEAD, K_CHOICE, K_RANGE, K_PATH, K_TEXT, K_NUMBER, K_BUTTON };

enum Setting {
    S_ROM, S_CABINET,
    S_SMOOTHING,
    S_MONITOR, S_ROTATION = S_MONITOR + kWindows, S_FULLSCREEN = S_ROTATION + kWindows,
    S_NUDGE_STRENGTH = S_FULLSCREEN + kWindows, S_NUDGE_ENABLED, S_NUDGE_DEVICE, S_NUDGE_X, S_NUDGE_Y,
    S_NUDGE_THRESHOLD, S_NUDGE_RELEASE, S_NUDGE_COOLDOWN,
    S_PLUNGER_DEVICE, S_PLUNGER_AXIS, S_PLUNGER_REST, S_PLUNGER_DRAWN, S_PLUNGER_DEADZONE,
    S_PLUNGER_RELEASE,
    S_BUTTONS_DEVICE,
    S_BUTTON,  // one for each of the board's nine inputs
};

struct Choice {
    int value;
    const char* label;
};

struct Row {
    Kind kind;
    int id;  // a Setting
    const char* label;
    const char* help;
    const Choice* choices = nullptr;
    int count = 0;
    int lo = 0, hi = 0, step = 1;  // K_RANGE and K_NUMBER
    bool slider = false;
};

constexpr Choice on_off[] = {{0, "Off"}, {1, "On"}};
constexpr Choice rotation_choices[] = {{0, "0\xC2\xB0"}, {90, "90\xC2\xB0"}, {180, "180\xC2\xB0"}, {270, "270\xC2\xB0"}};
constexpr Choice cabinet_choices[] = {{1, "Cabinet (three windows)"}, {0, "Desktop (one window)"}};

template <typename T, std::size_t N>
constexpr int count_of(const T (&)[N]) { return static_cast<int>(N); }

enum { TAB_GAME, TAB_VIDEO, TAB_CABINET, TAB_BUTTONS, TAB_COUNT };
constexpr const char* tab_names[TAB_COUNT] = {"Game", "Video", "Cabinet", "Buttons"};
constexpr int kRowsMax = 64;
std::array<std::vector<Row>, TAB_COUNT> rows;

void add(int tab, Row r) { rows[static_cast<std::size_t>(tab)].push_back(r); }
void add_choice(int tab, int id, const char* label, const Choice* c, int n, const char* help)
{
    add(tab, Row{K_CHOICE, id, label, help, c, n});
}
void add_range(int tab, int id, const char* label, int lo, int hi, int step, bool slider, const char* help)
{
    add(tab, Row{K_RANGE, id, label, help, nullptr, 0, lo, hi, step, slider});
}
void add_head(int tab, const char* label, const char* help) { add(tab, Row{K_HEAD, 0, label, help}); }

// The board's inputs in the order `input.buttons` has them, with what each
// does on the table.
constexpr std::size_t kButtons = std::tuple_size_v<decltype(alien_crush::host::CabinetButtons::buttons)>;
constexpr std::array<const char*, kButtons> button_rows = {
    "Left Flipper", "Right Flipper", "Nudge", "Run (Start, Launch, Pause)", "Select",
    "Up", "Down", "Right (Menu)", "I (Launch, Confirm)",
};

void build_rows()
{
    add(TAB_GAME, Row{K_PATH, S_ROM, "ROM File",
        "The HuCard image, a .pce file: the one dump the port supports (see README.md). A path "
        "that is not absolute is taken from the launcher's folder. Browse, type it after Enter, or "
        "drop the file on this window."});
    add_choice(TAB_GAME, S_CABINET, "Mode", cabinet_choices, count_of(cabinet_choices),
        "Cabinet: the playfield, the backglass and the score display, each in a window of its own, "
        "for a virtual pinball cabinet. Desktop: the game in one window.");

    add_choice(TAB_VIDEO, S_SMOOTHING, "Smoothing", on_off, 2,
        "On: on a playfield display faster than 66 Hz, a picture for every refresh, the table's "
        "sprites, poses and palettes eased between the game's sixty a second. Off: the game's "
        "frames only. Cabinet mode only. F7 changes it while playing, and a cabinet remembers.");
    static const char monitor_help[] =
        "The monitor the window opens on. F10 in the game moves the focused window too, and the "
        "game remembers where it was left.";
    static const char rotation_help[] =
        "How far the window's picture is turned clockwise, for a monitor that is mounted on its "
        "side. R and Shift+R turn the focused window while playing, and the game remembers.";
    static const char fullscreen_help[] =
        "Whether the window fills its monitor. F11 in the game toggles the focused window.";
    add_head(TAB_VIDEO, "Cabinet Windows", "For the cabinet's windows; a desktop window ignores them.");
    static std::array<std::array<std::string, 3>, kWindows> labels;
    for (int i = 0; i < kWindows; ++i) {
        auto& l = labels[static_cast<std::size_t>(i)];
        const std::string name = kWindowNames[static_cast<std::size_t>(i)];
        l = {name + " Monitor", name + " Rotation", name + " Fullscreen"};
        add_choice(TAB_VIDEO, S_MONITOR + i, l[0].c_str(), nullptr, 0, monitor_help);
        add_choice(TAB_VIDEO, S_ROTATION + i, l[1].c_str(), rotation_choices, count_of(rotation_choices), rotation_help);
        add_choice(TAB_VIDEO, S_FULLSCREEN + i, l[2].c_str(), on_off, 2, fullscreen_help);
    }

    static const char nudge_help[] =
        "A nudge board, such as a PinOne's accelerometer, read as two axes of a controller in "
        "cabinet mode: a shove past the threshold nudges the table, once, until it settles below "
        "the release.";
    add_head(TAB_CABINET, "Nudge", nudge_help);
    add_range(TAB_CABINET, S_NUDGE_STRENGTH, "Nudge Strength", 0, 160, 5, true,
        "How hard every nudge is, keyboard, controller and board alike, as a multiple of the "
        "game's own.");
    add_choice(TAB_CABINET, S_NUDGE_ENABLED, "Nudge Board", on_off, 2, nudge_help);
    add(TAB_CABINET, Row{K_TEXT, S_NUDGE_DEVICE, "Nudge Device",
        "Part of the controller's name, any case. The first controller whose name has it in is "
        "the board."});
    add_range(TAB_CABINET, S_NUDGE_X, "Nudge X Axis", 0, 255, 1, false, "The axis that reads side to side.");
    add_range(TAB_CABINET, S_NUDGE_Y, "Nudge Y Axis", 0, 255, 1, false,
        "The axis that reads front to back. Not the X axis.");
    add_range(TAB_CABINET, S_NUDGE_THRESHOLD, "Nudge Threshold", 1, 100, 1, true,
        "How far a shove has to reach to nudge, as a percentage of the axis.");
    add_range(TAB_CABINET, S_NUDGE_RELEASE, "Nudge Release", 0, 99, 1, true,
        "How still the board has to settle before it can nudge again, as a percentage of the "
        "axis. Below the threshold.");
    add_range(TAB_CABINET, S_NUDGE_COOLDOWN, "Nudge Cooldown", 100, 5000, 50, true,
        "The least time from one nudge to the next.");
    static const char plunger_help[] =
        "An analog plunger, read as one axis of a controller in cabinet mode. Run alien-crush.exe "
        "--cabinet-plunger-probe and pull the rod a few times: it prints the axis and the two "
        "values to put here.";
    add_head(TAB_CABINET, "Plunger", plunger_help);
    add(TAB_CABINET, Row{K_TEXT, S_PLUNGER_DEVICE, "Plunger Device",
        "Part of the controller's name, any case. Empty: the first controller that has the axis."});
    add_range(TAB_CABINET, S_PLUNGER_AXIS, "Plunger Axis", -1, 255, 1, false,
        "Off until one is named, because a plunger and a thumbstick look the same from here. A "
        "PinOne's is axis 2.");
    add(TAB_CABINET, Row{K_NUMBER, S_PLUNGER_REST, "Plunger Rest",
        "The axis value with the rod at rest, as the probe prints it.", nullptr, 0, -2147483647 - 1,
        2147483647});
    add(TAB_CABINET, Row{K_NUMBER, S_PLUNGER_DRAWN, "Plunger Drawn",
        "The axis value with the rod drawn fully back. The wrong way round with Rest, and every "
        "shot dribbles.", nullptr, 0, -2147483647 - 1, 2147483647});
    add_range(TAB_CABINET, S_PLUNGER_DEADZONE, "Plunger Deadzone", 0, 100, 1, true,
        "How much of the travel nearest rest still reads as rest, as a percentage.");
    add_range(TAB_CABINET, S_PLUNGER_RELEASE, "Plunger Release", 0, 100, 1, true,
        "How sudden a fall forward counts as letting go, as a percentage of the travel.");

    static const char button_help[] =
        "A cabinet board's own buttons, such as a PinOne's, read in cabinet mode. Enter or Detect "
        "waits for a button to be pressed on the board; Backspace turns the row off. The keyboard "
        "and other controllers still work alongside.";
    add_head(TAB_BUTTONS, "Board", button_help);
    add(TAB_BUTTONS, Row{K_TEXT, S_BUTTONS_DEVICE, "Buttons Device",
        "Part of the board's name, any case. The first controller whose name has it in, and that "
        "has every button below, is the board. Detect fills it in when it is empty."});
    add_head(TAB_BUTTONS, "Buttons", button_help);
    for (std::size_t i = 0; i < kButtons; ++i) {
        add(TAB_BUTTONS, Row{K_BUTTON, S_BUTTON + static_cast<int>(i), button_rows[i], button_help, nullptr, 0,
            -1, 255});
    }
}

bool is_monitor(int id) { return id >= S_MONITOR && id < S_MONITOR + kWindows; }
bool is_window(int id) { return id >= S_MONITOR && id < S_FULLSCREEN + kWindows; }
int window_of(int id) { return (id - S_MONITOR) % kWindows; }

// --- monitors ----------------------------------------------------------------
//
// Found and matched as the game finds and matches them (`src/host/cabinet.cpp`):
// every display with usable bounds, in SDL's order, known by its name; two
// with the same name are told apart by where the window is, which is why
// choosing one also moves the window onto it.

struct Monitor {
    std::string name;
    SDL_Rect usable{};  // where the game puts windows
    SDL_Rect bounds{};  // the whole display, for its label
    bool primary = false;
};

struct Monitors {
    std::vector<Monitor> list;
    std::vector<std::string> labels;
};
Monitors monitors;

// A window on a monitor the file names that is not connected.
constexpr int kMonitorAbsent = 1000;

void find_monitors()
{
    monitors = {};
    int count = 0;
    SDL_DisplayID* const ids = SDL_GetDisplays(&count);
    const auto primary = SDL_GetPrimaryDisplay();
    for (int i = 0; ids != nullptr && i < count; ++i) {
        Monitor m;
        if (!SDL_GetDisplayUsableBounds(ids[i], &m.usable) || m.usable.w <= 0 || m.usable.h <= 0) continue;
        if (!SDL_GetDisplayBounds(ids[i], &m.bounds)) m.bounds = m.usable;
        const char* const name = SDL_GetDisplayName(ids[i]);
        m.name = name != nullptr ? name : "Unnamed display";
        m.primary = ids[i] == primary;
        monitors.list.push_back(std::move(m));
    }
    SDL_free(ids);
    for (const auto& m : monitors.list) {
        char part[96];
        std::snprintf(part, sizeof part, ", %d \xC3\x97 %d", m.bounds.w, m.bounds.h);
        std::string label = (m.name.empty() ? std::string("Unnamed display") : m.name) + part;
        const auto same = std::ranges::count_if(monitors.list, [&](const auto& o) { return o.name == m.name; });
        if (same > 1) {
            std::snprintf(part, sizeof part, " at %d, %d", m.bounds.x, m.bounds.y);
            label += part;
        }
        if (m.primary) label += " (main)";
        monitors.labels.push_back(std::move(label));
    }
}

// Where the game opens a window the file has not placed: on the monitor of
// its turn, or the last there is, a little down and across from the corner.
Placement default_placement(int i)
{
    Placement p;
    p.w = kDefaultW[static_cast<std::size_t>(i)];
    p.h = kDefaultH[static_cast<std::size_t>(i)];
    if (monitors.list.empty()) return p;
    const auto& m = monitors.list[std::min(static_cast<std::size_t>(i), monitors.list.size() - 1)];
    const auto& b = m.usable;
    p.name = m.name;
    p.w = std::min(p.w, b.w);
    p.h = std::min(p.h, b.h);
    p.x = std::clamp(b.x + i * 40, b.x, b.x + b.w - p.w);
    p.y = std::clamp(b.y + i * 40, b.y, b.y + b.h - p.h);
    return p;
}

// The windows as the file will have them, placed as the game would open them
// if they were not already.
void place(Config& c)
{
    if (c.placed) return;
    for (int i = 0; i < kWindows; ++i) c.windows[static_cast<std::size_t>(i)] = default_placement(i);
    c.placed = true;
}

// The monitor the game would open the window on, as `Cabinet` picks it; or
// absent, for a monitor the file names that is not connected, which the game
// would stand another in for.
int monitor_get(const Config& c, int i)
{
    if (monitors.list.empty()) return 0;
    const int fallback = std::min(i, static_cast<int>(monitors.list.size()) - 1);
    if (!c.placed) return fallback;
    const auto& p = c.windows[static_cast<std::size_t>(i)];
    int selected = -1;
    for (std::size_t j = 0; j < monitors.list.size(); ++j) {
        if (monitors.list[j].name != p.name) continue;
        selected = static_cast<int>(j);
        const auto& b = monitors.list[j].usable;
        if (p.x >= b.x && p.x < b.x + b.w && p.y >= b.y && p.y < b.y + b.h) break;
    }
    return selected < 0 ? kMonitorAbsent : selected;
}

// Onto the monitor, in the middle, as F10 in the game puts it: at the size the
// window was, as much of it as fits.
void monitor_set(Config& c, int i, int v)
{
    if (v < 0 || v >= static_cast<int>(monitors.list.size()) || v == monitor_get(c, i)) return;
    place(c);
    const auto& m = monitors.list[static_cast<std::size_t>(v)];
    const auto& b = m.usable;
    auto& p = c.windows[static_cast<std::size_t>(i)];
    p.w = std::min(p.w > 0 ? p.w : kDefaultW[static_cast<std::size_t>(i)], b.w);
    p.h = std::min(p.h > 0 ? p.h : kDefaultH[static_cast<std::size_t>(i)], b.h);
    p.name = m.name;
    p.x = b.x + (b.w - p.w) / 2;
    p.y = b.y + (b.h - p.h) / 2;
}

int setting_get(Config& c, int id)
{
    auto& n = c.nudge;
    auto& p = c.plunger;
    if (is_monitor(id)) return monitor_get(c, window_of(id));
    if (id >= S_ROTATION && id < S_ROTATION + kWindows)
        return c.placed ? c.windows[static_cast<std::size_t>(window_of(id))].rotation : 0;
    if (id >= S_FULLSCREEN && id < S_FULLSCREEN + kWindows)
        return c.placed && c.windows[static_cast<std::size_t>(window_of(id))].fullscreen;
    if (id >= S_BUTTON && id < S_BUTTON + static_cast<int>(kButtons))
        return c.buttons.buttons[static_cast<std::size_t>(id - S_BUTTON)];
    switch (id) {
    case S_CABINET: return c.play.cabinet;
    case S_SMOOTHING: return c.play.smoothing;
    case S_NUDGE_STRENGTH: return static_cast<int>(std::lround(c.play.nudge_multiplier * 10.0));
    case S_NUDGE_ENABLED: return n.enabled;
    case S_NUDGE_X: return n.axis_x;
    case S_NUDGE_Y: return n.axis_y;
    case S_NUDGE_THRESHOLD: return n.threshold;
    case S_NUDGE_RELEASE: return n.release;
    case S_NUDGE_COOLDOWN: return n.cooldown_ms;
    case S_PLUNGER_AXIS: return p.axis < 0 ? -1 : p.axis;
    case S_PLUNGER_REST: return p.calibration.rest;
    case S_PLUNGER_DRAWN: return p.calibration.drawn;
    case S_PLUNGER_DEADZONE: return p.calibration.deadzone;
    case S_PLUNGER_RELEASE: return p.calibration.release;
    default: return 0;
    }
}

void setting_set(Config& c, int id, int v)
{
    auto& n = c.nudge;
    auto& p = c.plunger;
    if (is_monitor(id)) { monitor_set(c, window_of(id), v); return; }
    if (id >= S_ROTATION && id < S_ROTATION + kWindows) {
        place(c);
        c.windows[static_cast<std::size_t>(window_of(id))].rotation = v;
        return;
    }
    if (id >= S_FULLSCREEN && id < S_FULLSCREEN + kWindows) {
        place(c);
        c.windows[static_cast<std::size_t>(window_of(id))].fullscreen = v != 0;
        return;
    }
    if (id >= S_BUTTON && id < S_BUTTON + static_cast<int>(kButtons)) {
        c.buttons.buttons[static_cast<std::size_t>(id - S_BUTTON)] = v;
        return;
    }
    switch (id) {
    case S_CABINET: c.play.cabinet = v != 0; break;
    case S_SMOOTHING: c.play.smoothing = v != 0; break;
    case S_NUDGE_STRENGTH: c.play.nudge_multiplier = v / 10.0; break;
    case S_NUDGE_ENABLED: n.enabled = v != 0; break;
    case S_NUDGE_X: n.axis_x = v; break;
    case S_NUDGE_Y: n.axis_y = v; break;
    case S_NUDGE_THRESHOLD: n.threshold = v; break;
    case S_NUDGE_RELEASE: n.release = v; break;
    case S_NUDGE_COOLDOWN: n.cooldown_ms = v; break;
    case S_PLUNGER_AXIS: p.axis = v; break;
    case S_PLUNGER_REST: p.calibration.rest = v; break;
    case S_PLUNGER_DRAWN: p.calibration.drawn = v; break;
    case S_PLUNGER_DEADZONE: p.calibration.deadzone = static_cast<std::uint8_t>(v); break;
    case S_PLUNGER_RELEASE: p.calibration.release = static_cast<std::uint8_t>(v); break;
    default: break;
    }
}

std::string& text_of(Config& c, int id)
{
    switch (id) {
    case S_ROM: return c.play.rom;
    case S_NUDGE_DEVICE: return c.nudge.device;
    case S_BUTTONS_DEVICE: return c.buttons.device;
    default: return c.plunger.device;
    }
}

std::string range_text(const Row& r, int v)
{
    char out[64];
    if (r.kind == K_BUTTON) {
        if (v < 0) std::snprintf(out, sizeof out, "Off");
        else std::snprintf(out, sizeof out, "Button %d", v);
        return out;
    }
    switch (r.id) {
    case S_NUDGE_X:
    case S_NUDGE_Y: std::snprintf(out, sizeof out, "Axis %d", v); break;
    case S_PLUNGER_AXIS:
        if (v < 0) std::snprintf(out, sizeof out, "Off");
        else std::snprintf(out, sizeof out, "Axis %d", v);
        break;
    case S_NUDGE_STRENGTH: std::snprintf(out, sizeof out, "%d.%dx", v / 10, v % 10); break;
    case S_NUDGE_COOLDOWN: std::snprintf(out, sizeof out, "%d ms", v); break;
    default: std::snprintf(out, sizeof out, "%d%%", v); break;
    }
    return out;
}

// A slider's next value along. A value the file had that is not on the steps
// (a strength of 2.25) is shown as it is and left for the step either side of
// it.
int range_step(const Row& r, int v, int dir)
{
    int n;
    if (dir > 0) n = r.lo + ((v - r.lo) / r.step + 1) * r.step;
    else n = r.lo + ((v - r.lo + r.step - 1) / r.step - 1) * r.step;
    return n < r.lo ? r.lo : n > r.hi ? r.hi : n;
}

// A row's value from one config into another: what Defaults does. The
// windows are not here: Defaults on their tab unplaces all three at once.
void row_copy(Config& to, Config& from, const Row& r)
{
    switch (r.kind) {
    case K_CHOICE:
    case K_RANGE:
    case K_NUMBER:
    case K_BUTTON:
        if (r.id == S_NUDGE_STRENGTH) to.play.nudge_multiplier = from.play.nudge_multiplier;
        else if (!is_window(r.id)) setting_set(to, r.id, setting_get(from, r.id));
        break;
    case K_PATH:
    case K_TEXT: text_of(to, r.id) = text_of(from, r.id); break;
    default: break;
    }
}

// The controllers connected now, by name, for saying when a device named in
// the file is not one of them.
bool controller_named(const std::string& part)
{
    int count = 0;
    SDL_JoystickID* const ids = SDL_GetJoysticks(&count);
    bool found = false;
    for (int i = 0; ids != nullptr && i < count && !found; ++i) {
        const char* const name = SDL_GetJoystickNameForID(ids[i]);
        found = name != nullptr && SDL_strcasestr(name, part.c_str()) != nullptr;
    }
    SDL_free(ids);
    return found;
}

// --- the file ------------------------------------------------------------------
//
// The encodings are the game's: `window.*` as `Cabinet::save` writes them,
// `input.buttons` as `parse_cabinet_buttons` reads it, and a plunger device
// escaped as the game's exit escapes it.

std::array<Placement, kWindows> parse_windows(const std::string& text)
{
    std::istringstream stream(text);
    std::string magic;
    int version = 0;
    if (!(stream >> magic >> version) || magic != "ACWINDOWS" || version != 1)
        throw std::runtime_error("unsupported window placement");
    std::array<Placement, kWindows> out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        std::string role;
        auto& p = out[i];
        int fullscreen = 0;
        if (!(stream >> role >> std::quoted(p.name) >> p.x >> p.y >> p.w >> p.h >> fullscreen >> p.rotation)
            || role != kRoles[i] || p.name.size() > 1024 || p.x < -100000 || p.x > 100000 || p.y < -100000
            || p.y > 100000 || p.w < 64 || p.w > 32768 || p.h < 64 || p.h > 32768 || (fullscreen != 0 && fullscreen != 1)
            || p.rotation < 0 || p.rotation > 270 || p.rotation % 90 != 0)
            throw std::runtime_error("invalid window placement");
        p.fullscreen = fullscreen != 0;
    }
    stream >> std::ws;
    if (!stream.eof()) throw std::runtime_error("unexpected window placement fields");
    return out;
}

// Every field the game reads, from `settings` (cabinet.ini) and, for what it
// does not have, the older files beside it in `data`. Throws what the game
// would stop on.
Config read_config(const CabinetSettings& settings, const std::filesystem::path& data)
{
    Config c;
    c.play = alien_crush::host::parse_play_settings(settings);
    c.nudge = alien_crush::host::parse_cabinet_nudge(settings);
    c.buttons = alien_crush::host::load_cabinet_buttons(data / "cabinet-inputs.txt");
    const bool plunger_here = std::ranges::any_of(settings, [](const auto& f) { return f.first.starts_with("input.plunger."); });
    c.plunger = alien_crush::host::load_cabinet_plunger(plunger_here ? data / "cabinet.ini" : data / "cabinet-plunger.txt");
    std::error_code error;
    if (settings.contains("window.playfield")) {
        std::string text = "ACWINDOWS 1\n";
        for (const auto* role : kRoles) {
            const auto found = settings.find(std::string("window.") + role);
            if (found == settings.end()) throw std::runtime_error(std::string("window.") + role + " is missing");
            text += std::string(role) + ' ' + found->second + '\n';
        }
        c.windows = parse_windows(text);
        c.placed = true;
    } else if (std::filesystem::exists(data / "cabinet-windows.txt", error)) {
        const auto bytes = alien_crush::core::read_file(data / "cabinet-windows.txt", 16384);
        c.windows = parse_windows(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        c.placed = true;
    }
    return c;
}

// `settings` with every field of `c` in it; what else it had stays.
void store_config(CabinetSettings& settings, const Config& c)
{
    alien_crush::host::store_play_settings(settings, c.play);
    const auto& n = c.nudge;
    settings["input.nudge.enabled"] = n.enabled ? "1" : "0";
    settings["input.nudge.device"] = n.device;
    settings["input.nudge.axis_x"] = std::to_string(n.axis_x);
    settings["input.nudge.axis_y"] = std::to_string(n.axis_y);
    settings["input.nudge.threshold"] = std::to_string(n.threshold);
    settings["input.nudge.release"] = std::to_string(n.release);
    settings["input.nudge.cooldown_ms"] = std::to_string(n.cooldown_ms);

    const auto& p = c.plunger;
    std::string device;
    constexpr char hex[] = "0123456789ABCDEF";
    for (const unsigned char ch : p.device) {
        if (ch == '%' || ch == '=' || ch == '\r' || ch == '\n') {
            device += '%';
            device += hex[ch >> 4];
            device += hex[ch & 15];
        } else {
            device += static_cast<char>(ch);
        }
    }
    settings["input.plunger.device"] = device;
    settings["input.plunger.axis"] = std::to_string(p.axis < 0 ? -1 : p.axis);
    settings["input.plunger.rest"] = std::to_string(p.calibration.rest);
    settings["input.plunger.drawn"] = std::to_string(p.calibration.drawn);
    settings["input.plunger.deadzone"] = std::to_string(p.calibration.deadzone);
    settings["input.plunger.release"] = std::to_string(p.calibration.release);

    std::ostringstream buttons;
    if (!c.buttons.device.empty()) {
        buttons << "ACINPUTS2 " << std::quoted(c.buttons.device);
        for (const int b : c.buttons.buttons) buttons << ' ' << b;
    }
    settings["input.buttons"] = buttons.str();

    for (std::size_t i = 0; i < kRoles.size(); ++i) {
        const std::string key = std::string("window.") + kRoles[i];
        if (!c.placed) {
            settings.erase(key);
            continue;
        }
        const auto& w = c.windows[i];
        std::ostringstream line;
        line << std::quoted(w.name) << ' ' << w.x << ' ' << w.y << ' ' << w.w << ' ' << w.h << ' '
             << (w.fullscreen ? 1 : 0) << ' ' << w.rotation;
        settings[key] = line.str();
    }
}

// --- fonts -------------------------------------------------------------------

constexpr int kGlyphFirst = 32;
constexpr int kGlyphCount = 224;  // to 255: Latin-1; anything past it is drawn as '?'

struct Font {
    SDL_Texture* tex = nullptr;
    stbtt_packedchar ch[kGlyphCount]{};
    int w = 0, h = 0;
    float ascent = 0, height = 0;
};

int glyph_of(unsigned cp)
{
    return cp >= static_cast<unsigned>(kGlyphFirst) && cp < static_cast<unsigned>(kGlyphFirst + kGlyphCount)
        ? static_cast<int>(cp) - kGlyphFirst : '?' - kGlyphFirst;
}

bool font_make(Font& f, SDL_Renderer* ren, const unsigned char* ttf, float px)
{
    f = Font{};
    f.w = f.h = px > 60 ? 2048 : 1024;
    std::vector<unsigned char> alpha(static_cast<std::size_t>(f.w) * static_cast<std::size_t>(f.h));
    std::vector<Uint32> argb(alpha.size());
    stbtt_pack_context pc;
    bool ok = stbtt_PackBegin(&pc, alpha.data(), f.w, f.h, 0, 1, nullptr) != 0;
    if (ok) {
        stbtt_PackSetOversampling(&pc, 1, 1);
        ok = stbtt_PackFontRange(&pc, ttf, 0, px, kGlyphFirst, kGlyphCount, f.ch) != 0;
        stbtt_PackEnd(&pc);
    }
    if (ok) {
        for (std::size_t i = 0; i < alpha.size(); ++i) argb[i] = static_cast<Uint32>(alpha[i]) << 24 | 0xFFFFFFu;
        f.tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, f.w, f.h);
        ok = f.tex != nullptr && SDL_UpdateTexture(f.tex, nullptr, argb.data(), f.w * 4);
        if (f.tex != nullptr) SDL_SetTextureBlendMode(f.tex, SDL_BLENDMODE_BLEND);
        stbtt_fontinfo info;
        if (ok && stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0)) != 0) {
            int asc, desc, gap;
            stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
            const float s = stbtt_ScaleForPixelHeight(&info, px);
            f.ascent = static_cast<float>(asc) * s;
            f.height = static_cast<float>(asc - desc + gap) * s;
        }
    }
    return ok;
}

void font_free(Font& f)
{
    if (f.tex != nullptr) SDL_DestroyTexture(f.tex);
    f.tex = nullptr;
}

float text_width(const Font& f, std::string_view s)
{
    float w = 0;
    for (std::size_t i = 0; i < s.size();) w += f.ch[glyph_of(next_code_point(s, i))].xadvance;
    return w;
}

// Left edge `x`, top of the line `y`. Returns where the text ended.
float text(SDL_Renderer* ren, const Font& f, float x, float y, std::string_view s, SDL_Color col)
{
    SDL_SetTextureColorMod(f.tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(f.tex, col.a);
    float px = x, py = y + f.ascent;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned cp = next_code_point(s, i);
        if (cp < static_cast<unsigned>(kGlyphFirst)) continue;
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(f.ch, f.w, f.h, glyph_of(cp), &px, &py, &q, 1);
        const SDL_FRect src = {std::round(q.s0 * static_cast<float>(f.w)), std::round(q.t0 * static_cast<float>(f.h)),
                               std::round((q.s1 - q.s0) * static_cast<float>(f.w)),
                               std::round((q.t1 - q.t0) * static_cast<float>(f.h))};
        const SDL_FRect dst = {std::floor(q.x0), std::floor(q.y0), src.w, src.h};
        if (src.w > 0 && src.h > 0) SDL_RenderTexture(ren, f.tex, &src, &dst);
    }
    return px;
}

std::vector<unsigned char> read_all(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The system's own sans, regular and bold.
std::vector<unsigned char> load_font(bool bold)
{
    wchar_t dir[MAX_PATH];
    const UINT n = GetWindowsDirectoryW(dir, MAX_PATH);
    const std::filesystem::path fonts = std::filesystem::path(n > 0 && n < MAX_PATH ? dir : L"C:\\Windows") / "Fonts";
    for (const char* name : {bold ? "segoeuib.ttf" : "segoeui.ttf", bold ? "arialbd.ttf" : "arial.ttf"}) {
        auto ttf = read_all(fonts / name);
        if (!ttf.empty()) return ttf;
    }
    return bold ? load_font(false) : std::vector<unsigned char>{};
}

// --- the launcher's state -------------------------------------------------------

enum Part { P_ROW, P_DROP, P_OPTION, P_SLIDER, P_FIELD, P_BROWSE, P_DETECT, P_TAB, P_BUTTON };

struct Hot {
    SDL_FRect r;
    Part part;
    int row, index;
};

enum { UI_DEFAULTS, UI_SAVE, UI_QUIT, UI_PLAY, UI_BUTTONS };
constexpr const char* button_names[UI_BUTTONS] = {"Defaults", "Save", "Quit", "Play"};

constexpr SDL_Color C_TEXT = {232, 232, 232, 255};
constexpr SDL_Color C_SOFT = {205, 205, 205, 255};
constexpr SDL_Color C_DIM = {150, 150, 150, 255};
constexpr SDL_Color C_FAINT = {64, 64, 64, 255};
constexpr SDL_Color C_ACCENT = {252, 108, 0, 255};  // the logo's orange
constexpr SDL_Color C_RED = {236, 52, 44, 255};
constexpr SDL_Color C_BLACK = {0, 0, 0, 255};

// What the ROM row last found at a path, so that it is hashed once and not
// on every frame.
struct RomCheck {
    std::string path;
    std::filesystem::path resolved;
    bool found = false;
    bool supported = false;
};

struct Ui {
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    int w = 0, h = 0;
    float scale = 1, mouse_scale = 1;
    std::vector<unsigned char> ttf[2];
    Font body, small, bold, title;
    SDL_Texture* logo = nullptr;  // the heading, when there is one, `logo_n` times the ROM's size
    int logo_n = 0;
    Config cfg, def;
    CabinetSettings rest;        // the file as it was read, for the fields not shown
    std::filesystem::path base;  // the launcher's own directory, where the game runs
    std::filesystem::path data;  // portable-data, absolute
    std::filesystem::path ini;   // its cabinet.ini
    bool ini_exists = false;
    int tab = 0, focus = 0, button = UI_PLAY;
    bool on_buttons = false;
    float scroll = 0;
    int hover = -1;
    bool editing = false;
    int edit_row = 0;
    std::size_t caret = 0;
    std::string edit;
    bool capturing = false;
    int capture_row = 0;
    Uint64 capture_end = 0;
    Uint64 pad_quiet_until = 0;  // a detected press, again as a controller's
    int dragging = -1;
    // The open dropdown's row (-1: none), the option lit in it, the first one
    // in sight, and where its box was drawn.
    int drop_row = -1, drop_sel = 0, drop_first = 0, drop_shown = 0;
    float drop_x = 0, drop_y = 0, drop_w = 0, drop_h = 0;
    std::vector<Hot> hot;
    std::string status;
    bool status_bad = false;
    bool dirty = false;
    int held_dir = 0, stick_x = 0, stick_y = 0;
    Uint64 held_next = 0;
    // Whether the last event changed anything shown. Most do; a mouse move
    // that crosses nothing, and a stick or sensor that drifts, do not, and
    // are not drawn for: a frame for every one of those kept the renderer
    // presenting at the display's refresh rate under a moving cursor.
    bool redraw = true;
    RomCheck rom;
    HANDLE game = nullptr;  // the game that Play started, while it runs
    std::vector<SDL_Joystick*> joysticks;  // open, so that their buttons can be detected
};
Ui ui;

void say(bool bad, const char* fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ui.status = buf;
    ui.status_bad = bad;
}

void changed() { ui.dirty = true; }

std::vector<Row>& tab_rows() { return rows[static_cast<std::size_t>(ui.tab)]; }
int row_count() { return static_cast<int>(tab_rows().size()); }

const Row* row_at(int i) { return i >= 0 && i < row_count() ? &tab_rows()[static_cast<std::size_t>(i)] : nullptr; }

// --- layout ----------------------------------------------------------------------

float L(float v) { return v * ui.scale; }
#ifdef AC_LAUNCHER_LOGO
constexpr float HEADER_H = 122;  // the logo is LOGO_H of it
#else
constexpr float HEADER_H = 74;
#endif
constexpr float TABS_H = 48;
constexpr float ROW_H = 44;
constexpr float HEAD_H = 58;
constexpr float HELP_H = 84;
constexpr float BAR_H = 72;
constexpr float MARGIN = 28;
constexpr float LABEL_W = 250;

std::array<float, kRowsMax + 1> row_y{};  // from the top of the list; the last is its height

float row_height(const Row& r) { return r.kind == K_HEAD ? L(HEAD_H) : L(ROW_H); }

void layout()
{
    float y = L(10);
    for (int i = 0; i < row_count(); ++i) {
        row_y[static_cast<std::size_t>(i)] = y;
        y += row_height(tab_rows()[static_cast<std::size_t>(i)]);
    }
    row_y[static_cast<std::size_t>(row_count())] = y + L(10);
}

float list_top() { return L(HEADER_H + TABS_H); }
float list_h() { return static_cast<float>(ui.h) - L(HELP_H + BAR_H) - list_top(); }

void clamp_scroll()
{
    const float most = row_y[static_cast<std::size_t>(row_count())] - list_h();
    if (ui.scroll > most) ui.scroll = most;
    if (ui.scroll < 0) ui.scroll = 0;
}

void reveal(int i)
{
    layout();
    const auto k = static_cast<std::size_t>(i);
    const float h = row_height(tab_rows()[k]);
    float top = row_y[k];
    // A row just under its heading brings the heading with it.
    if (i > 0 && tab_rows()[k - 1].kind == K_HEAD) top = row_y[k - 1];
    if (top < ui.scroll) ui.scroll = top - L(4);
    if (row_y[k] + h > ui.scroll + list_h()) ui.scroll = row_y[k] + h - list_h() + L(4);
    clamp_scroll();
}

int first_row()
{
    for (int i = 0; i < row_count(); ++i)
        if (tab_rows()[static_cast<std::size_t>(i)].kind != K_HEAD) return i;
    return 0;
}

// --- files and the game ------------------------------------------------------------

void find_base()
{
    std::wstring exe(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
        if (n == 0) break;
        if (n < exe.size()) {
            exe.resize(n);
            ui.base = std::filesystem::path(exe).parent_path();
            return;
        }
        exe.resize(exe.size() * 2);
    }
    const char* b = SDL_GetBasePath();
    ui.base = b != nullptr ? path_of_utf8(b) : std::filesystem::current_path();
}

// The path as it should go in the file: from the launcher's folder when it is
// under it, so that the folder can be moved; as it is otherwise.
std::string relative_to_base(const std::filesystem::path& path)
{
    std::error_code error;
    const auto rel = std::filesystem::relative(path, ui.base, error);
    if (!error && !rel.empty() && rel.begin()->native() != L"..") return utf8_of(rel);
    return utf8_of(path);
}

// Where the ROM is and whether it is the supported one, rechecked only when
// its path changes.
const RomCheck& rom_check()
{
    const std::string& rom = ui.cfg.play.rom;
    if (ui.rom.path != rom || ui.rom.resolved.empty()) {
        ui.rom.path = rom;
        const auto path = path_of_utf8(rom.empty() ? kRomDefault : rom);
        ui.rom.resolved = (path.is_absolute() ? path : ui.base / path).lexically_normal();
        std::error_code error;
        ui.rom.found = std::filesystem::is_regular_file(ui.rom.resolved, error);
        ui.rom.supported = ui.rom.found && alien_crush::rom::validate_file(ui.rom.resolved).supported();
    }
    return ui.rom;
}

void load()
{
    find_monitors();
    ui.data = (ui.base / kDataRelative).lexically_normal();
    ui.ini = (ui.base / kConfigRelative).lexically_normal();
    std::error_code error;
    ui.ini_exists = std::filesystem::exists(ui.ini, error);
    ui.def = Config{};
    ui.cfg = Config{};
    ui.rest.clear();
    const auto name = utf8_of(kConfigRelative);
    try {
        // What is missing reads as empty, and the older files beside it still
        // count, as they do for the game.
        auto settings = alien_crush::host::read_cabinet_settings(ui.ini);
        ui.cfg = read_config(settings, ui.data);
        ui.rest = std::move(settings);
        if (ui.ini_exists) say(false, "Read %s", name.c_str());
        else say(false, "There is no %s yet. Save or Play makes one here.", name.c_str());
    } catch (const std::exception& e) {
        ui.cfg = Config{};
        say(true, "%s could not be read (%s), and the game would stop on it. The defaults are shown here; "
            "Save replaces it.", name.c_str(), e.what());
    }
    if (ui.cfg.play.rom.empty()) ui.cfg.play.rom = kRomDefault;
}

void commit_edit();
void drop_close();

// The file's own rules, which a save that broke would leave the game unable
// to read the file at all. Said, and the tab that has it shown.
bool check_before_saving()
{
    const auto& n = ui.cfg.nudge;
    const auto& b = ui.cfg.buttons;
    const char* wrong = nullptr;
    int tab = TAB_CABINET;
    if (n.device.empty()) wrong = "The nudge board needs a device name.";
    else if (n.axis_x == n.axis_y) wrong = "The nudge board's X and Y axes are the same axis.";
    else if (n.release >= n.threshold) wrong = "The nudge board's release has to be below its threshold.";
    else if (b.device.empty() && std::ranges::any_of(b.buttons, [](int v) { return v >= 0; })) {
        wrong = "The buttons need the board's device name.";
        tab = TAB_BUTTONS;
    }
    if (wrong == nullptr) return true;
    if (ui.tab != tab) {
        ui.tab = tab;
        ui.focus = first_row();
        ui.scroll = 0;
        ui.on_buttons = false;
    }
    say(true, "%s Nothing is written until it is fixed.", wrong);
    return false;
}

bool save()
{
    if (ui.editing) commit_edit();
    if (!check_before_saving()) return false;
    try {
        auto settings = ui.rest;
        store_config(settings, ui.cfg);
        alien_crush::host::write_cabinet_settings(ui.ini, settings);
        ui.rest = std::move(settings);
    } catch (const std::exception& e) {
        say(true, "Could not write %s: %s", utf8_of(ui.ini).c_str(), e.what());
        return false;
    }
    ui.ini_exists = true;
    ui.dirty = false;
    say(false, "Saved %s", utf8_of(kConfigRelative).c_str());
    return true;
}

// True once the game Play started has closed, with its exit code.
bool game_closed(DWORD& code)
{
    if (WaitForSingleObject(ui.game, 0) != WAIT_OBJECT_0) return false;
    code = 0;
    GetExitCodeProcess(ui.game, &code);
    CloseHandle(ui.game);
    ui.game = nullptr;
    return true;
}

// Never the launcher's end: it waits for the game, out of sight.
bool launch()
{
    const auto& rom = rom_check();
    if (!rom.supported) {
        ui.tab = TAB_GAME;
        ui.on_buttons = false;
        ui.focus = 0;
        ui.scroll = 0;
        if (!rom.found) say(true, "The ROM is not at %s. Choose it here first.", utf8_of(rom.resolved).c_str());
        else say(true, "%s is not the supported ROM. Choose the supported dump here first.", utf8_of(rom.resolved).c_str());
        return false;
    }
    const auto exe = ui.base / kGameExe;
    std::error_code error;
    if (!std::filesystem::is_regular_file(exe, error)) {
        say(true, "alien-crush.exe is not beside the launcher, in %s", utf8_of(ui.base).c_str());
        return false;
    }
    if (!save()) return false;
    // No options: the game reads what was just saved, from the folder it is
    // started in.
    std::wstring cmd = L"\"" + exe.wstring() + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, ui.base.c_str(), &si, &pi)) {
        say(true, "Could not start %s (error %lu)", utf8_of(exe).c_str(), static_cast<unsigned long>(GetLastError()));
        return false;
    }
    CloseHandle(pi.hThread);
    ui.game = pi.hProcess;
    ui.held_dir = 0;
    drop_close();
    SDL_HideWindow(ui.win);
    return false;
}

// A ROM chosen in the system's dialog.
bool browse(std::string& out)
{
    wchar_t file[MAX_PATH * 4] = L"";
    const std::wstring dir = ui.base.wstring();
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    const SDL_PropertiesID props = SDL_GetWindowProperties(ui.win);
    ofn.hwndOwner = static_cast<HWND>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrInitialDir = dir.c_str();
    ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = L"The ROM";
    ofn.lpstrFilter = L"PC Engine HuCards (*.pce)\0*.pce\0All files\0*.*\0";
    if (GetOpenFileNameW(&ofn) == 0) return false;
    out = relative_to_base(std::filesystem::path(file));
    return true;
}

// --- changing things ----------------------------------------------------------------

void begin_edit(int i)
{
    const Row* r = row_at(i);
    ui.editing = true;
    ui.edit_row = i;
    ui.edit = r->kind == K_NUMBER ? std::to_string(setting_get(ui.cfg, r->id)) : text_of(ui.cfg, r->id);
    ui.caret = ui.edit.size();
    SDL_StartTextInput(ui.win);
}

void end_edit()
{
    ui.editing = false;
    SDL_StopTextInput(ui.win);
}

void set_text(int id, const std::string& value)
{
    auto& dst = text_of(ui.cfg, id);
    if (dst != value) {
        dst = value;
        changed();
    }
}

void commit_edit()
{
    const Row* r = row_at(ui.edit_row);
    end_edit();
    if (r == nullptr) return;
    std::string t = trimmed(ui.edit);
    if (r->kind == K_NUMBER) {
        char* end = nullptr;
        const long long v = std::strtoll(t.c_str(), &end, 10);
        if (t.empty() || *end != '\0' || v < r->lo || v > r->hi) {
            say(true, "%s wants a whole number; it is as it was.", r->label);
            return;
        }
        if (v != setting_get(ui.cfg, r->id)) {
            setting_set(ui.cfg, r->id, static_cast<int>(v));
            changed();
        }
        return;
    }
    // "Copy as path" puts quotes round it.
    if (r->kind == K_PATH && t.size() >= 2 && t.front() == '"' && t.back() == '"') t = t.substr(1, t.size() - 2);
    if (r->id == S_ROM && t.empty()) { say(true, "The ROM needs a path; it is as it was."); return; }
    if (r->id == S_NUDGE_DEVICE && t.empty()) { say(true, "The nudge board needs a name; it is as it was."); return; }
    set_text(r->id, t);
}

void edit_insert(std::string_view s)
{
    if (ui.edit.size() + s.size() > 1024) return;
    ui.edit.insert(ui.caret, s);
    ui.caret += s.size();
}

void start_capture(int i)
{
    ui.capturing = true;
    ui.capture_row = i;
    ui.capture_end = SDL_GetTicks() + 5000;
    ui.status.clear();
}

// Backspace, or West: the row's button off.
void remove_binding(int i)
{
    const Row* r = row_at(i);
    if (r == nullptr || r->kind != K_BUTTON || setting_get(ui.cfg, r->id) < 0) return;
    setting_set(ui.cfg, r->id, -1);
    changed();
}

// A button pressed on a joystick while one was waited for: its number, and
// the board's name when there is none yet. A press on some other controller
// is said, and changes nothing.
void capture_button(SDL_JoystickID which, int button)
{
    const Row* r = row_at(ui.capture_row);
    ui.capturing = false;
    // The same press, when the board is a gamepad too, comes again as the
    // gamepad's, which would be taken as the launcher's own controls.
    ui.pad_quiet_until = SDL_GetTicks() + 250;
    const char* const found = SDL_GetJoystickNameForID(which);
    const std::string name = found != nullptr ? found : "";
    auto& device = ui.cfg.buttons.device;
    if (device.empty()) {
        if (name.empty()) {
            say(true, "That controller has no name to find it by.");
            return;
        }
        device = name;
    } else if (SDL_strcasestr(name.c_str(), device.c_str()) == nullptr) {
        say(true, "That was %s, which is not the board (%s).", name.c_str(), device.c_str());
        return;
    }
    setting_set(ui.cfg, r->id, button);
    changed();
    say(false, "%s is button %d on %s.", r->label, button, name.c_str());
}

void move_focus(int d)
{
    if (ui.on_buttons) {
        if (d < 0) {
            ui.on_buttons = false;
            for (int i = row_count() - 1; i >= 0; --i)
                if (tab_rows()[static_cast<std::size_t>(i)].kind != K_HEAD) { ui.focus = i; break; }
            reveal(ui.focus);
        }
        return;
    }
    for (int i = ui.focus + d; i >= 0 && i < row_count(); i += d)
        if (tab_rows()[static_cast<std::size_t>(i)].kind != K_HEAD) { ui.focus = i; reveal(i); return; }
    if (d > 0) ui.on_buttons = true;
    else ui.scroll = 0;
}

void set_tab(int t)
{
    if (ui.editing) commit_edit();
    drop_close();
    ui.capturing = false;
    ui.tab = (t + TAB_COUNT) % TAB_COUNT;
    ui.focus = first_row();
    ui.scroll = 0;
    ui.on_buttons = false;
}

// --- dropdowns -------------------------------------------------------------------------

constexpr int kDropShown = 10;

// The values of a row with a fixed list, in the order the list shows them;
// none for a row without one. A button's is Off and the first sixteen, and an
// axis or a button the file had that is past the usual sixteen is in the list
// in its place.
std::vector<int> row_options(const Row& r)
{
    std::vector<int> out;
    if (is_monitor(r.id)) {
        for (int i = 0; i < static_cast<int>(monitors.list.size()); ++i) out.push_back(i);
        if (setting_get(ui.cfg, r.id) == kMonitorAbsent) out.push_back(kMonitorAbsent);
    } else if (r.kind == K_CHOICE) {
        for (int i = 0; i < r.count; ++i) out.push_back(r.choices[i].value);
    } else if ((r.kind == K_RANGE && !r.slider) || r.kind == K_BUTTON) {
        const int shown_hi = r.hi > 15 ? 15 : r.hi;
        for (int v = r.lo; v <= shown_hi; v += r.step) out.push_back(v);
        const int cur = setting_get(ui.cfg, r.id);
        if (cur > shown_hi && cur <= r.hi) out.push_back(cur);
    }
    return out;
}

std::string option_text(const Row& r, int v)
{
    if (is_monitor(r.id)) {
        if (v == kMonitorAbsent) return ui.cfg.windows[static_cast<std::size_t>(window_of(r.id))].name + " (not connected)";
        if (v >= 0 && v < static_cast<int>(monitors.labels.size())) return monitors.labels[static_cast<std::size_t>(v)];
        return "No monitor found";
    }
    if (r.kind == K_CHOICE) {
        for (int k = 0; k < r.count; ++k)
            if (r.choices[k].value == v) return r.choices[k].label;
        return std::to_string(v);
    }
    return range_text(r, v);
}

int option_index(const std::vector<int>& opts, int v)
{
    const auto at = std::ranges::find(opts, v);
    return at == opts.end() ? 0 : static_cast<int>(at - opts.begin());
}

void drop_reveal(int n)
{
    if (ui.drop_sel < ui.drop_first) ui.drop_first = ui.drop_sel;
    if (ui.drop_sel >= ui.drop_first + ui.drop_shown) ui.drop_first = ui.drop_sel - ui.drop_shown + 1;
    if (ui.drop_first > n - ui.drop_shown) ui.drop_first = n - ui.drop_shown;
    if (ui.drop_first < 0) ui.drop_first = 0;
}

void drop_open(int i)
{
    if (ui.editing) commit_edit();
    const Row* r = row_at(i);
    const auto opts = r != nullptr ? row_options(*r) : std::vector<int>{};
    if (opts.empty()) return;
    ui.focus = i;
    ui.on_buttons = false;
    reveal(i);
    ui.drop_row = i;
    ui.drop_sel = option_index(opts, setting_get(ui.cfg, r->id));
    // The value in the middle of what shows, where there is room.
    ui.drop_shown = kDropShown;
    ui.drop_first = ui.drop_sel - kDropShown / 2;
    drop_reveal(static_cast<int>(opts.size()));
}

void drop_close() { ui.drop_row = -1; }

void drop_move(int d)
{
    const int n = static_cast<int>(row_options(*row_at(ui.drop_row)).size());
    ui.drop_sel = std::clamp(ui.drop_sel + d, 0, n - 1);
    drop_reveal(n);
}

void drop_pick()
{
    const Row* r = row_at(ui.drop_row);
    const auto opts = row_options(*r);
    drop_close();
    if (ui.drop_sel < 0 || ui.drop_sel >= static_cast<int>(opts.size())) return;
    const int v = opts[static_cast<std::size_t>(ui.drop_sel)];
    if (v != setting_get(ui.cfg, r->id)) {
        setting_set(ui.cfg, r->id, v);
        changed();
    }
}

void change(int d)
{
    if (ui.on_buttons) {
        ui.button = (ui.button + d + UI_BUTTONS) % UI_BUTTONS;
        return;
    }
    const Row* r = row_at(ui.focus);
    if (r == nullptr || (r->kind != K_CHOICE && r->kind != K_RANGE && r->kind != K_BUTTON)) return;
    const int v = setting_get(ui.cfg, r->id);
    int nv = v;
    const auto opts = row_options(*r);
    if (!opts.empty()) {
        // A choice goes round; a list of numbers stops at its ends.
        const int n = static_cast<int>(opts.size());
        int to = option_index(opts, v) + d;
        if (r->kind == K_CHOICE) to = (to + n) % n;
        else to = std::clamp(to, 0, n - 1);
        nv = opts[static_cast<std::size_t>(to)];
    } else {
        nv = range_step(*r, v, d);
    }
    if (nv != v) {
        setting_set(ui.cfg, r->id, nv);
        changed();
    }
}

bool press_button(int b)
{
    switch (b) {
    case UI_DEFAULTS:
        for (const auto& r : tab_rows()) row_copy(ui.cfg, ui.def, r);
        if (ui.tab == TAB_GAME && ui.cfg.play.rom.empty()) ui.cfg.play.rom = kRomDefault;
        // The windows go back to wherever the game first opens them.
        if (ui.tab == TAB_VIDEO) {
            ui.cfg.placed = false;
            ui.cfg.windows = {};
        }
        changed();
        say(false, "%s is back to its defaults. Nothing is written until Save or Play.", tab_names[ui.tab]);
        return false;
    case UI_SAVE: save(); return false;
    case UI_PLAY: return launch();
    case UI_QUIT: return true;
    default: return false;
    }
}

bool activate(bool from_pad)
{
    if (ui.drop_row >= 0) { drop_pick(); return false; }
    if (ui.on_buttons) return press_button(ui.button);
    const Row* r = row_at(ui.focus);
    if (r == nullptr) return false;
    switch (r->kind) {
    case K_CHOICE:
    case K_RANGE: drop_open(ui.focus); break;
    case K_PATH:
        if (from_pad) {
            std::string path;
            if (browse(path)) set_text(r->id, path);
        } else {
            begin_edit(ui.focus);
        }
        break;
    case K_TEXT:
    case K_NUMBER: begin_edit(ui.focus); break;
    case K_BUTTON: start_capture(ui.focus); break;
    default: break;
    }
    return false;
}

// --- drawing --------------------------------------------------------------------------

void fill(float x, float y, float w, float h, SDL_Color c)
{
    SDL_SetRenderDrawColor(ui.ren, c.r, c.g, c.b, c.a);
    const SDL_FRect r = {std::floor(x), std::floor(y), std::floor(w), std::floor(h)};
    SDL_RenderFillRect(ui.ren, &r);
}

void outline(float x, float y, float w, float h, float t, SDL_Color c)
{
    fill(x, y, w, t, c);
    fill(x, y + h - t, w, t, c);
    fill(x, y, t, h, c);
    fill(x + w - t, y, t, h, c);
}

// Pointing down, or up when `up`.
void chevron(float cx, float cy, float size, bool up, SDL_Color c)
{
    SDL_Vertex v[3]{};
    const float hw = size, hh = size * 0.55f, dir = up ? -1.0f : 1.0f;
    v[0].position = {cx, cy + dir * hh};
    v[1].position = {cx - hw, cy - dir * hh};
    v[2].position = {cx + hw, cy - dir * hh};
    for (auto& vertex : v) vertex.color = {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
    SDL_RenderGeometry(ui.ren, nullptr, v, 3, nullptr, 0);
}

void hot(float x, float y, float w, float h, Part part, int row, int index)
{
    if (ui.hot.size() >= 512) return;
    // Only what can be seen of a row in the list can be clicked.
    if (part != P_TAB && part != P_BUTTON && part != P_OPTION) {
        const float top = list_top(), bottom = top + list_h();
        if (y < top) { h -= top - y; y = top; }
        if (y + h > bottom) h = bottom - y;
        if (h <= 0) return;
    }
    ui.hot.push_back(Hot{{x, y, w, h}, part, row, index});
}

// Text cut to `w` from the left, with an ellipsis: a path's end is the part
// worth seeing.
void text_tail(const Font& f, float x, float y, float w, std::string_view s, SDL_Color c)
{
    if (text_width(f, s) <= w) { text(ui.ren, f, x, y, s, c); return; }
    const float dots = text_width(f, "...");
    while (!s.empty() && text_width(f, s) + dots > w) s.remove_prefix(next_boundary(s, 0));
    const float e = text(ui.ren, f, x, y, "...", c);
    text(ui.ren, f, e, y, s, c);
}

// Words laid into `w`, `max_lines` at most; returns how many lines it took.
// Drawn unless `measure`.
int wrap_lines(const Font& f, float x, float y, float w, std::string_view s, SDL_Color c, int max_lines, bool measure)
{
    int lines = 0;
    while (!s.empty() && lines < max_lines) {
        std::size_t take = 0, fit = 0;
        for (;;) {
            while (take < s.size() && s[take] != ' ') ++take;
            if (fit != 0 && text_width(f, s.substr(0, take)) > w) break;
            fit = take;
            if (take >= s.size()) break;
            ++take;
        }
        if (!measure) text(ui.ren, f, x, y + static_cast<float>(lines) * f.height, s.substr(0, fit), c);
        ++lines;
        s.remove_prefix(fit);
        while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    }
    return lines;
}

float wrap(const Font& f, float x, float y, float w, std::string_view s, SDL_Color c, int max_lines)
{
    return static_cast<float>(wrap_lines(f, x, y, w, s, c, max_lines, false)) * f.height;
}

// On is lit and Off dim, and so is a button or an axis that is off; anything
// else is text.
SDL_Color option_color(const Row& r, int v)
{
    if (r.choices == on_off) return v != 0 ? C_ACCENT : C_DIM;
    if ((r.kind == K_BUTTON || r.id == S_PLUNGER_AXIS) && v < 0) return C_DIM;
    return C_TEXT;
}

void draw_dropdown(int i, const Row& r, float x, float y, float w, float h, bool focused)
{
    const int v = setting_get(ui.cfg, r.id);
    const bool open = ui.drop_row == i;
    fill(x, y, w, h, open ? SDL_Color{18, 18, 18, 255} : C_BLACK);
    outline(x, y, w, h, L(1), open ? C_ACCENT : focused ? C_DIM : C_FAINT);
    text_tail(ui.body, x + L(14), y + (h - ui.body.height) / 2, w - L(54), option_text(r, v), option_color(r, v));
    chevron(x + w - L(22), y + h / 2, L(6), open, focused || open ? C_ACCENT : C_DIM);
    hot(x, y, w, h, P_DROP, i, 0);
    if (open) {
        ui.drop_x = x;
        ui.drop_y = y;
        ui.drop_w = w;
        ui.drop_h = h;
    }
}

// The open list, over everything else: below its box, or above it when there
// is more room there.
void draw_drop_list()
{
    const Row* r = row_at(ui.drop_row);
    const auto opts = r != nullptr ? row_options(*r) : std::vector<int>{};
    const int n = static_cast<int>(opts.size());
    if (n == 0) { drop_close(); return; }
    const float ih = L(34), pad = L(4);
    const float below = static_cast<float>(ui.h) - L(8) - (ui.drop_y + ui.drop_h);
    const float above = ui.drop_y - L(8);
    int shown = n < kDropShown ? n : kDropShown;
    const bool up = static_cast<float>(shown) * ih + pad * 2 > below && above > below;
    const int fits = static_cast<int>(((up ? above : below) - pad * 2) / ih);
    if (shown > fits) shown = fits < 1 ? 1 : fits;
    if (shown != ui.drop_shown) {
        ui.drop_shown = shown;
        drop_reveal(n);
    }
    ui.drop_first = std::clamp(ui.drop_first, 0, std::max(0, n - shown));
    const float lh = static_cast<float>(shown) * ih + pad * 2;
    const float lx = ui.drop_x, lw = ui.drop_w;
    const float ly = up ? ui.drop_y - lh + L(1) : ui.drop_y + ui.drop_h - L(1);
    fill(lx, ly, lw, lh, SDL_Color{18, 18, 18, 255});
    outline(lx, ly, lw, lh, L(1), C_ACCENT);
    const int cur = setting_get(ui.cfg, r->id);
    for (int k = 0; k < shown; ++k) {
        const int o = ui.drop_first + k;
        const float iy = ly + pad + static_cast<float>(k) * ih;
        if (o == ui.drop_sel) {
            fill(lx + L(1), iy, lw - L(2), ih, SDL_Color{40, 40, 40, 255});
            fill(lx + L(1), iy, L(4), ih, C_ACCENT);
        }
        const int v = opts[static_cast<std::size_t>(o)];
        const SDL_Color c = v == cur ? C_ACCENT : o == ui.drop_sel ? C_TEXT : C_SOFT;
        text_tail(ui.body, lx + L(14), iy + (ih - ui.body.height) / 2, lw - L(34), option_text(*r, v), c);
        hot(lx, iy, lw, ih, P_OPTION, ui.drop_row, o);
    }
    if (n > shown) {
        const float track = lh - pad * 2;
        const float bar_h = track * static_cast<float>(shown) / static_cast<float>(n);
        const float bar_y = ly + pad + track * static_cast<float>(ui.drop_first) / static_cast<float>(n);
        fill(lx + lw - L(8), bar_y, L(3), bar_h, C_DIM);
    }
}

void draw_slider(int i, const Row& r, float x, float y, float w, float h, bool focused)
{
    const int v = setting_get(ui.cfg, r.id);
    const float track_w = w - L(96);
    const float frac = std::clamp(static_cast<float>(v - r.lo) / static_cast<float>(r.hi - r.lo), 0.0f, 1.0f);
    const float mid = y + h / 2;
    fill(x, mid - L(3), track_w, L(6), C_FAINT);
    fill(x, mid - L(3), track_w * frac, L(6), focused ? C_ACCENT : C_DIM);
    const float kx = x + track_w * frac;
    fill(kx - L(5), mid - L(11), L(10), L(22), focused ? C_TEXT : C_DIM);
    text(ui.ren, ui.body, x + track_w + L(18), y + (h - ui.body.height) / 2, range_text(r, v), C_TEXT);
    hot(x - L(8), y, track_w + L(16), h, P_SLIDER, i, 0);
}

void draw_field(int i, const Row& r, float x, float y, float w, float h, bool focused)
{
    const float bw = r.kind == K_PATH ? L(96) : 0;
    const float fw = bw > 0 ? w - bw - L(8) : w;
    const bool editing = ui.editing && ui.edit_row == i;
    std::string value = editing ? ui.edit : r.kind == K_NUMBER ? std::to_string(setting_get(ui.cfg, r.id)) : text_of(ui.cfg, r.id);
    bool bad = false;
    if (r.id == S_ROM && !editing) bad = !rom_check().supported;
    fill(x, y, fw, h, SDL_Color{14, 14, 14, 255});
    outline(x, y, fw, h, L(1), editing ? C_ACCENT : bad ? C_RED : focused ? C_DIM : C_FAINT);
    const float tx = x + L(10), ty = y + (h - ui.body.height) / 2, tw = fw - L(20);
    SDL_Rect clip = {static_cast<int>(x), static_cast<int>(y), static_cast<int>(fw), static_cast<int>(h)};
    const SDL_Rect list = {0, static_cast<int>(list_top()), ui.w, static_cast<int>(list_h())};
    SDL_GetRectIntersection(&clip, &list, &clip);
    SDL_SetRenderClipRect(ui.ren, &clip);
    if (editing) {
        // The caret kept in view: the text slides left under it.
        const float cw = text_width(ui.body, std::string_view(ui.edit).substr(0, ui.caret));
        const float shift = cw > tw ? cw - tw : 0;
        text(ui.ren, ui.body, tx - shift, ty, ui.edit, C_TEXT);
        fill(tx - shift + cw, ty + L(2), L(2), ui.body.height - L(4), C_ACCENT);
    } else if (!value.empty()) {
        text_tail(ui.body, tx, ty, tw, value, bad ? C_RED : C_TEXT);
    } else if (r.id == S_PLUNGER_DEVICE) {
        text(ui.ren, ui.body, tx, ty, "Any controller", C_DIM);
    }
    SDL_SetRenderClipRect(ui.ren, &list);
    hot(x, y, fw, h, P_FIELD, i, 0);
    if (bw > 0) {
        outline(x + fw + L(8), y, bw, h, L(1), focused ? C_DIM : C_FAINT);
        const float lw = text_width(ui.body, "Browse");
        text(ui.ren, ui.body, x + fw + L(8) + (bw - lw) / 2, ty, "Browse", C_TEXT);
        hot(x + fw + L(8), y, bw, h, P_BROWSE, i, 0);
    }
}

// A board's button: its number as a dropdown, or the wait for a press, and
// Detect beside it, as Browse is beside the ROM.
void draw_button_row(int i, const Row& r, float x, float y, float w, float h, bool focused)
{
    const float bw = L(96);
    const float fw = w - bw - L(8);
    if (ui.capturing && ui.capture_row == i) {
        const auto left = static_cast<int>((ui.capture_end - SDL_GetTicks() + 999) / 1000);
        char msg[64];
        std::snprintf(msg, sizeof msg, "Press a button on the board... %d", left);
        fill(x, y, fw, h, C_BLACK);
        outline(x, y, fw, h, L(2), C_ACCENT);
        text_tail(ui.body, x + L(14), y + (h - ui.body.height) / 2, fw - L(28), msg, C_ACCENT);
    } else {
        draw_dropdown(i, r, x, y, fw, h, focused);
    }
    outline(x + fw + L(8), y, bw, h, L(1), focused ? C_DIM : C_FAINT);
    const float lw = text_width(ui.body, "Detect");
    text(ui.ren, ui.body, x + fw + L(8) + (bw - lw) / 2, y + (h - ui.body.height) / 2, "Detect", C_TEXT);
    hot(x + fw + L(8), y, bw, h, P_DETECT, i, 0);
}

// The title's logo as the heading, if the launcher was built with it, LOGO_H
// high. It is blown up by whole pixels to the next size past that and then
// drawn smoothly down, so that its pixels stay sharp at any scale, and as wide
// as the cartridge's pixels are (AC_LOGO_ASPECT). Its width is put in `w`.
constexpr float LOGO_H = 104;
bool draw_logo(float& w)
{
#ifdef AC_LAUNCHER_LOGO
    const float k = L(LOGO_H) / static_cast<float>(AC_LOGO_H);
    const int n = k <= 1 ? 1 : static_cast<int>(std::ceil(k));
    if (ui.logo_n != n) {
        if (ui.logo != nullptr) SDL_DestroyTexture(ui.logo);
        ui.logo_n = 0;
        const int tw = AC_LOGO_W * n, th = AC_LOGO_H * n;
        ui.logo = SDL_CreateTexture(ui.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, tw, th);
        if (ui.logo == nullptr) return false;
        std::vector<std::uint32_t> big(static_cast<std::size_t>(tw) * static_cast<std::size_t>(th));
        for (int y = 0; y < th; ++y)
            for (int x = 0; x < tw; ++x)
                big[static_cast<std::size_t>(y) * static_cast<std::size_t>(tw) + static_cast<std::size_t>(x)] =
                    ac_logo[(y / n) * AC_LOGO_W + x / n];
        SDL_UpdateTexture(ui.logo, nullptr, big.data(), tw * 4);
        SDL_SetTextureBlendMode(ui.logo, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(ui.logo, SDL_SCALEMODE_LINEAR);
        ui.logo_n = n;
    }
    const float h = std::round(L(LOGO_H));
    const SDL_FRect to = {std::round(L(MARGIN)), std::round((L(HEADER_H) - h) / 2),
                          std::round(static_cast<float>(AC_LOGO_W) * AC_LOGO_ASPECT * k), h};
    SDL_RenderTexture(ui.ren, ui.logo, nullptr, &to);
    w = to.w;
    return true;
#else
    (void)w;
    return false;
#endif
}

void draw()
{
    SDL_GetCurrentRenderOutputSize(ui.ren, &ui.w, &ui.h);
    layout();
    clamp_scroll();
    ui.hot.clear();
    SDL_SetRenderDrawColor(ui.ren, 0, 0, 0, 255);
    SDL_RenderClear(ui.ren);
    const auto W = static_cast<float>(ui.w), H = static_cast<float>(ui.h);

    // The heading, and the file this is.
    float tw, sy;
    if (!draw_logo(tw)) {
        text(ui.ren, ui.title, L(MARGIN), L(18), "ALIEN CRUSH", C_ACCENT);
        tw = text_width(ui.title, "ALIEN CRUSH");
        sy = L(18) + ui.title.ascent - ui.body.ascent;
    } else {
        sy = (L(HEADER_H) - ui.body.height) / 2;
    }
    // Just after the heading: "Config File:" whole, and as much of the path's
    // end as fits after it.
    static const char label[] = "Config File: ";
    const float label_x = L(MARGIN) + tw + L(24), label_w = text_width(ui.body, label);
    const float room = W - L(MARGIN) - label_x - label_w;
    if (room > L(60)) {
        text(ui.ren, ui.body, label_x, sy, label, C_TEXT);
        text_tail(ui.body, label_x + label_w, sy, room, utf8_of(ui.ini), C_DIM);
    }

    // The tabs.
    float x = L(MARGIN) - L(12);
    const float ty = L(HEADER_H);
    for (int t = 0; t < TAB_COUNT; ++t) {
        // Measured bold whichever it is, so that the tabs keep their places.
        const Font& f = t == ui.tab ? ui.bold : ui.body;
        const float w = text_width(ui.bold, tab_names[t]) + L(24);
        const float tx = x + (w - text_width(f, tab_names[t])) / 2;
        text(ui.ren, f, tx, ty + (L(TABS_H) - f.height) / 2, tab_names[t], t == ui.tab ? C_TEXT : C_DIM);
        if (t == ui.tab) fill(tx, ty + L(TABS_H) - L(3), text_width(f, tab_names[t]), L(3), C_ACCENT);
        hot(x, ty, w, L(TABS_H), P_TAB, -1, t);
        x += w;
    }
    fill(0, ty + L(TABS_H) - L(1), W, L(1), C_FAINT);

    // The rows.
    const float top = list_top(), lh = list_h();
    const SDL_Rect list = {0, static_cast<int>(top), ui.w, static_cast<int>(lh)};
    SDL_SetRenderClipRect(ui.ren, &list);
    const float cx = L(MARGIN) + L(LABEL_W);
    const float cw_full = W - cx - L(MARGIN) - L(12);
    const float cw = cw_full < L(460) ? cw_full : L(460);
    for (int i = 0; i < row_count(); ++i) {
        const Row& r = tab_rows()[static_cast<std::size_t>(i)];
        const float y = top + row_y[static_cast<std::size_t>(i)] - ui.scroll;
        const float h = row_height(r);
        if (y + h < top || y > top + lh) continue;
        if (r.kind == K_HEAD) {
            text(ui.ren, ui.bold, L(MARGIN), y + h - ui.bold.height - L(8), r.label, C_ACCENT);
            fill(L(MARGIN), y + h - L(4), W - L(MARGIN) * 2, L(1), C_FAINT);
            continue;
        }
        const bool focused = !ui.on_buttons && ui.focus == i;
        if (focused) {
            fill(0, y, W, h, SDL_Color{26, 26, 26, 255});
            fill(0, y, L(4), h, C_ACCENT);
        } else if (ui.hover == i) {
            fill(0, y, W, h, SDL_Color{14, 14, 14, 255});
        }
        hot(0, y, W, h, P_ROW, i, 0);
        text(ui.ren, ui.body, L(MARGIN), y + (h - ui.body.height) / 2, r.label, focused ? C_TEXT : C_SOFT);
        const float ry = y + L(6), rh = h - L(12);
        switch (r.kind) {
        case K_CHOICE: draw_dropdown(i, r, cx, ry, cw, rh, focused); break;
        case K_RANGE:
            if (r.slider) draw_slider(i, r, cx, ry, cw, rh, focused);
            else draw_dropdown(i, r, cx, ry, cw, rh, focused);
            break;
        case K_PATH: draw_field(i, r, cx, ry, cw_full, rh, focused); break;
        case K_TEXT:
        case K_NUMBER: draw_field(i, r, cx, ry, cw, rh, focused); break;
        default: draw_button_row(i, r, cx, ry, cw, rh, focused); break;
        }
    }
    SDL_SetRenderClipRect(ui.ren, nullptr);
    const float content = row_y[static_cast<std::size_t>(row_count())];
    if (content > lh) {
        const float bar_h = lh * lh / content, bar_y = top + (lh - bar_h) * ui.scroll / (content - lh);
        fill(W - L(6), bar_y, L(3), bar_h, C_FAINT);
    }

    // What the focused thing is, and anything wrong with it.
    const float help_y = H - L(HELP_H + BAR_H);
    fill(0, help_y, W, L(1), C_FAINT);
    const float help_w = W - L(MARGIN) * 2;
    float hy = help_y + L(10);
    if (ui.on_buttons) {
        static const char* const button_help[UI_BUTTONS] = {
            "Put this tab back to its defaults. Nothing is written until Save or Play.",
            "Write these settings to the file. Ctrl+S.",
            "Close the launcher. Escape.",
            "Save, then start the game. Ctrl+Enter or F5, or Start on a controller.",
        };
        wrap(ui.small, L(MARGIN), hy, help_w, button_help[ui.button], C_DIM, 2);
    } else if (const Row* r = row_at(ui.focus)) {
        std::string warn;
        if (r->id == S_ROM && r->kind == K_PATH) {
            const auto& rom = rom_check();
            if (!rom.found) warn = "Not found: " + utf8_of(rom.resolved);
            else if (!rom.supported) warn = "Not the supported ROM: " + utf8_of(rom.resolved);
        } else {
            // A device named that nothing connected now answers to.
            const std::string* device = nullptr;
            if (r->id == S_NUDGE_DEVICE && ui.cfg.nudge.enabled) device = &ui.cfg.nudge.device;
            else if (r->id == S_PLUNGER_DEVICE && ui.cfg.plunger.axis >= 0) device = &ui.cfg.plunger.device;
            else if (r->id == S_BUTTONS_DEVICE || r->kind == K_BUTTON) device = &ui.cfg.buttons.device;
            if (device != nullptr && !device->empty() && !controller_named(*device))
                warn = "No controller connected now has '" + *device + "' in its name.";
        }
        const int warn_lines = warn.empty() ? 0 : wrap_lines(ui.small, 0, 0, help_w, warn, C_RED, 2, true);
        hy += wrap(ui.small, L(MARGIN), hy, help_w, r->help, C_DIM, 3 - warn_lines);
        if (!warn.empty()) wrap(ui.small, L(MARGIN), hy, help_w, warn, C_RED, 2);
    }

    // The buttons, and what just happened.
    const float by = H - L(BAR_H) + L(14), bh = L(BAR_H) - L(28);
    float bx[UI_BUTTONS], bw[UI_BUTTONS];
    for (int b = 0; b < UI_BUTTONS; ++b) bw[b] = text_width(ui.bold, button_names[b]) + L(44);
    bx[UI_DEFAULTS] = L(MARGIN);
    bx[UI_SAVE] = bx[UI_DEFAULTS] + bw[UI_DEFAULTS] + L(10);
    bx[UI_PLAY] = W - L(MARGIN) - bw[UI_PLAY];
    bx[UI_QUIT] = bx[UI_PLAY] - L(10) - bw[UI_QUIT];
    for (int b = 0; b < UI_BUTTONS; ++b) {
        const bool f = ui.on_buttons && ui.button == b;
        const bool play = b == UI_PLAY;
        if (play) fill(bx[b], by, bw[b], bh, C_ACCENT);
        outline(bx[b], by, bw[b], bh, f ? L(2) : L(1), f ? C_TEXT : play ? C_ACCENT : C_FAINT);
        const float lw = text_width(ui.bold, button_names[b]);
        text(ui.ren, ui.bold, bx[b] + (bw[b] - lw) / 2, by + (bh - ui.bold.height) / 2, button_names[b],
             play ? C_BLACK : f ? C_TEXT : C_SOFT);
        hot(bx[b], by, bw[b], bh, P_BUTTON, -1, b);
    }
    const std::string status = ui.dirty && ui.status.empty() ? "Unsaved changes" : ui.status;
    const float sx = bx[UI_SAVE] + bw[UI_SAVE] + L(20), sw = bx[UI_QUIT] - L(20) - sx;
    if (sw > L(40) && !status.empty())
        text_tail(ui.small, sx, by + (bh - ui.small.height) / 2, sw, status, ui.status_bad ? C_RED : C_DIM);

    if (ui.drop_row >= 0) draw_drop_list();
}

// --- scale ------------------------------------------------------------------------

float display_scale()
{
    int ww = 0, wh = 0, ow = 0, oh = 0;
    SDL_GetWindowSize(ui.win, &ww, &wh);
    SDL_GetCurrentRenderOutputSize(ui.ren, &ow, &oh);
    ui.mouse_scale = ww > 0 ? static_cast<float>(ow) / static_cast<float>(ww) : 1;
    const float s = SDL_GetWindowDisplayScale(ui.win);
    return s <= 0 ? 1 : std::clamp(s, 1.0f, 4.0f);
}

bool make_fonts()
{
    font_free(ui.body);
    font_free(ui.small);
    font_free(ui.bold);
    font_free(ui.title);
    const float s = ui.scale;
    return font_make(ui.body, ui.ren, ui.ttf[0].data(), 19 * s) && font_make(ui.small, ui.ren, ui.ttf[0].data(), 16 * s)
        && font_make(ui.bold, ui.ren, ui.ttf[1].data(), 19 * s) && font_make(ui.title, ui.ren, ui.ttf[1].data(), 30 * s);
}

// --- input ------------------------------------------------------------------------

const Hot* hit(float x, float y)
{
    const SDL_FPoint p = {x * ui.mouse_scale, y * ui.mouse_scale};
    for (auto it = ui.hot.rbegin(); it != ui.hot.rend(); ++it)
        if (SDL_PointInRectFloat(&p, &it->r)) return &*it;
    return nullptr;
}

void slide_to(int i, float mx)
{
    const Row* r = row_at(i);
    for (const auto& h : ui.hot) {
        if (h.part != P_SLIDER || h.row != i) continue;
        const float track_x = h.r.x + L(8), track_w = h.r.w - L(16);
        const float f = std::clamp((mx * ui.mouse_scale - track_x) / track_w, 0.0f, 1.0f);
        int v = r->lo + static_cast<int>(f * static_cast<float>(r->hi - r->lo) + 0.5f);
        v = std::min(r->hi, r->lo + ((v - r->lo + r->step / 2) / r->step) * r->step);
        if (v != setting_get(ui.cfg, r->id)) {
            setting_set(ui.cfg, r->id, v);
            changed();
        }
        return;
    }
}

// True when the launcher is done.
bool click(float mx, float my)
{
    const Hot* h = hit(mx, my);
    if (ui.editing && !(h != nullptr && h->part == P_FIELD && h->row == ui.edit_row)) commit_edit();
    // An open list takes the click: a choice, or anywhere else to close it.
    if (ui.drop_row >= 0) {
        if (h != nullptr && h->part == P_OPTION) {
            ui.drop_sel = h->index;
            drop_pick();
        } else {
            drop_close();
        }
        return false;
    }
    if (h == nullptr) return false;
    const Hot at = *h;  // `hot` is redrawn by what follows
    if (at.row >= 0) {
        ui.focus = at.row;
        ui.on_buttons = false;
    }
    switch (at.part) {
    case P_TAB: set_tab(at.index); break;
    case P_BUTTON:
        ui.on_buttons = true;
        ui.button = at.index;
        return press_button(at.index);
    case P_DROP: drop_open(at.row); break;
    case P_SLIDER: ui.dragging = at.row; slide_to(at.row, mx); break;
    case P_FIELD:
        if (!ui.editing) begin_edit(at.row);
        break;
    case P_BROWSE: {
        std::string path;
        if (browse(path)) set_text(row_at(at.row)->id, path);
        break;
    }
    case P_DETECT: start_capture(at.row); break;
    default: break;
    }
    return false;
}

enum { DIR_NONE, DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

void go(int dir)
{
    if (ui.drop_row >= 0) {
        if (dir == DIR_UP || dir == DIR_DOWN) drop_move(dir == DIR_UP ? -1 : 1);
        return;
    }
    switch (dir) {
    case DIR_UP: move_focus(-1); break;
    case DIR_DOWN: move_focus(1); break;
    case DIR_LEFT: change(-1); break;
    case DIR_RIGHT: change(1); break;
    default: break;
    }
}

void hold(int dir)
{
    ui.held_dir = dir;
    ui.held_next = SDL_GetTicks() + 380;
    go(dir);
}

// True when the launcher is done.
bool handle(const SDL_Event& e)
{
    ui.redraw = true;
    switch (e.type) {
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
    case SDL_EVENT_JOYSTICK_BALL_MOTION:
    case SDL_EVENT_JOYSTICK_HAT_MOTION:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
        ui.redraw = false;
        return false;
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
        // A board's buttons are the launcher's only while one is waited for.
        if (ui.capturing && e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN) {
            capture_button(e.jbutton.which, e.jbutton.button);
            return false;
        }
        ui.redraw = false;
        return false;
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        ui.redraw = !ui.capturing;  // and decided below
        if (ui.capturing) return false;
        break;
    default: break;
    }

    // A button being waited for: a key or a click stops the wait. A
    // controller's own press is the board's, and comes as a joystick's above.
    if (ui.capturing) {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)) {
            ui.capturing = false;
            say(false, "Nothing was pressed on the board; the button is as it was.");
            return false;
        }
        if (e.type != SDL_EVENT_QUIT && e.type != SDL_EVENT_JOYSTICK_ADDED && e.type != SDL_EVENT_JOYSTICK_REMOVED)
            return false;
    }

    switch (e.type) {
    case SDL_EVENT_QUIT: return true;
    case SDL_EVENT_JOYSTICK_ADDED:
        if (SDL_Joystick* joystick = SDL_OpenJoystick(e.jdevice.which)) ui.joysticks.push_back(joystick);
        return false;
    case SDL_EVENT_JOYSTICK_REMOVED:
        if (SDL_Joystick* joystick = SDL_GetJoystickFromID(e.jdevice.which)) {
            std::erase(ui.joysticks, joystick);
            SDL_CloseJoystick(joystick);
        }
        return false;
    case SDL_EVENT_DISPLAY_ADDED:
    case SDL_EVENT_DISPLAY_REMOVED:
    case SDL_EVENT_DISPLAY_MOVED:
    case SDL_EVENT_DISPLAY_ORIENTATION:
        find_monitors();
        return false;
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
        const float s = display_scale();
        if (s != ui.scale) {
            ui.scale = s;
            make_fonts();
        }
        return false;
    }
    case SDL_EVENT_DROP_FILE: {
        const std::string path = e.drop.data != nullptr ? e.drop.data : "";
        const auto ext = path_of_utf8(path).extension().u8string();
        if (same_ignoring_case(std::string(ext.begin(), ext.end()), ".pce")) {
            if (ui.editing) end_edit();
            const auto rel = relative_to_base(path_of_utf8(path));
            set_text(S_ROM, rel);
            say(false, "The ROM is %s", rel.c_str());
        } else {
            say(true, "That is not a .pce file.");
        }
        return false;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (ui.dragging >= 0) {
            const int was = setting_get(ui.cfg, row_at(ui.dragging)->id);
            slide_to(ui.dragging, e.motion.x);
            ui.redraw = setting_get(ui.cfg, row_at(ui.dragging)->id) != was;
            return false;
        }
        const Hot* h = hit(e.motion.x, e.motion.y);
        if (ui.drop_row >= 0) {
            const int was = ui.drop_sel;
            if (h != nullptr && h->part == P_OPTION) ui.drop_sel = h->index;
            ui.redraw = ui.drop_sel != was;
            return false;
        }
        const int hover = h != nullptr && h->row >= 0 ? h->row : -1;
        ui.redraw = hover != ui.hover;
        ui.hover = hover;
        return false;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_LEFT) return click(e.button.x, e.button.y);
        return false;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        ui.dragging = -1;
        return false;
    case SDL_EVENT_MOUSE_WHEEL:
        if (ui.drop_row >= 0) {
            ui.drop_first = std::max(0, ui.drop_first - static_cast<int>(e.wheel.y * 3));  // `draw_drop_list` keeps it in range
            return false;
        }
        ui.scroll -= e.wheel.y * L(ROW_H) * 1.5f;
        clamp_scroll();
        return false;
    case SDL_EVENT_TEXT_INPUT:
        if (ui.editing) edit_insert(e.text.text);
        return false;
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(e.gdevice.which);
        return false;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (SDL_Gamepad* pad = SDL_GetGamepadFromID(e.gdevice.which)) SDL_CloseGamepad(pad);
        return false;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (SDL_GetTicks() < ui.pad_quiet_until) return false;
        if (ui.editing) commit_edit();
        switch (e.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: hold(DIR_UP); break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: hold(DIR_DOWN); break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: hold(DIR_LEFT); break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: hold(DIR_RIGHT); break;
        case SDL_GAMEPAD_BUTTON_SOUTH: return activate(true);
        case SDL_GAMEPAD_BUTTON_EAST: drop_close(); break;
        case SDL_GAMEPAD_BUTTON_WEST:
            if (!ui.on_buttons) remove_binding(ui.focus);
            break;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: set_tab(ui.tab - 1); break;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: set_tab(ui.tab + 1); break;
        case SDL_GAMEPAD_BUTTON_START: return launch();
        default: break;
        }
        return false;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (e.gbutton.button >= SDL_GAMEPAD_BUTTON_DPAD_UP && e.gbutton.button <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
            ui.held_dir = DIR_NONE;
        return false;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        // The left stick as a D-pad, with the same repeat.
        ui.redraw = false;
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) ui.stick_x = e.gaxis.value;
        else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) ui.stick_y = e.gaxis.value;
        else return false;
        const int ax = std::abs(ui.stick_x), ay = std::abs(ui.stick_y);
        int dir = DIR_NONE;
        if (ax > 20000 || ay > 20000)
            dir = ay >= ax ? (ui.stick_y < 0 ? DIR_UP : DIR_DOWN) : (ui.stick_x < 0 ? DIR_LEFT : DIR_RIGHT);
        else if (ax > 12000 || ay > 12000)
            dir = ui.held_dir;  // between the two thresholds: as it was
        if (dir != ui.held_dir) {
            ui.redraw = true;
            if (dir != DIR_NONE) hold(dir);
            else ui.held_dir = DIR_NONE;
        }
        return false;
    }
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Keycode k = e.key.key;
        const bool ctrl = (e.key.mod & SDL_KMOD_CTRL) != 0;
        const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
        if (ui.editing) {
            switch (k) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_TAB: commit_edit(); break;
            case SDLK_ESCAPE: end_edit(); break;
            case SDLK_LEFT: ui.caret = previous_boundary(ui.edit, ui.caret); break;
            case SDLK_RIGHT: ui.caret = next_boundary(ui.edit, ui.caret); break;
            case SDLK_HOME: ui.caret = 0; break;
            case SDLK_END: ui.caret = ui.edit.size(); break;
            case SDLK_BACKSPACE:
                if (ui.caret > 0) {
                    const std::size_t from = previous_boundary(ui.edit, ui.caret);
                    ui.edit.erase(from, ui.caret - from);
                    ui.caret = from;
                }
                break;
            case SDLK_DELETE:
                if (ui.caret < ui.edit.size()) ui.edit.erase(ui.caret, next_boundary(ui.edit, ui.caret) - ui.caret);
                break;
            case SDLK_V:
                if (ctrl && SDL_HasClipboardText()) {
                    char* clip = SDL_GetClipboardText();
                    std::string pasted = clip != nullptr ? clip : "";
                    SDL_free(clip);
                    pasted = pasted.substr(0, pasted.find_first_of("\r\n"));
                    edit_insert(pasted);
                }
                break;
            case SDLK_A:
                if (ctrl) { ui.edit.clear(); ui.caret = 0; }
                break;
            default: break;
            }
            return false;
        }
        if (ui.drop_row >= 0) {
            switch (k) {
            case SDLK_UP: drop_move(-1); break;
            case SDLK_DOWN: drop_move(1); break;
            case SDLK_PAGEUP: drop_move(-(kDropShown - 1)); break;
            case SDLK_PAGEDOWN: drop_move(kDropShown - 1); break;
            case SDLK_HOME: drop_move(-1000); break;
            case SDLK_END: drop_move(1000); break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE: drop_pick(); break;
            case SDLK_ESCAPE:
            case SDLK_TAB: drop_close(); break;
            default: break;
            }
            return false;
        }
        if (ctrl && k == SDLK_S) { save(); return false; }
        if ((ctrl && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) || k == SDLK_F5) return launch();
        switch (k) {
        case SDLK_UP: go(DIR_UP); break;
        case SDLK_DOWN: go(DIR_DOWN); break;
        case SDLK_LEFT: go(DIR_LEFT); break;
        case SDLK_RIGHT: go(DIR_RIGHT); break;
        case SDLK_TAB: set_tab(ui.tab + (shift ? -1 : 1)); break;
        case SDLK_PAGEUP: ui.scroll -= list_h() * 0.8f; clamp_scroll(); break;
        case SDLK_PAGEDOWN: ui.scroll += list_h() * 0.8f; clamp_scroll(); break;
        case SDLK_HOME: ui.on_buttons = false; ui.focus = first_row(); ui.scroll = 0; break;
        case SDLK_END: ui.on_buttons = true; break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE: return activate(false);
        case SDLK_BACKSPACE:
        case SDLK_DELETE:
            if (!ui.on_buttons) remove_binding(ui.focus);
            break;
        case SDLK_ESCAPE: return press_button(UI_QUIT);
        default: break;
        }
        return false;
    }
    default: return false;
    }
}

// Keys as if pressed: `Tab,Down,Ctrl+S`. True when the launcher is done.
bool press_keys(const std::string& list)
{
    std::size_t at = 0;
    while (at <= list.size()) {
        const std::size_t comma = list.find(',', at);
        std::string tok = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
        at = comma == std::string::npos ? list.size() + 1 : comma + 1;
        if (tok.empty()) continue;
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        for (;;) {
            if (SDL_strncasecmp(tok.c_str(), "Ctrl+", 5) == 0) { e.key.mod |= SDL_KMOD_LCTRL; tok.erase(0, 5); }
            else if (SDL_strncasecmp(tok.c_str(), "Shift+", 6) == 0) { e.key.mod |= SDL_KMOD_LSHIFT; tok.erase(0, 6); }
            else break;
        }
        e.key.key = SDL_GetKeyFromName(tok.c_str());
        e.key.scancode = SDL_GetScancodeFromName(tok.c_str());
        if (e.key.key == SDLK_UNKNOWN) { std::fprintf(stderr, "error: no key '%s'\n", tok.c_str()); return true; }
        if (handle(e)) return true;
        draw();  // where things are now, which the next key may read back
    }
    return false;
}

// The launcher as it stands, drawn once into a PNG.
bool screenshot(const char* path)
{
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(ui.ren, &w, &h);
    SDL_Texture* t = SDL_CreateTexture(ui.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    bool ok = t != nullptr && SDL_SetRenderTarget(ui.ren, t);
    if (ok) {
        draw();
        SDL_Surface* s = SDL_RenderReadPixels(ui.ren, nullptr);
        ok = s != nullptr && SDL_SavePNG(s, path);
        SDL_DestroySurface(s);
        SDL_SetRenderTarget(ui.ren, nullptr);
    }
    if (t != nullptr) SDL_DestroyTexture(t);
    if (!ok) std::fprintf(stderr, "error: cannot write '%s': %s\n", path, SDL_GetError());
    return ok;
}

} // namespace

int main(int argc, char* argv[])
{
    std::string keys;
    const char* shot = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--press") == 0 && i + 1 < argc) keys = argv[++i];
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) shot = argv[++i];
        else {
            std::fprintf(stderr, "usage: %s [--press <keys>] [--screenshot <file.png>]\n", argv[0]);
            return 2;
        }
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "error: %s\n", SDL_GetError());
        return 1;
    }
    find_base();
    build_rows();
    load();
    ui.focus = first_row();

    ui.ttf[0] = load_font(false);
    ui.ttf[1] = load_font(true);
    if (ui.ttf[0].empty()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, "The launcher could not find a font to draw with.", nullptr);
        return 1;
    }

    // Sized for the display it opens on: the layout at that display's scale,
    // or as much of the display as there is.
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    float s = SDL_GetDisplayContentScale(display);
    s = s <= 0 ? 1 : std::clamp(s, 1.0f, 4.0f);
    SDL_Rect usable = {0, 0, 1280, 720};
    SDL_GetDisplayUsableBounds(display, &usable);
    int ww = static_cast<int>(980 * s), wh = static_cast<int>(780 * s);
    ww = std::min(ww, usable.w * 9 / 10);
    wh = std::min(wh, usable.h * 9 / 10);
    ui.win = SDL_CreateWindow(kTitle, ww, wh, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (ui.win == nullptr) { std::fprintf(stderr, "error: %s\n", SDL_GetError()); return 1; }
    // The sizes above are pixels; the window's own units may not be.
    const float density = SDL_GetWindowPixelDensity(ui.win);
    if (density > 1) {
        ww = static_cast<int>(static_cast<float>(ww) / density);
        wh = static_cast<int>(static_cast<float>(wh) / density);
        SDL_SetWindowSize(ui.win, ww, wh);
    }
    SDL_SetWindowPosition(ui.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetWindowMinimumSize(ui.win, std::min(static_cast<int>(640 * s / std::max(density, 1.0f)), usable.w),
                             std::min(static_cast<int>(480 * s / std::max(density, 1.0f)), usable.h));
    // Direct3D 9, which is what SDL2 picks first and what the ZAMN launcher
    // draws with. SDL3 would pick Direct3D 11, and a graphics driver's game
    // overlay (NVIDIA's, for one) takes a Direct3D 11 window for a game:
    // it announces itself when the launcher opens, and hooking the window
    // made the mouse cursor lag while the launcher was up. A settings screen
    // needs none of what 11 has over 9. Anything SDL has, if 9 is missing.
    ui.ren = SDL_CreateRenderer(ui.win, "direct3d");
    if (ui.ren == nullptr) ui.ren = SDL_CreateRenderer(ui.win, nullptr);
    if (ui.ren == nullptr) { std::fprintf(stderr, "error: %s\n", SDL_GetError()); return 1; }
    SDL_SetRenderVSync(ui.ren, 1);
    SDL_SetRenderDrawBlendMode(ui.ren, SDL_BLENDMODE_BLEND);
    ui.scale = display_scale();
    if (!make_fonts()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, "The launcher could not make its font.", ui.win);
        return 1;
    }
    SDL_StopTextInput(ui.win);
    draw();  // where things are, which a click or a slide reads back
    bool done = !keys.empty() && press_keys(keys);
    if (shot != nullptr) {
        const bool ok = screenshot(shot);
        SDL_Quit();
        return ok ? 0 : 1;
    }
    SDL_RenderPresent(ui.ren);
    if (ui.game == nullptr) SDL_ShowWindow(ui.win);

    while (!done) {
        SDL_Event e;
        if (ui.game != nullptr) {
            // What comes while the game runs is dropped, so that a button
            // pressed in the game is not one pressed here.
            if (SDL_WaitEventTimeout(&e, 250)) {
                do done = done || e.type == SDL_EVENT_QUIT; while (SDL_PollEvent(&e));
            }
            DWORD code = 0;
            if (!done && game_closed(code)) {
                // The game writes the file too, on its way out: where the
                // windows were, and what F7 and R were left at. Read again,
                // so that the next Play does not put back what it replaced.
                // Nothing here has changed since Play saved.
                load();
                if (code != 0) say(true, "The game stopped with exit code %lu.", static_cast<unsigned long>(code));
                else say(false, "The game has closed.");
                draw();
                SDL_RenderPresent(ui.ren);
                SDL_ShowWindow(ui.win);
                SDL_RaiseWindow(ui.win);
            }
            continue;
        }
        // Asleep until something happens, but awake for a countdown or a held
        // direction, which happen without an event.
        const int wait = ui.held_dir != DIR_NONE ? 16 : ui.capturing ? 100 : 1000;
        bool redraw = false;
        if (SDL_WaitEventTimeout(&e, wait)) {
            done = handle(e);
            redraw = ui.redraw;
            while (!done && ui.game == nullptr && SDL_PollEvent(&e)) {
                done = handle(e);
                redraw = redraw || ui.redraw;
            }
        }
        // The countdown of a key being waited for, and a held direction's
        // repeat, change what is shown without an event.
        const Uint64 now = SDL_GetTicks();
        redraw = redraw || ui.capturing;
        if (ui.capturing && now >= ui.capture_end) ui.capturing = false;
        if (ui.held_dir != DIR_NONE && now >= ui.held_next) {
            go(ui.held_dir);
            ui.held_next = now + 70;
            redraw = true;
        }
        if (!done && redraw) {
            draw();
            SDL_RenderPresent(ui.ren);
        }
    }

    font_free(ui.body);
    font_free(ui.small);
    font_free(ui.bold);
    font_free(ui.title);
    if (ui.logo != nullptr) SDL_DestroyTexture(ui.logo);
    for (SDL_Joystick* joystick : ui.joysticks) SDL_CloseJoystick(joystick);
    SDL_DestroyRenderer(ui.ren);
    SDL_DestroyWindow(ui.win);
    SDL_Quit();
    return 0;
}
