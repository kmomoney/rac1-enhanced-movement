/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The strafe decisions: Going Commando's strafe, apart from game memory (DESIGN.md section 4). They
 * cover the stick's sector, the side timers, the flip, the hip twist, the start from standing, and
 * stepping a looping animation backward for the backpedal. Written for this mod from rac2-decomp:
 * func_L00_002B39F0 (sectors), func_L00_002B35A0 (facing and sides), func_L00_002C9FC8 (flip),
 * func_L00_002B6BC8 (flip drift), func_L00_002B7B80 (twist) and func_L00_002CB3A8 (standing start).
 */
#ifndef RAC1_ENHANCED_MOVEMENT_STRAFE_H
#define RAC1_ENHANCED_MOVEMENT_STRAFE_H

#include <math.h>

#include "rac1/angle.h"
#include "rac1/rac1.h"
#include "rac1/rac2.h"

/* Stick directions, coded as rac2's +0x1457 and rac1's stick_direction_vs_facing code them. */
enum {
    BM_LEFT = 0,
    BM_RIGHT = 1,
    BM_FORWARD = 2,
    BM_BACK = 3
};

/* Returns the stick's sector on screen (func_L00_002B39F0). `x` and `y` are the pad's axes, x to
 * the right and y down. It tests back, then right, then left, each within 45 degrees, and otherwise
 * returns forward. */
static inline int bm_stick_sector(float x, float y) {
    const float a = atan2f(y, x);

    // Down on the stick, tested first, is back.
    if (bm_diff(a, BM_HALF_PI) < RAC2_STRAFE_SECTOR) {
        return BM_BACK;
    }

    // Within 45 degrees of right is right.
    if (bm_diff(a, 0.0f) < RAC2_STRAFE_SECTOR) {
        return BM_RIGHT;
    }

    // Within 45 degrees of left is left.
    if (bm_diff(a, BM_PI) < RAC2_STRAFE_SECTOR) {
        return BM_LEFT;
    }

    return BM_FORWARD;
}

/* Returns whether he backpedals: the stick is in the back sector and past 0.15 (rac2's +0x1456). */
static inline int bm_backpedal(int sector, float stick) {
    return sector == BM_BACK && stick > RAC2_STRAFE_BACKPEDAL_STICK;
}

/* Returns the stick's direction against his facing, as rac1's stick_direction_vs_facing gives it
 * for the flip: the move yaw less the facing, tested for back, right and left within 45 degrees,
 * else forward. */
static inline int bm_stick_vs_facing(float move_yaw, float facing) {
    const float a = bm_sub(move_yaw, facing);

    // Behind him, within 45 degrees (0.785 rad).
    if (bm_diff(a, BM_PI) < 0.785398185f) {
        return BM_BACK;
    }

    // To his right; the yaw grows to his left.
    if (bm_diff(a, -BM_HALF_PI) < 0.785398185f) {
        return BM_RIGHT;
    }

    // To his left.
    if (bm_diff(a, BM_HALF_PI) < 0.785398185f) {
        return BM_LEFT;
    }

    return BM_FORWARD;
}

/* The side timers, in rac2 ticks. */
typedef struct bm_sides {
    int left;  /* +0x146E, set going left */
    int right; /* +0x146C, set going right */
} bm_sides;

/* Returns this tick's factor on the ground speed from the side timers (func_L00_002B35A0). Both
 * timers count down first. On foot, going left while the right timer runs gives -0.25, and going
 * right while the left timer runs gives (4 - its count) / 4. */
static inline float bm_side_factor(bm_sides* s, int sector, int on_foot) {
    float factor = 1.0f;

    // The left timer counts down each strafing tick.
    if (s->left > 0) {
        s->left--;
    }

    // So does the right timer.
    if (s->right > 0) {
        s->right--;
    }

    // Changing sides costs speed only on foot.
    if (!on_foot) {
        return factor;
    }

    // Going left restarts the left timer; coming from the right sends him back at a quarter speed.
    if (sector == BM_LEFT) {
        s->left = RAC2_STRAFE_SIDE_TICKS;

        // He went right within the last five ticks.
        if (s->right != 0) {
            factor = RAC2_STRAFE_REVERSE;
        }
    } else if (sector == BM_RIGHT) {
        // Going right restarts the right timer.
        s->right = RAC2_STRAFE_SIDE_TICKS;

        // Coming from the left within five ticks, the speed comes back a quarter at a time.
        if (s->left != 0) {
            factor = (4.0f - (float)s->left) * 0.25f;
        }
    }

    return factor;
}

/* Returns whether his facing is still `angle` or more off the camera's yaw while strafing
 * (func_L00_002B3268). */
static inline int bm_strafe_turning(float facing, float camera_yaw, float angle) {
    return bm_diff(facing, camera_yaw) >= angle;
}

/* Returns whether idle should start the walk to turn him to the camera while strafing, as rac2's
 * standing start does: 10 degrees or more off the camera, with a stick of 0.22 or less, on which
 * rac1 would not start by itself. */
static inline int bm_strafe_start(float facing, float camera_yaw, float stick) {
    return bm_strafe_turning(facing, camera_yaw, RAC2_STRAFE_START_ANGLE)
           && !(stick > RAC2_STRAFE_START_STICK);
}

/* Returns whether X starts a flip while strafing (func_L00_002C9FC8): the stick is past 0.9 and not
 * forward of his facing. */
static inline int bm_flip(float stick, int stick_vs_facing) {
    return stick > RAC2_FLIP_STICK && stick_vs_facing != BM_FORWARD;
}

/* Returns the flip drift (high_jump_drift_speed) to set before rac1's tick of state 0xB, so that
 * rac1's easing lands on rac2's value. rac1 eases toward 4.2 u/s by 8, or toward 0 near landing
 * (`rac1_to_zero`). rac2 eases toward max(stick * 5, 2) by 37 (func_L00_002B6BC8). */
static inline float bm_flip_drift_preset(
    float drift, float stick, int rac1_to_zero, const bm_tick* t
) {
    float goal = stick * rac2_speed(RAC2_FLIP_DRIFT_PER_STICK, t);
    const float rac1_goal = rac1_to_zero ? 0.0f : RAC1_FLIP_DRIFT * t->ft;
    const float rac1_step = RAC1_FLIP_DRIFT_STEP * t->ft2;
    float want;

    // rac2's goal never drops under 2 u/s.
    if (goal < rac2_speed(RAC2_FLIP_DRIFT_MIN, t)) {
        goal = rac2_speed(RAC2_FLIP_DRIFT_MIN, t);
    }

    want = bm_approach(drift, goal, rac2_accel(RAC2_FLIP_DRIFT_STEP, t));

    // Above rac1's goal rac1 will step it down, so start one step higher.
    if (want > rac1_goal) {
        return want + rac1_step;
    }

    // Below it rac1 will step it up, so start one step lower.
    if (want < rac1_goal) {
        return want - rac1_step;
    }

    return want;
}

/* Returns the flip drift at its start, 5 u/s, as rac2's state entry sets it for the strafe. */
static inline float bm_flip_drift_start(const bm_tick* t) {
    return rac2_speed(RAC2_FLIP_DRIFT_START, t);
}

/* Returns the hip twist's yaw: the direction he moves less his facing (func_L00_002B7B80). It is
 * pose record 0's target, and record 1 takes its negative. */
static inline float bm_twist(float strafe_yaw, float facing) {
    return bm_sub(strafe_yaw, facing);
}

/* Returns whether the twist applies: the stick is past 0.15 and he is neither flipping (state 0xB)
 * nor backpedalling. */
static inline int bm_twist_on(float stick, int state, int backpedal) {
    return stick > RAC2_TWIST_STICK && state != RAC1_STATE_HIGH_JUMP && !backpedal;
}

/* A place in a looping animation: `time` of the way from key `frame` to key `next` (frame + 1,
 * wrapping). rac1 keeps these as the moby's frame, prev_frame and anim_time. */
typedef struct bm_anim {
    float time;
    int frame;
    int next;
} bm_anim;

/* Steps a looping animation back by `speed` (anim_speed) times the key's rate, the reverse of
 * advance_hero_animation. Each key's time runs at its own rate, so time carried past a key is
 * divided by the old key's rate and multiplied by the new one's. `rates[i]` is key i's rate.
 * Returns the number of keys stepped back over. */
static inline int bm_anim_step_back(bm_anim* a, float speed, const float* rates, int count) {
    int moved = 0;

    // An animation with no keys, or a place outside it, stays where it is.
    if (count <= 0 || a->frame < 0 || a->frame >= count) {
        return 0;
    }

    a->time -= speed * rates[a->frame];

    // Each key passed takes it back one, with the time carried at the new key's rate, for a lap at
    // most. A key with no rate carries no time.
    while (a->time < 0.0f && moved < count) {
        const int old = a->frame;
        const float old_rate = rates[old];

        a->next = old;
        a->frame = (old + count - 1) % count;
        a->time = (old_rate > 0.0f ? a->time / old_rate * rates[a->frame] : 0.0f) + 1.0f;
        moved++;
    }

    // Only bad rates step back a whole lap in a tick; the place then stops at the key's start.
    if (a->time < 0.0f) {
        a->time = 0.0f;
    }

    return moved;
}

/* Returns the place as advance_hero_animation compares it with the sound triggers: the key times 16
 * plus the sixteenths of the way to the next. */
static inline int bm_anim_place(const bm_anim* a) {
    return a->frame * 16 + (int)(a->time * 16.0f);
}

/* Returns whether stepping back from place `from` to place `to` passed a trigger at `at`. It turns
 * the forward test (from < at <= to) round to to < at <= from, split at the loop's end when the
 * step wrapped. */
static inline int bm_anim_crossed_back(int from, int to, int at, int wrapped) {
    // Within the lap, the trigger lies between the new place and the old.
    if (!wrapped) {
        return to < at && at <= from;
    }

    // Across the loop's end, it lies at or before the old place, or after the new one.
    return at <= from || to < at;
}

/* Sets `out` to his velocity while strafing under a surface's gravity, where rac1's set_planar_vel
 * only moves him along his facing: `speed` along the stick's way in the world (`stick`, from
 * stick_target) laid flat on the surface (`normal`, its up). Returns 0, leaving `out` alone, when
 * the normal is unknown or the stick points along it. */
static inline int bm_surface_strafe_velocity(
    const float stick[3], const float normal[3], float speed, float out[3]
) {
    const float nn = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];

    // No normal: the probe found no ground.
    if (nn < 1e-6f) {
        return 0;
    }

    // The stick's way less its part along the normal lies on the surface.
    const float along = (stick[0] * normal[0] + stick[1] * normal[1] + stick[2] * normal[2]) / nn;
    float flat[3] =
        {stick[0] - along * normal[0], stick[1] - along * normal[1], stick[2] - along * normal[2]};
    const float len = sqrtf(flat[0] * flat[0] + flat[1] * flat[1] + flat[2] * flat[2]);

    // The stick points into or out of the surface: no way along it.
    if (len < 1e-4f) {
        return 0;
    }

    out[0] = flat[0] / len * speed;
    out[1] = flat[1] / len * speed;
    out[2] = flat[2] / len * speed;
    return 1;
}

/* Returns whether the gravity lets him strafe: normal gravity (mode 0), or a surface's gravity while
 * he is in none of the Magneboots' own states. rac1's Magneboots walk him in their own states, where
 * rac2's strafe has no place; a mod that plays magnetic surfaces as plain ground (Going Commando's
 * Gravity Boots) runs and jumps there in the plain states, under the surface's gravity (mode 1). */
static inline int bm_strafe_gravity_ok(int gravity_mode, int state) {
    // Normal gravity.
    if (gravity_mode == 0) {
        return 1;
    }

    // A surface's gravity, outside the Magneboots' walk, wrench swing and hop.
    return gravity_mode == 1 && state != RAC1_STATE_MAGNET_WALK && state != RAC1_STATE_MAGNET_WRENCH
           && state != RAC1_STATE_MAGNET_HOP;
}

#endif /* RAC1_ENHANCED_MOVEMENT_STRAFE_H */
