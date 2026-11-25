// SPDX-License-Identifier: GPL-3.0-or-later
//
// The rac1 part of the mod: Going Commando's movement through rac1's hook points (DESIGN.md). The
// decisions live in src/rac1/ground.h, air.h, strafe.h, wrench.h, camera.h and clank.h, away from
// game memory; this file reads the hero into them and applies what they return. Addresses are in
// discs.h and offsets in hero.h.
//
// Where each hook acts in a tick (DESIGN.md section 1):
//   pad poll          process_report: undoes last tick's pad and camera steers, and takes L2/R2
//                     for the strafe
//   animation         advance_animation: undoes last tick's hero steers, clamps idle after the
//                     run's exit, and plays the backpedal (the walk or run backwards)
//   hero_update       update_motion_input: before the update, sets the tick's context, the jump's
//                     air speed, the run's flags and the flip's drift. Inside it, turn_to_heading,
//                     approach_speed and set_planar_velocity give rac2's numbers, including the
//                     wrench's lock-on turn and the slam's drive. After it, sets the twist and the
//                     steers rac1's transitions will read (the exit, the jump, the flip, the start,
//                     the wrench's swings and Square)
//   transitions       rac1's own, the loaded level's copy
//   follow camera     apply_overrides: the strafe camera, and the turn speed (its gain) for the
//                     rest of the camera's update
//
// Clank alone (vehicle mode 1) gets a tick of his own under the `clank` setting (clank_update): his
// jump's entry values before his update; his run's turn, speeds and velocity and his punch's
// lock-on turn inside it; and after it the steers for his transitions (a remembered Square, the
// punch's way out).
#include "rac1/game.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "openrac/games/rac1.hpp"
#include "rac1/air.h"
#include "rac1/camera.h"
#include "rac1/clank.h"
#include "rac1/discs.h"
#include "rac1/ground.h"
#include "rac1/hero.h"
#include "rac1/strafe.h"
#include "rac1/wrench.h"

namespace enhanced_movement::rac1 {

namespace {

using openrac::gaddr;
namespace hp = openrac::rac1;

// The mod's settings (openrac-mod.json), read once at load.
struct Settings {
    bool ground = true;
    bool air = true;
    std::uint32_t strafe = BM_PAD_L2 | BM_PAD_R2;  // strafe buttons, 0 when off
    bool flips = true;
    bool twist = true;
    bool wrench = true;
    int camera = BM_CAMERA_ENHANCED;  // a BM_CAMERA_* (camera.h)
    int clank = BM_CLANK_ENHANCED;    // a BM_CLANK_* (clank.h)
    bool trace = false;
};

// A word of game memory changed for one tick (DESIGN.md section 1): a hero value rac1's transitions
// read, or the follow camera's gain. `always` puts it back even after a state change, for the
// animation's frame and blend flag, which the next animation advance recomputes from its own last
// values.
struct Steered {
    gaddr address = 0;
    std::uint32_t old_bits = 0;
    std::uint32_t wrote_bits = 0;
    bool always = false;
};

// A slot of the pad's press history changed for one tick, with the bits added and removed.
struct PadEdit {
    int slot = 0;
    std::uint32_t added = 0;
    std::uint32_t removed = 0;
};

// The state the hooks keep between calls. load resets it, since the load test loads the mod several
// times.
struct State {
    bool in_update = false;    // inside hero_update, where the inner hooks act
    bool active = false;       // this tick is Ratchet's, in play and on foot
    bool clank = false;        // inside Clank's update (clank_update)
    bool strafing = false;     // strafing this tick
    bool strafe_held = false;  // a strafe button is held, read from the pad
    int sector = BM_FORWARD;
    float strafe_rate = 0.0f;  // the strafe's yaw rate (rac2's +0x145C)
    float strafe_yaw = 0.0f;   // the stick's yaw while strafing (rac2's +0x1458)
    bm_sides sides{};
    bm_run_flags flags{};
    float k = 1.0f;             // the run's acceleration factor, set by the turn
    bool flip_steered = false;  // a flip was steered for the last transitions
    bool flipping = false;      // in a flip the strafe started
    int last_state = -1;
    bm_tick tick{};
    // Hero values steered for the transitions; the next tick's first hook puts them back.
    Steered hero_steers[4];
    int hero_steer_count = 0;
    int steered_state = -1;
    bool idle_clamp = false;
    std::uint32_t pad_r1 = 0;  // R1 the flip added to pad_held
    PadEdit pad_edits[64];     // press history slots changed for the transitions
    int pad_edit_count = 0;
    // Follow camera words changed for its update and put back before the next tick, and the level
    // whose program holds them.
    Steered cam_steers[8];
    int cam_steer_count = 0;
    int cam_steer_level = -1;
    std::string decided;  // what the mod did this tick, for the trace
};

// The host, the disc's row in discs.h, the settings, the hooks' state and the numbers for the
// `clank` setting. load sets them all.
openrac::Mod* s_mod = nullptr;
const bm_disc* s_disc = nullptr;
Settings s_set;
State s;
bm_clank_params s_clank = bm_clank_params_for(BM_CLANK_ENHANCED);

// The game functions the strafe camera and the backpedal call (discs.h), found at load.
using OrbitClampFn = void (*)(int32_t);
using SlideFn = void (*)();
using SpringFn = void (*)(float, float);
using PlaySoundFn = int32_t (*)(int32_t, int32_t, std::uint32_t);
OrbitClampFn s_orbit_clamp = nullptr;
SlideFn s_start_slide = nullptr;
SpringFn s_lateral_spring = nullptr;
PlaySoundFn s_play_sound = nullptr;

// ---- Game memory ----

// Reads a value from game memory.
template <class T>
T get(gaddr address) {
    return s_mod->memory().read<T>(address);
}

// Writes a value into game memory.
template <class T>
void put(gaddr address, T value) {
    s_mod->memory().write<T>(address, value);
}

// Returns whether a pointer read from game memory can be followed: not null and inside the EE's
// 32 MB.
bool valid(gaddr address) {
    return address != 0 && address < 0x02000000u;
}

// Returns the address of a field of the hero.
gaddr hero(std::uint32_t offset) {
    return s_disc->hero + offset;
}

// Returns the address of a field of the first pad's state.
gaddr pad(std::uint32_t offset) {
    return s_disc->pad + offset;
}

// Returns the address of a level 0 global in the loaded level. Each level loads its program
// elsewhere, so it is looked up every time.
gaddr level0(std::uint32_t address) {
    return s_mod->relocate_level(0, address);
}

// Returns the tick's units: g_frame_time, its square, g_frame_scale_sq and frame_scale. Falls back
// to NTSC's when a value read is out of range.
bm_tick read_tick() {
    bm_tick tick;
    tick.ft = get<float>(s_mod->relocate(s_disc->frame_time));
    tick.ft2 = get<float>(s_mod->relocate(s_disc->frame_time_sq));
    tick.fs2 = get<float>(s_mod->relocate(s_disc->frame_scale_sq));
    tick.fs = get<float>(s_mod->relocate(s_disc->frame_scale));

    // The game runs at 1/100 to 1/25 of a second a tick. Anything else means it has not set its
    // numbers yet.
    if (!(tick.ft > 0.01f && tick.ft < 0.04f) || !(tick.ft2 > 0.0f) || !(tick.fs2 > 0.0f)
        || !(tick.fs > 0.5f && tick.fs < 2.0f)) {
        return bm_tick_ntsc();
    }

    return tick;
}

// Returns the hero's state, a RAC1_STATE_* (rac1.h).
int state() {
    return get<int32_t>(hero(BM_HERO_STATE));
}

// Returns the yaw of the camera in use, which the stick is read against.
float camera_yaw() {
    return get<float>(level0(s_disc->camera_yaw));
}

// Returns whether Ratchet is in play on foot: a level is loaded, his moby exists, no scene or movie
// runs, and he is not Clank or in a vehicle.
bool playing() {
    // There is no hero before the first level or between levels.
    if (s_mod->loaded_level() < 0 || get<std::uint32_t>(hero(BM_HERO_MOBY)) == 0) {
        return false;
    }

    return get<int32_t>(level0(s_disc->cinematic_mode)) == 0
           && get<std::uint8_t>(hero(BM_HERO_VEHICLE_MODE)) == 0;
}

// Returns whether a control mode is one on foot, where the strafe takes L2/R2 (rac1.h).
bool foot_mode(int mode) {
    // These are the modes for standing, running, falling, jumping, landing, attacking, waiting on
    // an item and crouching.
    switch (mode) {
        case RAC1_MODE_IDLE:
        case RAC1_MODE_RUN:
        case RAC1_MODE_FALL:
        case RAC1_MODE_JUMP:
        case RAC1_MODE_LAND:
        case RAC1_MODE_ATTACK:
        case RAC1_MODE_ITEM_RELEASE_WAIT:
        case RAC1_MODE_CROUCH:
            return true;
        default:
            return false;
    }
}

// Returns whether the strafe takes L2/R2 from the pad now: the setting is on, and Ratchet is in
// play in a mode on foot, outside rac1's own strafe-like state 0x81.
bool strafe_takes_pad() {
    return s_set.strafe != 0 && playing() && foot_mode(get<int32_t>(hero(BM_HERO_CONTROL_MODE)))
           && state() != RAC1_STATE_OWN_STRAFE;
}

// Returns whether he strafes this tick: a strafe button is held and the pad's buttons are the
// strafe's, the follow camera is in use, the gravity allows it (bm_strafe_gravity_ok), and he is not
// in the long jump (rac2 drops its strafe there).
bool strafe_now() {
    // No strafe button is held, or the game has the buttons now.
    if (!s.strafe_held || !strafe_takes_pad()) {
        return false;
    }

    const gaddr cam = get<std::uint32_t>(level0(s_disc->follow_cam));
    return valid(cam) && get<int16_t>(cam + BM_CAMERA_KIND) == 0
           && bm_strafe_gravity_ok(get<std::uint8_t>(hero(BM_HERO_GRAVITY_MODE)), state())
           && state() != RAC1_STATE_PACK_LONG_JUMP;
}

// Returns whether a state is the run, on dry ground or wading.
bool is_run(int st) {
    return st == RAC1_STATE_RUN || st == RAC1_STATE_RUN_WADING;
}

// Returns how many polls before the newest one `mask` was pressed (0 for the newest), looking back
// `polls` polls as pad_pressed_within does, or -1 when it was not. `slot`, if given, gets the
// press's history slot.
int pressed_within(std::uint32_t mask, int polls, int* slot) {
    const int count = get<int32_t>(pad(BM_PAD_HIST_COUNT));
    const int next = get<int16_t>(pad(BM_PAD_HIST_IDX));
    const int limit = count < polls ? count : polls;

    // The search runs from the newest poll back.
    for (int i = 1; i <= limit; ++i) {
        const int entry = ((next - i) % BM_PAD_HIST_LEN + BM_PAD_HIST_LEN) % BM_PAD_HIST_LEN;

        // The newest poll with the press gives the answer.
        if (get<std::uint32_t>(pad(BM_PAD_HIST_BTN + 4 * entry)) & mask) {
            // A caller that marks the press needs its slot.
            if (slot != nullptr) {
                *slot = entry;
            }

            return i - 1;
        }
    }

    return -1;
}

// Returns the stick's angle `back` polls ago, from the pad's angle history.
float stick_angle_back(int back) {
    const int next = get<int16_t>(pad(BM_PAD_HIST_IDX));
    const int entry = ((next - back) % BM_PAD_HIST_LEN + BM_PAD_HIST_LEN) % BM_PAD_HIST_LEN;
    return get<float>(pad(BM_PAD_HIST_ANG + 4 * entry));
}

// Returns whether a hold-to-fire weapon (items 15, 16 and 21: the Blaster, Pyrocitor and
// Morph-o-Ray) is in hand with its animation on, when rac1 slows the turn
// (turn_hero_toward_move_direction_with_stick_scale).
bool firing() {
    // Only an item in hand with its animation running counts.
    if (get<int32_t>(hero(BM_HERO_ITEM_STATE)) != 2
        || get<std::uint8_t>(hero(BM_HERO_ITEM_ANIM)) == 0) {
        return false;
    }

    const int id = get<int32_t>(hero(BM_HERO_ITEM_ID));
    return id == 15 || id == 16 || id == 21;
}

// Returns the run's inputs for ground.h, with the yaw error as it is now.
bm_run_in run_in() {
    bm_run_in in{};
    in.stick = get<float>(hero(BM_HERO_STICK));
    in.yaw_error = get<float>(hero(BM_HERO_YAW_ERROR));
    in.flat_speed = get<float>(hero(BM_HERO_FLAT_SPEED));
    in.step = get<int32_t>(hero(BM_HERO_STEP));
    in.wading = state() == RAC1_STATE_RUN_WADING;
    in.item_flag = get<std::uint8_t>(hero(BM_HERO_ITEM_FLAG)) != 0;
    in.firing = firing();
    in.stick_swing = bm_diff(stick_angle_back(1), stick_angle_back(7));
    in.strafing = s.strafing;
    return in;
}

// ---- Steering the transitions ----

// Changes a hero value for the transitions about to run; undo_hero_steers puts it back. `always`
// puts it back even after a state change.
template <class T>
void steer(gaddr address, T value, bool always = false) {
    static_assert(sizeof(T) == 4, "a steered value is a word");

    // A tick needs at most four steers, the size of the table.
    if (s.hero_steer_count >= 4) {
        return;
    }

    Steered& steered = s.hero_steers[s.hero_steer_count++];
    steered.address = address;
    steered.old_bits = get<std::uint32_t>(address);
    put<T>(address, value);
    steered.wrote_bits = get<std::uint32_t>(address);
    steered.always = always;
}

// Puts back the hero values steered for the last transitions. A value goes back only if the state
// did not change (or it is marked `always`) and it still holds what the mod wrote; otherwise the
// game has moved on from it.
void undo_hero_steers() {
    const bool same = state() == s.steered_state;

    // Newest first, so a value steered twice gets its first old value back.
    for (int i = s.hero_steer_count - 1; i >= 0; --i) {
        const Steered& steered = s.hero_steers[i];

        // Only a value the game left alone goes back, and only in the same state unless `always`.
        if ((same || steered.always) && get<std::uint32_t>(steered.address) == steered.wrote_bits) {
            put<std::uint32_t>(steered.address, steered.old_bits);
        }
    }

    s.hero_steer_count = 0;
}

// Takes back the pad steers before the next poll: R1 in pad_held, and the presses added to or
// hidden from the press history.
void undo_pad_steers() {
    // R1 comes out only if the mod put it in.
    if (s.pad_r1 != 0) {
        put<std::uint32_t>(pad(BM_PAD_HELD), get<std::uint32_t>(pad(BM_PAD_HELD)) & ~s.pad_r1);
        put<std::uint32_t>(
            pad(BM_PAD_PREV_HELD), get<std::uint32_t>(pad(BM_PAD_PREV_HELD)) & ~s.pad_r1
        );
        s.pad_r1 = 0;
    }

    // History edits are undone newest first, so a slot edited twice ends as it was.
    for (int i = s.pad_edit_count - 1; i >= 0; --i) {
        const PadEdit& edit = s.pad_edits[i];
        const gaddr slot = pad(BM_PAD_HIST_BTN + 4 * edit.slot);
        put<std::uint32_t>(slot, (get<std::uint32_t>(slot) & ~edit.added) | edit.removed);
    }

    s.pad_edit_count = 0;
}

// Adds `bits` to a slot of the press history for the transitions about to run.
void pad_add(int slot, std::uint32_t bits) {
    const gaddr address = pad(BM_PAD_HIST_BTN + 4 * slot);
    const std::uint32_t added = bits & ~get<std::uint32_t>(address);

    // Bits already there need no edit, and an edit needs room in the table to be undone.
    if (added == 0 || s.pad_edit_count >= 64) {
        return;
    }

    put<std::uint32_t>(address, get<std::uint32_t>(address) | added);
    s.pad_edits[s.pad_edit_count++] = PadEdit{slot, added, 0};
}

// Hides the presses of `mask` in the newest `polls` polls of the history from the transitions about
// to run.
void pad_hide(std::uint32_t mask, int polls) {
    const int count = get<int32_t>(pad(BM_PAD_HIST_COUNT));
    const int next = get<int16_t>(pad(BM_PAD_HIST_IDX));
    const int limit = count < polls ? count : polls;

    // The loop runs from the newest poll back, while the edit table has room.
    for (int i = 1; i <= limit && s.pad_edit_count < 64; ++i) {
        const int entry = ((next - i) % BM_PAD_HIST_LEN + BM_PAD_HIST_LEN) % BM_PAD_HIST_LEN;
        const gaddr address = pad(BM_PAD_HIST_BTN + 4 * entry);
        const std::uint32_t removed = get<std::uint32_t>(address) & mask;

        // Only a poll with the press needs an edit.
        if (removed != 0) {
            put<std::uint32_t>(address, get<std::uint32_t>(address) & ~removed);
            s.pad_edits[s.pad_edit_count++] = PadEdit{entry, 0, removed};
        }
    }
}

// Returns how many polls back `mask` was last pressed, 1 for the newest, or 0 when the history has
// none.
int press_age(std::uint32_t mask) {
    return pressed_within(mask, BM_PAD_HIST_LEN, nullptr) + 1;
}

// ---- The wrench ----

// Returns whether the wrench is in hand and ready, when rac1's item code takes Square for it.
bool wrench_in_hand() {
    return get<int32_t>(hero(BM_HERO_ITEM_ID)) == RAC1_ITEM_WRENCH
           && get<int32_t>(hero(BM_HERO_ITEM_STATE)) == 2
           && get<std::uint32_t>(hero(BM_HERO_ITEM_MOBY)) != 0;
}

// Returns what the transitions read of a swing, for wrench.h.
bm_swing_in swing_in(int st) {
    bm_swing_in in{};
    in.state = st;
    in.record = get<int32_t>(hero(BM_HERO_ATTACK_RECORD));
    in.frame = get<float>(hero(BM_HERO_ANIM_FRAME));
    in.changing = get<int32_t>(hero(BM_HERO_ANIM_CHANGING)) != 0;
    in.done = (get<int32_t>(hero(BM_HERO_ANIM_FLAGS)) & 2) != 0;
    in.stick = get<float>(hero(BM_HERO_STICK));
    in.timer = get<int32_t>(hero(BM_HERO_STATE_TIMER));
    in.square = press_age(BM_PAD_SQUARE);
    in.cross = press_age(BM_PAD_CROSS);
    in.crouch = (get<std::uint32_t>(pad(BM_PAD_HELD)) & (BM_PAD_R1 | BM_PAD_R2)) != 0;
    in.unk308 = get<int16_t>(hero(BM_HERO_UNK308));
    in.slip = get<std::uint8_t>(hero(BM_HERO_ON_SLIP)) != 0;
    in.out = get<std::uint8_t>(hero(BM_HERO_ITEM_THROWN)) != 0;
    return in;
}

// Steers this tick's transitions to Going Commando's wrench, after hero_update (DESIGN.md section
// 5). The swings and the throw end, chain and jump out as rac2's do, Square counts for as long as
// rac2 counts it, and Square starts nothing while the wrench is out. Returns whether rac1's item
// code takes a Square this tick on foot; the flip must then not add R1, which would make a throw.
bool steer_wrench(int st) {
    // Nothing changes unless the wrench is in hand and ready.
    if (!wrench_in_hand()) {
        return false;
    }

    const int age = press_age(BM_PAD_SQUARE);
    bm_reach_in reach{};
    reach.state = st;
    reach.control_mode = get<int32_t>(hero(BM_HERO_CONTROL_MODE));
    reach.timer = get<int32_t>(hero(BM_HERO_STATE_TIMER));
    reach.rail_land = get<int32_t>(hero(BM_HERO_RAIL_LAND));
    reach.unk450 = get<int32_t>(hero(BM_HERO_UNK450));
    reach.frame = get<float>(hero(BM_HERO_ANIM_FRAME));
    reach.slip = get<std::uint8_t>(hero(BM_HERO_ON_SLIP)) != 0;
    const bm_windows windows = bm_wrench_windows(&reach, &s.tick);
    // Control modes 0 and 1 are standing and running.
    const bool on_foot = reach.control_mode < 2;

    // With the wrench setting off, rac1's windows decide.
    if (!s_set.wrench) {
        return on_foot && bm_within(age, windows.rac1);
    }

    const bool out = get<std::uint8_t>(hero(BM_HERO_ITEM_THROWN)) != 0;

    // While the wrench is thrown, rac2 takes no Square; rac1 would slam, or throw from the crouch.
    if (out && bm_within(age, bm_scale_ticks(RAC1_WRENCH_AIR_LATE_WINDOW, &s.tick))) {
        pad_hide(BM_PAD_SQUARE, bm_scale_ticks(RAC1_WRENCH_AIR_LATE_WINDOW, &s.tick));
        s.decided += " wrench-out";
    }

    // In a swing or the throw, rac2 decides how it ends or chains into the next.
    if (st == RAC1_STATE_WRENCH_COMBO || st == RAC1_STATE_CROUCH_SWING) {
        const bm_swing_in in = swing_in(st);
        const bm_swing_steer sw = bm_swing_steer_for(&in, &s.tick);
        static const char* const kSwingActions[] = {"stay", "chain", "jump", "run", "idle"};

        // rac1's code is shown the frame, or the blend flag, that makes it do the same.
        if (sw.kind == BM_STEER_FRAME) {
            steer<float>(hero(BM_HERO_ANIM_FRAME), sw.frame, true);
        } else if (sw.kind == BM_STEER_HOLD) {
            steer<int32_t>(hero(BM_HERO_ANIM_CHANGING), 1, true);
        }

        // At the steered frame, older Square presses would count, so they are hidden.
        if (sw.kind == BM_STEER_FRAME && sw.hide_square > 0) {
            pad_hide(BM_PAD_SQUARE, sw.hide_square);
        }

        // The same goes for X.
        if (sw.kind == BM_STEER_FRAME && sw.hide_cross > 0) {
            pad_hide(BM_PAD_CROSS, sw.hide_cross);
        }

        // The trace notes the swing only when rac2's choice differs from rac1's.
        if (sw.rac2 != sw.rac1) {
            s.decided += std::string(" swing-") + kSwingActions[sw.rac2] + "-for-"
                         + kSwingActions[sw.rac1]
                         + (sw.kind == BM_STEER_FRAME  ? " steer-frame"
                            : sw.kind == BM_STEER_HOLD ? " steer-hold"
                                                       : "");
        }

        return false;
    }

    // While the wrench is out, Square starts nothing else.
    if (out) {
        return false;
    }

    // A Square too old for rac1's window but within rac2's is shown as pressed in the newest poll.
    if (bm_wrench_buffer(windows, age)) {
        const int newest =
            (get<int16_t>(pad(BM_PAD_HIST_IDX)) + BM_PAD_HIST_LEN - 1) % BM_PAD_HIST_LEN;
        pad_add(newest, BM_PAD_SQUARE);
        s.decided += " square-buffer";
        return on_foot;
    }

    return on_foot && bm_within(age, windows.rac1);
}

// Steers this tick's transitions after hero_update (DESIGN.md 2.3, 2.4 and 4.2): the flip, the
// plain jump, the run's exit and the strafe's standing start. `square` is true when rac1's item
// code takes a Square this tick.
void steer_transitions(int st, bool square) {
    const float stick = get<float>(hero(BM_HERO_STICK));
    const float facing = get<float>(hero(BM_HERO_FACING));
    const float off_camera = bm_diff(facing, camera_yaw());
    const int window =
        bm_scale_ticks(is_run(st) ? RAC1_RUN_JUMP_WINDOW : RAC1_IDLE_JUMP_WINDOW, &s.tick);
    int cross_slot = -1;
    const bool cross = pressed_within(BM_PAD_CROSS, window, &cross_slot) >= 0;

    s.steered_state = st;

    // The flip is rac1's crouch high jump, reached by showing R1 held and a stick flick beside X.
    // A Square the wrench takes this tick wins: rac2 swings, and R1 would make rac1 throw.
    if (s_set.flips && s.strafing && cross && !square
        && (st == RAC1_STATE_IDLE || st == RAC1_STATE_RUN)) {
        const int dir = bm_stick_vs_facing(get<float>(hero(BM_HERO_MOVE_YAW)), facing);
        const bool idle_ok = get<int16_t>(hero(BM_HERO_UNK308)) != 1;
        const bool run_ok = get<int16_t>(hero(BM_HERO_AIR_FRAMES))
                            <= bm_scale_ticks(RAC1_RUN_JUMP_AIR_TICKS, &s.tick);

        // Both rac2's flip test and rac1's conditions for its crouch jump this tick must pass.
        if (bm_flip(stick, dir) && (st == RAC1_STATE_IDLE ? idle_ok : run_ok)) {
            const std::uint32_t held = get<std::uint32_t>(pad(BM_PAD_HELD));

            // R1 is added, and taken back later, only if the player is not holding it.
            if (!(held & BM_PAD_R1)) {
                put<std::uint32_t>(pad(BM_PAD_HELD), held | BM_PAD_R1);
                s.pad_r1 = BM_PAD_R1;
            }

            pad_add(cross_slot, BM_PAD_FLICK);
            s.flip_steered = true;
            s.decided += " flip";
            return;
        }
    }

    // In the run, X gives the plain jump where rac2 does: while strafing, or under 0.85 of stick.
    // rac1 picks the running jump only above its walk band, so a target speed of 0 prevents it.
    if ((s_set.ground || s.strafing) && st == RAC1_STATE_RUN && cross
        && !bm_rac2_running_jump(stick, s.strafing)) {
        steer<float>(hero(BM_HERO_TARGET_SPEED), 0.0f);
        s.decided += " plain-jump";
        return;
    }

    // Where rac2 leaves the run, he goes to idle at once, without the skid. rac1 picks idle when
    // the speed is under its STOP test and the run has lasted 25 ticks.
    if (s_set.ground && st == RAC1_STATE_RUN && !cross
        && bm_run_exit(stick, s.strafing, off_camera)) {
        const float stop = RAC1_EXIT_STOP_SPEED * s.tick.ft;

        // rac1 is shown a speed just under its STOP test.
        if (get<float>(hero(BM_HERO_SPEED_XY)) >= stop) {
            steer<float>(hero(BM_HERO_SPEED_XY), stop * 0.99f);
        }

        const int idle_after = bm_scale_ticks(RAC1_EXIT_IDLE_TICKS, &s.tick);

        // rac1 is shown a timer past its 25 ticks, scaled as it scales them (30 on PAL).
        if (get<int32_t>(hero(BM_HERO_STATE_TIMER)) <= idle_after) {
            steer<int32_t>(hero(BM_HERO_STATE_TIMER), idle_after + 1);
        }

        s.idle_clamp = true;
        s.decided += " exit";
        return;
    }

    // Standing while strafing and turned away from the camera, rac1 is shown a stick past 0.22, so
    // it starts the walk that turns him.
    if (s.strafing && st == RAC1_STATE_IDLE && bm_strafe_start(facing, camera_yaw(), stick)) {
        steer<float>(hero(BM_HERO_STICK), RAC2_STRAFE_START_STICK + 0.01f);
        s.decided += " strafe-start";
    }
}

// ---- The twist ----

// Sets rac2's twist (func_L00_002B7B80) on rac1's pose records 0 and 1, before their easing runs.
void twist() {
    const gaddr table = level0(s_disc->pose_table);
    const gaddr hips = table + BM_POSE_HIPS * BM_POSE_RECORD;
    const gaddr body = table + BM_POSE_BODY * BM_POSE_RECORD;
    const float yaw = bm_twist(s.strafe_yaw, get<float>(hero(BM_HERO_FACING)));

    put<float>(hips + BM_POSE_TARGET_YAW, yaw);
    put<float>(hips + BM_POSE_ACCEL, RAC2_TWIST_HIP_ACCEL);
    put<float>(hips + BM_POSE_DAMP, RAC2_TWIST_HIP_DAMP);
    put<float>(body + BM_POSE_TARGET_YAW, -yaw);
    put<float>(body + BM_POSE_ACCEL, RAC2_TWIST_BODY_ACCEL);
    put<float>(body + BM_POSE_DAMP, RAC2_TWIST_BODY_DAMP);
}

// ---- The camera's turn ----

// Changes a follow camera word for the update about to read it; undo_camera_steers puts it back
// before the next tick.
void cam_steer(gaddr address, float value) {
    // An update needs at most eight steers, the size of the table.
    if (s.cam_steer_count >= 8) {
        return;
    }

    Steered& steered = s.cam_steers[s.cam_steer_count++];
    steered.address = address;
    steered.old_bits = get<std::uint32_t>(address);
    put<float>(address, value);
    steered.wrote_bits = get<std::uint32_t>(address);
    s.cam_steer_level = s_mod->loaded_level();
}

// Puts back the follow camera words steered for its last update. A word goes back only if it still
// holds what the mod wrote (rac1 resets the gain from its table anyway), and only while the same
// level is loaded, since another level's program sits elsewhere.
void undo_camera_steers() {
    // Newest first, so a word steered twice gets its first old value back.
    for (int i = s.cam_steer_count - 1; i >= 0 && s_mod->loaded_level() == s.cam_steer_level; --i) {
        const Steered& steered = s.cam_steers[i];

        // Only a word the game has not changed since goes back.
        if (get<std::uint32_t>(steered.address) == steered.wrote_bits) {
            put<std::uint32_t>(steered.address, steered.old_bits);
        }
    }

    s.cam_steer_count = 0;
}

// Sets the gain the stick turns the follow camera by, for the rest of its update after the
// overrides (DESIGN.md section 8). `cam` is the camera's moby.
void camera_speed(gaddr cam) {
    // The turn speed applies in play (a level, his moby, no scene), on foot or not, since Clank's
    // follow camera turns like Ratchet's.
    if (s_mod->loaded_level() < 0 || get<std::uint32_t>(hero(BM_HERO_MOBY)) == 0
        || get<int32_t>(level0(s_disc->cinematic_mode)) != 0) {
        return;
    }

    const gaddr gain_at = level0(s_disc->cam_gain);
    const float gain = get<float>(gain_at);
    const int option = get<int32_t>(s_mod->relocate(s_disc->cam_option));
    const float want = bm_camera_gain(s_set.camera, gain, option);

    // An unchanged gain needs no steer.
    if (want != gain) {
        cam_steer(gain_at, want);
    }

    const float x = get<float>(pad(BM_PAD_ANALOG));
    const float y = get<float>(pad(BM_PAD_ANALOG + 4));
    const gaddr vars = get<std::uint32_t>(cam + BM_CAMERA_VARS);
    // Without its variables, the camera counts as one the stick neither turns nor pitches (3).
    const int flags = valid(vars) ? get<int32_t>(vars + BM_CAMERA_TURN_FLAGS) : 3;

    // The trace gets a camera line while the stick turns it; the camera updates after the hero.
    if (s_set.trace && bm_camera_turning(x, y, flags)) {
        char line[200];
        std::snprintf(
            line,
            sizeof line,
            "camera option %d gain %.5f -> %.5f stick %.2f %.2f flags %d",
            option,
            gain,
            want,
            x,
            y,
            flags
        );
        s_mod->info(line);
    }
}

// ---- Clank ----

// Returns whether Clank alone is in play and the `clank` setting changes him: a level, his moby, no
// scene, vehicle mode 1 (DESIGN.md section 9). Giant Clank (mode 2) is left to the game.
bool clank_playing() {
    // The setting leaves him as the first game has him, or there is no hero yet.
    if (s_set.clank == BM_CLANK_RAC1 || s_mod->loaded_level() < 0
        || get<std::uint32_t>(hero(BM_HERO_MOBY)) == 0) {
        return false;
    }

    return get<int32_t>(level0(s_disc->cinematic_mode)) == 0
           && get<std::uint8_t>(hero(BM_HERO_VEHICLE_MODE)) == RAC1_VEHICLE_CLANK;
}

// Steers Clank's transitions this tick, after his update (DESIGN.md 9.4). A Square the setting's
// choice still remembers is shown as pressed in the newest poll, and the punch's frame is steered
// so rac1 leaves the punch where the choice does.
void clank_steer(int st) {
    const int age = press_age(BM_PAD_SQUARE);
    bool landing = false;
    bool reads_square = st == RAC1_STATE_CLANK_IDLE || bm_clank_is_run(st);

    s.steered_state = st;

    // In his jump, Square counts only when landing: past the apex, near the ground, after takeoff.
    if (st == RAC1_STATE_CLANK_JUMP || st == RAC1_STATE_CLANK_LEDGE_JUMP) {
        landing =
            get<int16_t>(hero(BM_HERO_PAST_APEX)) != 0
            && get<float>(hero(BM_HERO_GROUND_DISTANCE)) < RAC1_CLANK_LAND_SQUARE_HEIGHT
            && get<int32_t>(hero(BM_HERO_STATE_TIMER)) > get<int32_t>(hero(BM_HERO_TAKEOFF_FRAME));
        reads_square = landing;
    }

    // A Square older than rac1 remembers, but within the choice's window, is shown as pressed.
    if (reads_square && bm_clank_square_buffer(&s_clank, age, landing ? 1 : 0, &s.tick)) {
        const int newest =
            (get<int16_t>(pad(BM_PAD_HIST_IDX)) + BM_PAD_HIST_LEN - 1) % BM_PAD_HIST_LEN;
        pad_add(newest, BM_PAD_SQUARE);
        s.decided += " clank-square";
    }

    // Only the punch has a way out to steer.
    if (st != RAC1_STATE_CLANK_PUNCH) {
        return;
    }

    bm_clank_punch_in in{};
    in.record = get<int32_t>(hero(BM_HERO_ATTACK_RECORD));
    in.frame = get<float>(hero(BM_HERO_ANIM_FRAME));
    in.changing = get<int32_t>(hero(BM_HERO_ANIM_CHANGING)) != 0;
    in.stick = get<float>(hero(BM_HERO_STICK));
    in.cross = press_age(BM_PAD_CROSS);
    const bm_clank_punch_steer sw = bm_clank_punch(&s_clank, &in);

    // At the steered frame rac1 jumps out or stands; when it stands, X is hidden from its jump
    // window.
    if (sw.steer) {
        steer<float>(hero(BM_HERO_ANIM_FRAME), sw.frame, true);
        pad_hide(BM_PAD_CROSS, sw.hide_cross);
        s.decided += sw.jump      ? " clank-jump-out"
                     : sw.run_out ? " clank-run-out"
                                  : " clank-punch-exit";
    }
}

// Writes the trace line for one of Clank's ticks.
void clank_trace(int st) {
    // Nothing is written unless the trace is on.
    if (!s_set.trace) {
        return;
    }

    char line[400];
    std::snprintf(
        line,
        sizeof line,
        "clank state %#x stick %.3f (%.2f, %.2f) along %d turnaround %d err %.3f flat %.4f "
        "target %.4f ground %.4f facing %.3f anim %.2f rec %d square %d cross %d%s",
        st,
        get<float>(hero(BM_HERO_STICK)),
        get<float>(hero(BM_HERO_STICK_X)),
        get<float>(hero(BM_HERO_STICK_Y)),
        s.flags.along,
        s.flags.turnaround,
        get<float>(hero(BM_HERO_YAW_ERROR)),
        get<float>(hero(BM_HERO_FLAT_SPEED)),
        get<float>(hero(BM_HERO_TARGET_SPEED)),
        get<float>(hero(BM_HERO_GROUND_SPEED)),
        get<float>(hero(BM_HERO_FACING)),
        get<float>(hero(BM_HERO_ANIM_FRAME)),
        get<int32_t>(hero(BM_HERO_ATTACK_RECORD)),
        press_age(BM_PAD_SQUARE),
        press_age(BM_PAD_CROSS),
        s.decided.c_str()
    );
    s_mod->info(line);
}

// Runs Clank's tick in rac1.player.update_motion_input while clank_playing. It sets his jump's
// entry values on its first tick, lets the inner hooks handle his helpers' calls (with s.clank
// set), and steers his transitions after.
template <class Original>
void clank_update(Original original) {
    s.active = false;
    s.strafing = false;
    s.flipping = false;
    s.flip_steered = false;
    s.tick = read_tick();

    const int st = state();
    const bool new_state = st != s.last_state || get<int32_t>(hero(BM_HERO_STATE_TIMER)) == 0;
    s.last_state = st;

    // On his jump's first tick, before any physics, the choice's takeoff, animation frames and air
    // stick speed replace the ones rac1 set.
    if (new_state && st == RAC1_STATE_CLANK_JUMP) {
        bm_clank_jump in{};
        in.takeoff = get<int32_t>(hero(BM_HERO_TAKEOFF_FRAME));
        in.apex = get<float>(hero(BM_HERO_APEX_FRAME));
        in.target = get<float>(hero(BM_HERO_TARGET_FRAME));
        in.air_speed = get<float>(hero(BM_HERO_AIR_STICK_SPEED));
        const bm_clank_jump out = bm_clank_jump_entry(&s_clank, in, &s.tick);
        put<int32_t>(hero(BM_HERO_TAKEOFF_FRAME), out.takeoff);
        put<float>(hero(BM_HERO_APEX_FRAME), out.apex);
        put<float>(hero(BM_HERO_TARGET_FRAME), out.target);
        put<float>(hero(BM_HERO_AIR_STICK_SPEED), out.air_speed);
        s.decided += " clank-jump";
    }

    // The run flags are clear outside the run and as it starts, as in rac2.
    if (!bm_clank_is_run(st) || new_state) {
        s.flags = bm_run_flags{0, 0};
    }

    s.clank = true;
    s.in_update = true;
    original();
    s.in_update = false;
    s.clank = false;

    // A scene, the level's end or a switch back to Ratchet ends Clank's tick here.
    if (!clank_playing()) {
        return;
    }

    const int after = state();
    clank_steer(after);
    clank_trace(after);
}

// Handles rac1.player.turn_to_heading in Clank's update: his run's turn and his punch's lock-on
// turn.
template <class Original>
void clank_turn(Original original, int32_t mode, float accel, float damp, float limit) {
    const int st = state();
    const bm_spring in{accel, damp, limit};
    bm_spring out{};

    // In the run, rac2's spring replaces rac1's two, the turnaround's chosen by last tick's flag as
    // rac2 does.
    if (bm_clank_is_run(st)
        && bm_clank_turn(
            &s_clank, in, get<float>(hero(BM_HERO_STICK)), s.flags.turnaround, &s.tick, &out
        )) {
        original(mode, out.accel, out.damp, out.limit);
        return;
    }

    // In the punch, the lock-on turn gets the choice's limit.
    if (st == RAC1_STATE_CLANK_PUNCH && bm_clank_punch_turn(&s_clank, in, &s.tick, &out)) {
        original(mode, out.accel, out.damp, out.limit);
        return;
    }

    // A run turn the mod does not recognise goes through as it came, and the trace says so.
    if (bm_clank_is_run(st)) {
        s.decided += " clank-turn-unknown";
    }

    original(mode, accel, damp, limit);
}

// Handles rac1.player.approach_speed in Clank's update: his run's top speed, acceleration and
// braking.
template <class Original>
void clank_speed(Original original, float up, float down) {
    // Only the run changes; his punch, idle and glide speeds are the same in both games.
    if (!bm_clank_is_run(state())) {
        original(up, down);
        return;
    }

    const gaddr target = hero(BM_HERO_TARGET_SPEED);
    const float came = get<float>(target);
    const bm_speed sp = bm_clank_run_speed(&s_clank, came, down, &s.tick);
    put<float>(target, sp.target);
    original(sp.up, sp.down);

    // rac1's target goes back unless the game changed it.
    if (get<float>(target) == sp.target) {
        put<float>(target, came);
    }
}

// Handles rac1.player.set_planar_velocity in Clank's update. It updates rac2's run flags here,
// after his turn and speed step as rac2 does, and moves him along the stick while they say so.
template <class Original>
void clank_velocity(Original original, float angle) {
    // Only the run changes.
    if (!bm_clank_is_run(state())) {
        original(angle);
        return;
    }

    bm_clank_flag_in in{};
    in.stick = get<float>(hero(BM_HERO_STICK));
    in.yaw_error = get<float>(hero(BM_HERO_YAW_ERROR));
    in.flat_speed = get<float>(hero(BM_HERO_FLAT_SPEED));
    in.stick_swing =
        bm_diff(stick_angle_back(RAC2_CLANK_FLICK_FROM), stick_angle_back(RAC2_CLANK_FLICK_TO));
    bm_clank_flags_update(&s.flags, &s_clank, &in, &s.tick);

    // On magnet ground (unk308 set), rac1 keeps him on his facing.
    if (get<int16_t>(hero(BM_HERO_UNK308)) != 0) {
        original(angle);
        return;
    }

    // rac1 reads an angle past pi, here 99999, as his facing.
    original(s.flags.along ? get<float>(hero(BM_HERO_MOVE_YAW)) : 99999.0f);
}

// ---- The hooks ----

// Writes the trace line for one of Ratchet's ticks, with every input the decisions used.
void trace_tick(int st) {
    // Nothing is written unless the trace is on.
    if (!s_set.trace) {
        return;
    }

    char line[512];
    std::snprintf(
        line,
        sizeof line,
        "state %#x step %d stick %.3f (%.2f, %.2f) sector %d strafe %d "
        "along %d turnaround %d err %.3f flat %.4f target %.4f ground %.4f "
        "facing %.3f camera %.3f k %.1f flip %d anim %.2f rec %d square %d cross %d%s",
        st,
        get<int32_t>(hero(BM_HERO_STEP)),
        get<float>(hero(BM_HERO_STICK)),
        get<float>(hero(BM_HERO_STICK_X)),
        get<float>(hero(BM_HERO_STICK_Y)),
        s.sector,
        s.strafing ? 1 : 0,
        s.flags.along,
        s.flags.turnaround,
        get<float>(hero(BM_HERO_YAW_ERROR)),
        get<float>(hero(BM_HERO_FLAT_SPEED)),
        get<float>(hero(BM_HERO_TARGET_SPEED)),
        get<float>(hero(BM_HERO_GROUND_SPEED)),
        get<float>(hero(BM_HERO_FACING)),
        camera_yaw(),
        s.k,
        s.flipping ? 1 : 0,
        get<float>(hero(BM_HERO_ANIM_FRAME)),
        get<int32_t>(hero(BM_HERO_ATTACK_RECORD)),
        press_age(BM_PAD_SQUARE),
        press_age(BM_PAD_CROSS),
        s.decided.c_str()
    );
    s_mod->info(line);
}

// Hooks rac1.pad.process_report. It takes back last tick's pad and camera steers, then, on foot,
// takes the strafe buttons out of what the game sees and keeps them for the strafe.
template <class Original>
void process_report(Original original, gaddr pad_address, gaddr report, int32_t size) {
    // Only the first pad drives the hero.
    if (pad_address != s_disc->pad) {
        original(pad_address, report, size);
        return;
    }

    undo_pad_steers();
    undo_camera_steers();
    original(pad_address, report, size);
    s.strafe_held = false;

    // The strafe is off, or the game has the buttons now.
    if (!strafe_takes_pad()) {
        return;
    }

    const std::uint32_t mask = s_set.strafe;
    s.strafe_held = (get<std::uint32_t>(pad(BM_PAD_HELD)) & mask) != 0;

    // The buttons go from every copy the hero code reads; the menus read the _unmasked copies,
    // which keep them.
    for (std::uint32_t field :
         {BM_PAD_HELD,
          BM_PAD_PRESSED,
          BM_PAD_RELEASED,
          BM_PAD_RAW_HELD,
          BM_PAD_RAW_PRESSED,
          BM_PAD_RAW_RELEASED,
          BM_PAD_LATCHED}) {
        put<std::uint32_t>(pad(field), get<std::uint32_t>(pad(field)) & ~mask);
    }

    // They go from the newest poll of the press history too.
    const int newest = (get<int16_t>(pad(BM_PAD_HIST_IDX)) + BM_PAD_HIST_LEN - 1) % BM_PAD_HIST_LEN;
    const gaddr slot = pad(BM_PAD_HIST_BTN + 4 * newest);
    put<std::uint32_t>(slot, get<std::uint32_t>(slot) & ~mask);

    // L2's pressure goes as well.
    if (mask & BM_PAD_L2) {
        put<float>(pad(BM_PAD_ANALOG + 4 * 14), 0.0f);
    }

    // So does R2's.
    if (mask & BM_PAD_R2) {
        put<float>(pad(BM_PAD_ANALOG + 4 * 15), 0.0f);
    }
}

// Hooks rac1.player.advance_animation, the first hook of a tick, after the last tick's transitions.
// It undoes the hero steers, clamps idle after a steered run exit, and plays the backpedal.
template <class Original>
void advance_animation(Original original) {
    undo_hero_steers();

    // After a steered run exit, rac2 clamps idle's speed; by now idle has copied the momentum.
    if (s.idle_clamp) {
        s.idle_clamp = false;

        // The clamp applies only if the transitions did go from the run to idle.
        if (state() == RAC1_STATE_IDLE
            && get<int32_t>(hero(BM_HERO_PREV_STATE)) == RAC1_STATE_RUN) {
            const float clamp = bm_exit_clamp(&s.tick);
            float momentum[3];

            // The momentum's length is clamped to 1.5 units a second.
            for (int i = 0; i < 3; ++i) {
                momentum[i] = get<float>(hero(BM_HERO_MOMENTUM + 4 * i));
            }

            const float length = std::sqrt(
                momentum[0] * momentum[0] + momentum[1] * momentum[1] + momentum[2] * momentum[2]
            );

            // A longer momentum is scaled down to the clamp; one whose length is not finite stays.
            if (length > clamp && std::isfinite(length)) {
                // Every axis scales by the same factor, which keeps the direction.
                for (int i = 0; i < 3; ++i) {
                    put<float>(hero(BM_HERO_MOMENTUM + 4 * i), momentum[i] * clamp / length);
                }
            }

            // The ground speed is clamped too.
            if (get<float>(hero(BM_HERO_GROUND_SPEED)) > clamp) {
                put<float>(hero(BM_HERO_GROUND_SPEED), clamp);
            }
        }
    }

    // The backpedal steps the walk or run animation backwards, then runs the original with no step
    // of its own.
    if (s.strafing && is_run(state()) && bm_backpedal(s.sector, get<float>(hero(BM_HERO_STICK)))) {
        const gaddr moby = get<std::uint32_t>(hero(BM_HERO_MOBY));
        const int seq = valid(moby) ? get<std::uint8_t>(moby + BM_MOBY_SEQ) : -1;

        // Only a walk or run looping on itself, with no blend, loop or frame request pending, steps
        // back.
        if ((seq == BM_SEQ_WALK || seq == BM_SEQ_RUN)
            && get<std::uint8_t>(moby + BM_MOBY_PREV_SEQ) == seq
            && get<int32_t>(hero(BM_HERO_ANIM_BLEND_CURVE)) < 0
            && get<int32_t>(hero(BM_HERO_ANIM_QUEUED)) == 0
            && get<int32_t>(hero(BM_HERO_ANIM_LOOP_FRAME)) == -1) {
            const gaddr klass = get<std::uint32_t>(moby + BM_MOBY_CLASS);
            const gaddr record =
                valid(klass) ? get<std::uint32_t>(klass + BM_CLASS_SEQS + 4 * seq) : 0;
            const int count = valid(record) ? get<std::uint8_t>(record + BM_SEQ_FRAME_COUNT) : 0;
            bool keys_valid = true;
            const int frame = get<std::uint8_t>(moby + BM_MOBY_FRAME);

            // The sequence must have keys, and the current one must be among them.
            if (count > 0 && frame < count) {
                // The count is a u8, so 256 rates always fit.
                float rates[256];

                // Each key's rate is the first float of its frame data. A bad key stops the
                // backpedal.
                for (int i = 0; i < count; ++i) {
                    const gaddr key = get<std::uint32_t>(record + BM_SEQ_FRAMES + 4 * i);
                    keys_valid = keys_valid && valid(key);
                    rates[i] = keys_valid ? get<float>(key) : 0.0f;
                }

                // With a bad key, the animation advances as rac1 has it.
                if (!keys_valid) {
                    original();
                    return;
                }

                bm_anim anim{
                    get<float>(moby + BM_MOBY_ANIM_TIME),
                    frame,
                    get<std::uint8_t>(moby + BM_MOBY_NEXT)
                };
                const int from = bm_anim_place(&anim);
                const int moved =
                    bm_anim_step_back(&anim, get<float>(hero(BM_HERO_ANIM_SPEED)), rates, count);
                const bool wrapped = moved >= count || (moved > 0 && anim.frame > frame);

                // The original snaps a time over 0.99 to the next key, so the time stays under it.
                if (anim.time > 0.99f) {
                    anim.time = 0.99f;
                }

                const int to = bm_anim_place(&anim);
                put<std::uint8_t>(moby + BM_MOBY_FRAME, static_cast<std::uint8_t>(anim.frame));
                put<std::uint8_t>(moby + BM_MOBY_NEXT, static_cast<std::uint8_t>(anim.next));
                put<float>(moby + BM_MOBY_ANIM_TIME, anim.time);
                put<std::uint32_t>(
                    moby + BM_MOBY_FRAME_DATA,
                    get<std::uint32_t>(record + BM_SEQ_FRAMES + 4 * anim.frame)
                );
                put<std::uint32_t>(
                    moby + BM_MOBY_NEXT_DATA,
                    get<std::uint32_t>(record + BM_SEQ_FRAMES + 4 * anim.next)
                );
                put<float>(hero(BM_HERO_ANIM_RATE), rates[anim.frame]);

                const int triggers = get<std::uint8_t>(record + BM_SEQ_TRIGGER_COUNT);

                // The first sound trigger the step back crosses plays, as the original plays the
                // first one it crosses.
                for (int i = 0; i < triggers && s_play_sound != nullptr; ++i) {
                    const std::uint32_t trigger =
                        get<std::uint32_t>(record + BM_SEQ_FRAMES + 4 * (count + i));

                    // A trigger's place is its high 16 bits and its sound the low 16.
                    if (bm_anim_crossed_back(
                            from, to, static_cast<int32_t>(trigger) >> 16, wrapped
                        )) {
                        s_play_sound(static_cast<int32_t>(trigger & 0xFFFF), 0, moby);
                        break;
                    }
                }

                // The original runs with an animation speed of 0, so it takes no step of its own.
                const float speed = get<float>(hero(BM_HERO_ANIM_SPEED));
                put<float>(hero(BM_HERO_ANIM_SPEED), 0.0f);
                original();
                put<float>(hero(BM_HERO_ANIM_SPEED), speed);
                return;
            }
        }
    }

    original();
}

// Hooks rac1.player.update_motion_input (hero_update). It sets the tick's context before the update
// and the steers for the transitions after it.
template <class Original>
void update(Original original) {
    undo_hero_steers();
    s.decided.clear();

    // Clank alone gets his own tick under the `clank` setting.
    if (clank_playing()) {
        clank_update(original);
        return;
    }

    // Out of play or off his feet, the game runs unchanged and the mod keeps nothing of the tick.
    if (!playing()) {
        s.active = false;
        s.strafing = false;
        s.flipping = false;
        s.flip_steered = false;
        s.last_state = -1;
        original();
        return;
    }

    s.active = true;
    s.tick = read_tick();

    const int st = state();
    const bool new_state = st != s.last_state || get<int32_t>(hero(BM_HERO_STATE_TIMER)) == 0;
    s.last_state = st;
    s.strafing = strafe_now();
    // The stick's sector matters only while strafing.
    s.sector =
        s.strafing
            ? bm_stick_sector(get<float>(hero(BM_HERO_STICK_X)), get<float>(hero(BM_HERO_STICK_Y)))
            : BM_FORWARD;

    // For a flip the strafe started, rac2's drift applies from its first tick.
    if (s.flip_steered) {
        s.flip_steered = false;
        s.flipping = st == RAC1_STATE_HIGH_JUMP && new_state;

        // The flip starts drifting at 5 units a second.
        if (s.flipping) {
            put<float>(hero(BM_HERO_DRIFT_SPEED), bm_flip_drift_start(&s.tick));
        }
    } else if (st != RAC1_STATE_HIGH_JUMP) {
        // Out of the high jump, the flip is over.
        s.flipping = false;
    }

    // On each tick of the flip, the drift is preset so rac1's easing lands on rac2's, and the push
    // along the takeoff yaw is removed.
    if (s.flipping) {
        const bool to_zero = get<int16_t>(hero(BM_HERO_PAST_APEX)) != 0
                             && get<int32_t>(hero(BM_HERO_FRAMES_TO_LAND))
                                    < bm_scale_ticks(RAC1_FLIP_DRIFT_LANDING_TICKS, &s.tick);
        const float drift = get<float>(hero(BM_HERO_DRIFT_SPEED));
        put<float>(
            hero(BM_HERO_DRIFT_SPEED),
            bm_flip_drift_preset(drift, get<float>(hero(BM_HERO_STICK)), to_zero ? 1 : 0, &s.tick)
        );
        put<float>(hero(BM_HERO_AIR_TARGET_SPEED), 0.0f);
    }

    // On a jump's first tick, rac2's air stick speed is set.
    if (s_set.air && new_state && bm_is_jump_state(st)) {
        const gaddr speed = hero(BM_HERO_AIR_STICK_SPEED);
        put<float>(speed, bm_air_stick_speed(st, get<float>(speed), &s.tick));
    }

    // In the run, rac2's flags come from last tick's yaw error, and the turnaround flag goes to
    // rac1's unk3BE too.
    if (s_set.ground && is_run(st)) {
        bm_run_in in = run_in();
        bm_run_flags_update(&s.flags, &in, &s.tick);
        put<int16_t>(hero(BM_HERO_TURNAROUND), static_cast<int16_t>(s.flags.turnaround));
    } else if (!is_run(st)) {
        // Outside the run, the flags are clear.
        s.flags = bm_run_flags{0, 0};
    }

    s.k = 1.0f;
    s.in_update = true;
    original();
    s.in_update = false;

    const int after = state();

    // The update can end play, for example when he falls out of the world.
    if (!playing()) {
        return;
    }

    // The twist is set here, for the easing that runs after the transitions.
    if (s_set.twist && s.strafing
        && (is_run(after) || bm_is_jump_state(after) || after == RAC1_STATE_FALL)
        && bm_twist_on(
            get<float>(hero(BM_HERO_STICK)),
            after,
            bm_backpedal(s.sector, get<float>(hero(BM_HERO_STICK)))
        )) {
        twist();
    }

    // rac2's flip moves him at his flat speed.
    if (s.flipping && after == RAC1_STATE_HIGH_JUMP) {
        put<float>(hero(BM_HERO_GROUND_SPEED), get<float>(hero(BM_HERO_FLAT_SPEED)));
    }

    steer_transitions(after, steer_wrench(after));
    trace_tick(after);
}

// Hooks rac1.player.turn_to_heading: rac2's turns in the run, the wrench's lock-on turn, and the
// strafe's facing.
template <class Original>
void turn(Original original, int32_t mode, float accel, float damp, float limit) {
    // Clank's update has its own turns.
    if (s.in_update && s.clank) {
        clank_turn(original, mode, accel, damp, limit);
        return;
    }

    // Outside the hero's update, or out of play, the call goes through as it came.
    if (!s.in_update || !s.active) {
        original(mode, accel, damp, limit);
        return;
    }

    const int st = state();

    // The wrench's lock-on turn in the swings, the slam and the throw gets rac2's spring.
    if (s_set.wrench
        && (st == RAC1_STATE_WRENCH_COMBO || st == RAC1_STATE_WRENCH_SLAM
            || st == RAC1_STATE_CROUCH_SWING)) {
        bm_spring out{};

        // Only rac1's lock-on turn, recognised by its numbers, is replaced.
        if (bm_wrench_turn(st, bm_spring{accel, damp, limit}, &s.tick, &out)) {
            original(mode, out.accel, out.damp, out.limit);
            return;
        }

        original(mode, accel, damp, limit);
        return;
    }

    // In the run, the strafe or the ground setting decides the turn.
    if (is_run(st)) {
        // While strafing, he turns to face the camera on the strafe's own yaw rate, as in rac2, and
        // the side reversal scales his speed.
        if (s.strafing) {
            const float move = get<float>(hero(BM_HERO_MOVE_YAW));
            s.strafe_yaw = move;
            put<float>(hero(BM_HERO_YAW_RATE), s.strafe_rate);
            put<float>(hero(BM_HERO_MOVE_YAW), camera_yaw());

            // Under a surface's gravity rac1 turns him to the stick's way in the world instead of
            // the move yaw: for the call, that way is the stick's forward (the camera's row 0).
            const bool surface = get<std::uint8_t>(hero(BM_HERO_GRAVITY_MODE)) != 0;
            float stick_dir[4];

            if (surface) {
                for (int i = 0; i < 4; i++) {
                    stick_dir[i] = get<float>(hero(BM_HERO_STICK_DIR + 4 * i));
                    put<float>(
                        hero(BM_HERO_STICK_DIR + 4 * i),
                        get<float>(level0(s_disc->cam_matrix) + 4 * i)
                    );
                }
            }

            original(
                mode,
                rac2_spring(RAC2_STRAFE_TURN_ACCEL, &s.tick),
                rac2_spring(RAC2_STRAFE_TURN_DAMP, &s.tick),
                rac2_turn_limit(RAC2_STRAFE_TURN_LIMIT, &s.tick)
            );

            if (surface) {
                for (int i = 0; i < 4; i++) {
                    put<float>(hero(BM_HERO_STICK_DIR + 4 * i), stick_dir[i]);
                }
            }

            s.strafe_rate = get<float>(hero(BM_HERO_YAW_RATE));
            put<float>(hero(BM_HERO_YAW_RATE), 0.0f);
            put<float>(hero(BM_HERO_YAW_ERROR), 0.0f);
            put<float>(hero(BM_HERO_MOVE_YAW), move);

            const float side_factor = bm_side_factor(
                &s.sides, s.sector, get<std::uint8_t>(hero(BM_HERO_VEHICLE_MODE)) == 0
            );
            put<float>(
                hero(BM_HERO_GROUND_SPEED), get<float>(hero(BM_HERO_GROUND_SPEED)) * side_factor
            );
            return;
        }

        // Not strafing, rac2's turn applies, with k taken from the yaw error before the turn.
        if (s_set.ground) {
            const bm_run_in in = run_in();
            const bm_turn spring = bm_run_turn(&in, &s.flags, &s.tick);
            s.k = bm_run_k(&in, &s.flags);
            original(mode, spring.accel, spring.damp, spring.limit);
            return;
        }
    }

    // In the air after takeoff (the jump group's air turn, or the fall), strafing, he faces the
    // camera while still steering along the stick. The high jump (the flips), the pack long jump,
    // the ledge jump (0x1C) and states 0x0C, 0x11 and 0x4C keep the game's own turn.
    if (s.strafing
        && (st == RAC1_STATE_FALL
            || (bm_is_jump_state(st) && st != RAC1_STATE_HIGH_JUMP && st != 0x0C
                && st != RAC1_STATE_PACK_LONG_JUMP && st != 0x11 && st != 0x1C && st != 0x4C
                && get<int32_t>(hero(BM_HERO_STATE_TIMER))
                       >= get<int32_t>(hero(BM_HERO_TAKEOFF_FRAME))))) {
        const float move = get<float>(hero(BM_HERO_MOVE_YAW));
        s.strafe_yaw = move;
        put<float>(hero(BM_HERO_MOVE_YAW), camera_yaw());
        original(mode, accel, damp, limit);
        put<float>(hero(BM_HERO_MOVE_YAW), move);
        return;
    }

    original(mode, accel, damp, limit);
}

// Hooks rac1.player.approach_speed: rac2's target speed, acceleration and braking, and the slam's
// drive.
template <class Original>
void approach_speed(Original original, float up, float down) {
    // Clank's update has its own speeds.
    if (s.in_update && s.clank) {
        clank_speed(original, up, down);
        return;
    }

    // Outside the hero's update, or out of play, the call goes through as it came.
    if (!s.in_update || !s.active) {
        original(up, down);
        return;
    }

    const int st = state();

    // In the slam, off ice and high up, rac2 drives on at 4.5 units a second; rac1 drives only
    // toward a target.
    if (s_set.wrench && st == RAC1_STATE_WRENCH_SLAM && get<std::uint8_t>(hero(BM_HERO_ON_ICE)) == 0
        && bm_slam_drive(
            get<float>(hero(BM_HERO_GROUND_DISTANCE)),
            get<std::uint32_t>(hero(BM_HERO_MELEE_TARGET)) != 0,
            get<int32_t>(hero(BM_HERO_STATE_TIMER)),
            &s.tick
        )) {
        const gaddr target = hero(BM_HERO_TARGET_SPEED);
        const float came = get<float>(target);
        const float want = bm_slam_drive_speed(&s.tick);

        // The drive only ever raises rac1's target.
        if (came < want) {
            put<float>(target, want);
            s.decided += " slam-drive";
        }

        original(up, down);

        // rac1's target goes back unless the game changed it.
        if (came < want && get<float>(target) == want) {
            put<float>(target, came);
        }

        return;
    }

    // With the ground setting off, the call goes through as it came.
    if (!s_set.ground) {
        original(up, down);
        return;
    }

    // In the run, rac2's speeds apply, scaled as another mod scaled rac1's (read from `down`).
    if (is_run(st)) {
        // A push (a conveyor) adds to rac1's target, so rac1's numbers stay.
        if (get<float>(hero(BM_HERO_PUSH)) > 0.0f) {
            original(up, down);
            return;
        }

        const float own = bm_rac1_run_down(get<int16_t>(hero(BM_HERO_TURNAROUND)) != 0, &s.tick);
        float scale = own > 0.0f ? down / own : 1.0f;

        // A scale that is not a number, or negative, counts as 1.
        if (!std::isfinite(scale) || scale < 0.0f) {
            scale = 1.0f;
        }

        bm_run_in in = run_in();

        // While strafing, rac2 uses a yaw error of 0 here and a k of 1.
        if (s.strafing) {
            in.yaw_error = 0.0f;
        }

        const bm_speed sp = bm_run_speed(&in, &s.flags, s.strafing ? 1.0f : s.k, &s.tick);
        const gaddr target = hero(BM_HERO_TARGET_SPEED);
        const float came = get<float>(target);
        const float want = sp.target * scale;
        put<float>(target, want);
        original(sp.up * scale, sp.down * scale);

        // rac1's target goes back, so nothing else (the difficulty mod, for one) reads rac2's.
        if (get<float>(target) == want) {
            put<float>(target, came);
        }

        return;
    }

    // rac2 brakes harder in the idle group, STOP and the crouch; other states keep rac1's braking.
    original(up, down * bm_brake_scale(st, get<std::uint8_t>(hero(BM_HERO_ON_ICE)) != 0));
}

// Hooks rac1.player.set_planar_velocity: moves him along the stick where rac2 does.
template <class Original>
void planar_velocity(Original original, float angle) {
    // Clank's update has its own velocity.
    if (s.in_update && s.clank) {
        clank_velocity(original, angle);
        return;
    }

    // Outside the hero's update, or out of play, the call goes through as it came.
    if (!s.in_update || !s.active) {
        original(angle);
        return;
    }

    const int st = state();

    // In the run, the strafe or rac2's flag decides the direction.
    if (is_run(st)) {
        // While strafing, he moves along the stick.
        if (s.strafing) {
            original(s.strafe_yaw);

            // Under a surface's gravity rac1 moves him along his facing whatever the angle, so the
            // velocity is set along the stick's way on the surface, at his speed.
            if (get<std::uint8_t>(hero(BM_HERO_GRAVITY_MODE)) != 0) {
                float stick[3];
                float normal[3];
                float v[3];

                for (int i = 0; i < 3; i++) {
                    stick[i] = get<float>(hero(BM_HERO_STICK_DIR + 4 * i));
                    normal[i] = get<float>(hero(BM_HERO_GROUND_NORMAL + 4 * i));
                }

                if (bm_surface_strafe_velocity(
                        stick, normal, get<float>(hero(BM_HERO_GROUND_SPEED)), v
                    )) {
                    for (int i = 0; i < 3; i++) {
                        put<float>(hero(BM_HERO_VELOCITY + 4 * i), v[i]);
                    }
                }
            }

            return;
        }

        // Off magnet ground, rac2's flag moves him along the move yaw while he is slow and turned
        // away from the stick, and along his facing (an angle of 99999) otherwise.
        if (s_set.ground && get<int16_t>(hero(BM_HERO_UNK308)) == 0) {
            original(s.flags.along ? get<float>(hero(BM_HERO_MOVE_YAW)) : 99999.0f);
            return;
        }
    }

    // Before a jump's takeoff while strafing, he moves along the last strafe yaw where rac1 asks
    // for his facing (an angle past pi).
    if (s.strafing && bm_is_jump_state(st) && angle > BM_PI
        && get<int32_t>(hero(BM_HERO_STATE_TIMER)) < get<int32_t>(hero(BM_HERO_TAKEOFF_FRAME))) {
        original(s.strafe_yaw);
        return;
    }

    original(angle);
}

// Runs the camera's overrides, then, while strafing, asks for rac1's strafe camera.
template <class Original>
void strafe_camera(Original original, gaddr cam) {
    // Not strafing, or without the game functions, the camera stays the game's.
    if (!s.strafing || !s.active || s_orbit_clamp == nullptr || s_start_slide == nullptr
        || s_lateral_spring == nullptr) {
        original(cam);
        return;
    }

    const gaddr weight = level0(s_disc->blend_weight);
    const float before = get<float>(weight);
    original(cam);
    // rac1's state 0x81 makes these requests: orbit clamp 0, the slide, lateral spring 0.04 / 0.2.
    s_orbit_clamp(0);
    s_start_slide();
    s_lateral_spring(0.04f, 0.2f);

    const gaddr vars = get<std::uint32_t>(cam + BM_CAMERA_VARS);

    // The recentre timers are cleared as state 0x81 clears them, and the blend weight the
    // recentring wrote is put back.
    if (valid(vars)) {
        put<int16_t>(vars + 0x12, 0);
        put<int16_t>(vars + 0x14, 0);
        put<int16_t>(vars + 0x16, 0);
    }

    put<float>(weight, before);
}

// Hooks rac1.camera.follow.apply_overrides. It undoes the last update's camera steers, asks for
// rac1's strafe camera (its state 0x81's) while strafing, then sets the turn speed for the rest of
// the camera's update.
template <class Original>
void camera(Original original, gaddr cam) {
    undo_camera_steers();
    strafe_camera(original, cam);

    // The `rac1` camera choice leaves the turn speed alone.
    if (s_set.camera != BM_CAMERA_RAC1) {
        camera_speed(cam);
    }
}

// Returns the buttons for a `strafe` setting value, or the default, L2 and R2, for anything else.
std::uint32_t strafe_buttons(const std::string& value) {
    // `l2` strafes on L2 only.
    if (value == "l2") {
        return BM_PAD_L2;
    }

    // `r2` strafes on R2 only.
    if (value == "r2") {
        return BM_PAD_R2;
    }

    // `off` takes no button.
    if (value == "off") {
        return 0;
    }

    return BM_PAD_L2 | BM_PAD_R2;
}

// Returns the BM_CAMERA_* (camera.h) for a `camera` setting value. Anything but `rac1` and `rac2`
// gives the default, Enhanced, including 0.3.0's `modern` from a host that does not replace a
// removed value with the default as the port does.
int camera_choice(const std::string& value) {
    // `rac1` keeps the first game's camera.
    if (value == "rac1") {
        return BM_CAMERA_RAC1;
    }

    // `rac2` gives Going Commando's speeds.
    if (value == "rac2") {
        return BM_CAMERA_RAC2;
    }

    return BM_CAMERA_ENHANCED;
}

// Returns the BM_CLANK_* (clank.h) for a `clank` setting value, or the default, enhanced, for
// anything else.
int clank_choice(const std::string& value) {
    // `rac1` leaves Clank as the first game has him.
    if (value == "rac1") {
        return BM_CLANK_RAC1;
    }

    // `rac2` gives Going Commando's Clank.
    if (value == "rac2") {
        return BM_CLANK_RAC2;
    }

    return BM_CLANK_ENHANCED;
}

}  // namespace

void load(openrac::Mod& mod) {
    s_mod = &mod;

    // 1. The disc must have a row in discs.h; an address is never guessed.
    s_disc = nullptr;

    // Each row is one disc the mod supports.
    for (const bm_disc& disc : k_bm_discs) {
        // The running disc's version picks its row.
        if (mod.version() == disc.version) {
            s_disc = &disc;
        }
    }

    // A disc without a row is refused.
    if (s_disc == nullptr) {
        throw openrac::Error(
            "Works only with the PAL v2.00 and NTSC-U v1.00 discs, not "
            + std::string(mod.version())
        );
    }

    // 2. The settings are read once.
    s_set = Settings{};
    s_set.ground = mod.setting<bool>("ground");
    s_set.air = mod.setting<bool>("air");
    s_set.strafe = strafe_buttons(mod.setting<std::string>("strafe"));
    s_set.flips = mod.setting<bool>("flips");
    s_set.twist = mod.setting<bool>("twist");
    s_set.wrench = mod.setting<bool>("wrench");
    s_set.camera = camera_choice(mod.setting<std::string>("camera"));
    s_set.clank = clank_choice(mod.setting<std::string>("clank"));
    const char* env = std::getenv("OPENRAC_ENHANCED_MOVEMENT_TRACE");
    s_set.trace = mod.setting<bool>("trace") || (env != nullptr && std::strcmp(env, "1") == 0);

    // 3. Every piece of state the hooks keep is reset.
    s = State{};
    s_clank = bm_clank_params_for(s_set.clank);

    const bool strafe = s_set.strafe != 0;
    const bool clank = s_set.clank != BM_CLANK_RAC1;
    const bool movement = s_set.ground || s_set.air || strafe || s_set.wrench || clank;
    const bool camera_turn = s_set.camera != BM_CAMERA_RAC1;

    // With every part off, and the camera and Clank as the first game has them, no hook is taken.
    if (!movement && !camera_turn) {
        mod.info("Every setting is off, so the mod changes nothing.");
        return;
    }

    // 4. Only the strafe calls game functions: the strafe camera's, and the backpedal's sound.
    if (strafe) {
        s_orbit_clamp = mod.required_function<OrbitClampFn>(s_disc->orbit_clamp);
        s_start_slide = mod.required_function<SlideFn>(s_disc->start_slide);
        s_lateral_spring = mod.required_function<SpringFn>(s_disc->lateral_spring);
        s_play_sound = mod.function<PlaySoundFn>(s_disc->play_sound);
    }

    // 5. Each hook taken runs the game's code, or the next mod's, when it has nothing to do. The
    // hero's update and animation serve every part but the camera, Clank included.
    if (movement) {
        mod.hook<hp::Hero_UpdateMotionInput>([](auto original) { update(original); });
        mod.hook<hp::Hero_AdvanceAnimation>([](auto original) { advance_animation(original); });
    }

    // The turn serves the run, the strafe, the wrench's lock-on turn, and Clank's run and punch.
    if (s_set.ground || strafe || s_set.wrench || clank) {
        mod.hook<hp::Hero_TurnToHeading>(
            [](auto original, int32_t mode, float accel, float damp, float limit) {
                turn(original, mode, accel, damp, limit);
            }
        );
    }

    // The velocity serves the run, the strafe and Clank.
    if (s_set.ground || strafe || clank) {
        mod.hook<hp::Hero_SetPlanarVelocity>([](auto original, float angle) {
            planar_velocity(original, angle);
        });
    }

    std::string missing;

    // The speeds serve the run, the slam's drive and Clank's run. The 1.x line added this hook
    // point, so an older port may lack it.
    if (s_set.ground || s_set.wrench || clank) {
        // A port without it refuses the hook, and the rest of the mod still loads.
        try {
            mod.hook<hp::Hero_ApproachSpeed>([](auto original, float up, float down) {
                approach_speed(original, up, down);
            });
        } catch (const openrac::Error& e) {
            missing = std::string(" Speed changes: off (") + e.what() + ").";
        }
    }

    // The pad takes the strafe buttons, and before each tick undoes the press steers of the flip,
    // the wrench and Clank, and the camera's steers.
    if (strafe || s_set.wrench || camera_turn || clank) {
        mod.hook<hp::Pad_ProcessReport>(
            [](auto original, gaddr pad_address, gaddr report, int32_t size) {
                process_report(original, pad_address, report, size);
            }
        );
    }

    // The camera hook serves the strafe camera and the camera's turn speed.
    if (strafe || camera_turn) {
        mod.hook<hp::FollowCamera_ApplyOverrides>([](auto original, gaddr cam) {
            camera(original, cam);
        });
    }

    // 6. One line in the log says how the game will play.
    const char* buttons = s_set.strafe == (BM_PAD_L2 | BM_PAD_R2) ? "L2 or R2"
                          : s_set.strafe == BM_PAD_L2             ? "L2 only"
                                                                  : "R2 only";
    mod.info(
        std::string("Disc: ") + std::string(mod.version())
        + ". Running: " + (s_set.ground ? "Going Commando" : "Original")
        + ". Air control: " + (s_set.air ? "Going Commando" : "Original") + ". Strafe: "
        + (strafe ? std::string(buttons) + (s_set.flips ? ", flips" : "")
                        + (s_set.twist ? ", hip twist" : "")
                  : "Off")
        + ". Wrench: " + (s_set.wrench ? "Going Commando" : "Original") + ". Camera: "
        + (s_set.camera == BM_CAMERA_ENHANCED ? "Enhanced"
           : s_set.camera == BM_CAMERA_RAC2   ? "Going Commando"
                                              : "Original")
        + ". Clank: "
        + (s_set.clank == BM_CLANK_ENHANCED ? "Enhanced"
           : s_set.clank == BM_CLANK_RAC2   ? "Going Commando"
                                            : "Original")
        + (s_set.trace ? ". Trace: on" : "") + "." + missing
    );

    // A missing speed hook is a warning too.
    if (!missing.empty()) {
        mod.warn(
            "This OpenRAC is too old for part of the mod: Ratchet and Clank keep their original "
            "running speeds, and a slam from high up doesn't drive forward. Update OpenRAC to get "
            "them."
        );
    }
}

}  // namespace enhanced_movement::rac1
