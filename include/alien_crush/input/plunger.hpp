// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace alien_crush::input {

// Hardware-independent normalized pull and release detection, reused from Devil's Crush.
// Alien Crush launcher charge mapping is a separate, game-specific adapter.
struct PlungerCalibration {
    // The axis value with the rod at rest and with it drawn fully back, in
    // whatever units the host's controller reports. Two endpoints rather than
    // a range and an inversion flag: a plunger whose axis runs backwards is
    // one whose `drawn` is the smaller of the two, and needs no separate
    // setting to say so, and getting it the wrong way round is not subtle:
    // the on-screen shooter answers the rod being pushed in rather than pulled
    // out, and only the inch or two of push travel is reachable, so every shot
    // comes out at the bottom of the range.
    //
    // These inherited endpoints are provisional. Use the host's --input-probe
    // to measure the actual board's axis, rest and full-pull values before
    // configuring a game-specific launcher adapter.
    std::int32_t rest = 0;
    std::int32_t drawn = 32767;

    // How much of the travel nearest the rest position still reads as rest, in
    // percent. A plunger sensor is never quite still, and a rod that reads one
    // percent drawn would hold button I forever and never let the ball go.
    std::uint8_t deadzone = 8;

    // The one-frame fall, in percent of travel, that counts as the rod being
    // let go. 25 sits between the two motions it has to tell apart: a spring
    // return covers the whole travel in three or four frames, and a hand
    // easing the rod forward covers a few percent in each.
    std::uint8_t release = 25;

    [[nodiscard]] bool operator==(const PlungerCalibration&) const = default;
};

// A sample carries normalized draw (0..100) and a one-sample release edge.
struct PlungerSample {
    std::uint8_t draw = 0;
    // The shooter should be put at `draw` this frame.
    bool drawn = false;
    // Release retains the preceding pull distance, before the spring returns.
    bool released = false;

    [[nodiscard]] bool operator==(const PlungerSample&) const = default;
};

// Which controller the plunger is, and how it is read. Held as a name and an
// axis index rather than an SDL instance ID because it is written down in
// configuration and has to survive the controller being unplugged, renumbered
// by Windows, or plugged into a different socket.
struct PlungerBinding {
    // Matched against the controller's name as a case-insensitive substring.
    // Empty matches the first controller with the axis, which is the ordinary
    // case on a cabinet where the plunger board is the only thing plugged in.
    std::string device;

    // The axis to read, or a negative number for no plunger, which is the
    // default.
    //
    // Off rather than on, and that is a measurement rather than caution. A
    // plunger is indistinguishable from a stick: a centered plunger axis can
    // look exactly like a thumbstick pushed one way. Axis 2 on an ordinary gamepad is
    // the right stick, so a plunger that defaulted to on would let a stick
    // charge the shooter on any desktop with a pad plugged in. Nothing can
    // tell the two apart from here, so somebody has to say -- which costs one
    // run of `--input-probe`, and that has to be run anyway to learn
    // the two endpoints below.
    std::int32_t axis = -1;

    PlungerCalibration calibration;

    [[nodiscard]] bool enabled() const noexcept
    {
        return axis >= 0;
    }

    [[nodiscard]] bool operator==(const PlungerBinding&) const = default;
};

class Plunger {
public:
    void configure(const PlungerCalibration& calibration) noexcept;

    [[nodiscard]] const PlungerCalibration& calibration() const noexcept
    {
        return calibration_;
    }

    // One frame's axis reading. The value is the host's raw axis, unscaled and
    // unclamped: a rod pushed *past* its rest position -- which every spring
    // can do on the way back -- reads
    // as no draw rather than as a negative one.
    [[nodiscard]] PlungerSample update(std::int32_t raw) noexcept;

    // Forget the rod's position without forgetting the calibration.
    //
    // Called when the controller goes away and when the ball is served, and
    // for the same reason in both: a plunger that is already drawn when the
    // host starts watching it must not count as a pull. After this the rod has
    // to be seen at rest once before it can be drawn again, so a controller
    // whose axis happens to sit at full scale -- the wrong device, or one that
    // reports nothing -- can never launch anything.
    void rearm() noexcept;

    // Whether the rod has been seen at rest since the last `rearm`.
    [[nodiscard]] bool armed() const noexcept
    {
        return armed_;
    }

    // The draw the last `update` resolved, before any release test. Reported
    // for the probe, which exists to be watched while somebody moves the rod.
    [[nodiscard]] std::uint8_t draw() const noexcept
    {
        return draw_;
    }

private:
    [[nodiscard]] std::uint8_t resolve_draw(
        std::int32_t raw) const noexcept;

    PlungerCalibration calibration_{};
    std::uint8_t draw_ = 0;
    bool drawn_ = false;
    bool armed_ = false;
};

} // namespace alien_crush::input
