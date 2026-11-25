/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests the strafe decisions (src/rac1/strafe.h) against values worked out by hand from rac2's
 * code: the stick sectors, the backpedal, the side timers over a run of ticks, the flip, the flip
 * drift landing on rac2's value after rac1's easing, the twist across the wrap, and the backward
 * step of a looping animation.
 */
#include "check.h"
#include "rac1/strafe.h"

/* Tests the stick's sectors and the stick against the facing. */
static void sectors(void) {
    // Down is back, right is right, left is left, up is forward (the pad's y grows downward).
    CHECK_EQ(bm_stick_sector(0.0f, 1.0f), BM_BACK);
    CHECK_EQ(bm_stick_sector(1.0f, 0.0f), BM_RIGHT);
    CHECK_EQ(bm_stick_sector(-1.0f, 0.0f), BM_LEFT);
    CHECK_EQ(bm_stick_sector(0.0f, -1.0f), BM_FORWARD);

    // atan2(0.70, 0.71) is 44.6 degrees: 45.4 from down, so not back, but 44.6 from right.
    CHECK_EQ(bm_stick_sector(0.71f, 0.70f), BM_RIGHT);

    // atan2(0.71, 0.70) is 45.4 degrees: 44.6 from down, which is checked first, so back.
    CHECK_EQ(bm_stick_sector(0.70f, 0.71f), BM_BACK);

    // He backpedals with the stick back and past 0.15.
    CHECK_EQ(bm_backpedal(BM_BACK, 0.2f), 1);
    CHECK_EQ(bm_backpedal(BM_BACK, 0.1f), 0);
    CHECK_EQ(bm_backpedal(BM_RIGHT, 1.0f), 0);

    // Against a facing of 1.0: behind, right (a smaller yaw), left (a larger yaw) and ahead.
    CHECK_EQ(bm_stick_vs_facing(1.0f + BM_PI, 1.0f), BM_BACK);
    CHECK_EQ(bm_stick_vs_facing(1.0f - BM_HALF_PI, 1.0f), BM_RIGHT);
    CHECK_EQ(bm_stick_vs_facing(1.0f + BM_HALF_PI, 1.0f), BM_LEFT);
    CHECK_EQ(bm_stick_vs_facing(1.2f, 1.0f), BM_FORWARD);
}

/* Tests the side timers: right after left comes back in quarters, left after right reverses. */
static void sides(void) {
    bm_sides s = {0, 0};

    // Going left with no timer running keeps the speed and sets the left timer.
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), 1.0, TOL);
    CHECK_EQ(s.left, 5);

    // Going right next, the left timer first counts down to 4: (4 - 4) / 4 = 0. Then 3, 2 and 1
    // give 0.25, 0.5 and 0.75, and 0 gives 1.
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 0.0, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 0.5, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 0.75, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_RIGHT, 1), 1.0, TOL);

    // Going left again, the right timer (5) counts 4, 3, 2 and 1 with -0.25 each tick, then 1.
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), -0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), -0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), -0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), -0.25, TOL);
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 1), 1.0, TOL);

    // When not on foot the factor is 1 and no timer is set.
    s.left = 0;
    s.right = 0;
    CHECK_NEAR(bm_side_factor(&s, BM_LEFT, 0), 1.0, TOL);
    CHECK_EQ(s.left, 0);

    // Going forward or back sets neither timer.
    CHECK_NEAR(bm_side_factor(&s, BM_BACK, 1), 1.0, TOL);
    CHECK_EQ(s.left + s.right, 0);
}

/* Tests the turn to the camera, the standing start, the flip and the twist. */
static void moves(void) {
    // At 0.2 rad off he is still turning, against 10 degrees (0.17453295); at 0.1 he is not.
    CHECK_EQ(bm_strafe_turning(0.2f, 0.0f, RAC2_STRAFE_START_ANGLE), 1);
    CHECK_EQ(bm_strafe_turning(0.1f, 0.0f, RAC2_STRAFE_START_ANGLE), 0);

    // The standing start comes 0.5 rad off with the stick at rest, but not at stick 0.5, where rac1
    // starts by itself, nor at 0.1 rad off.
    CHECK_EQ(bm_strafe_start(0.5f, 0.0f, 0.0f), 1);
    CHECK_EQ(bm_strafe_start(0.5f, 0.0f, 0.5f), 0);
    CHECK_EQ(bm_strafe_start(0.1f, 0.0f, 0.0f), 0);

    // X flips with the stick past 0.9 and not forward.
    CHECK_EQ(bm_flip(0.95f, BM_LEFT), 1);
    CHECK_EQ(bm_flip(0.95f, BM_BACK), 1);
    CHECK_EQ(bm_flip(0.95f, BM_FORWARD), 0);
    CHECK_EQ(bm_flip(0.85f, BM_LEFT), 0);

    // Going 1.0 while facing -3.0 twists 4.0 - 2 pi = -2.2831853, the short way round.
    CHECK_NEAR(bm_twist(1.0f, -3.0f), -2.2831853, 1e-5);
    CHECK_NEAR(bm_twist(0.5f, 0.2f), 0.3, 1e-6);
    CHECK_EQ(bm_twist_on(0.5f, RAC1_STATE_RUN, 0), 1);
    CHECK_EQ(bm_twist_on(0.1f, RAC1_STATE_RUN, 0), 0);
    CHECK_EQ(bm_twist_on(0.5f, RAC1_STATE_HIGH_JUMP, 0), 0);
    CHECK_EQ(bm_twist_on(0.5f, RAC1_STATE_RUN, 1), 0);
}

/* Tests the flip drift: after the preset, rac1's easing lands on rac2's value. */
static void drift(void) {
    const bm_tick t = bm_tick_ntsc();
    const float step1 = RAC1_FLIP_DRIFT_STEP * t.ft2;
    float d = bm_flip_drift_start(&t);
    float pre;

    // It starts at 5 / 60 = 0.0833333.
    CHECK_NEAR(d, 0.0833333358, TOL);

    // At full stick rac2 holds 5 / 60. The preset is 0.0833333 + 8 / 3600 = 0.0855556, which rac1
    // eases toward 4.2 / 60 = 0.07 by 8 / 3600, back to 0.0833333.
    pre = bm_flip_drift_preset(d, 1.0f, 0, &t);
    CHECK_NEAR(pre, 0.0855555580, TOL);
    CHECK_NEAR(bm_approach(pre, RAC1_FLIP_DRIFT * t.ft, step1), 0.0833333358, TOL);

    // At stick 0.3 rac2's goal is the floor 2 / 60 = 0.0333333. One step of 37 / 3600 = 0.0102778
    // gives 0.0730556, over rac1's 0.07, so the preset is 0.0752778 and rac1 lands on 0.0730556.
    pre = bm_flip_drift_preset(d, 0.3f, 0, &t);
    d = bm_approach(pre, RAC1_FLIP_DRIFT * t.ft, step1);
    CHECK_NEAR(d, 0.0730555570, TOL);

    // On the next tick rac2 gives 0.0627778, now under rac1's 0.07, so the preset is 0.0605556,
    // which rac1 eases up to 0.0627778.
    pre = bm_flip_drift_preset(d, 0.3f, 0, &t);
    CHECK_NEAR(pre, 0.0605555574, TOL);
    CHECK_NEAR(bm_approach(pre, RAC1_FLIP_DRIFT * t.ft, step1), 0.0627777791, TOL);

    // Near landing rac1 eases to 0. From 0.0627778 rac2 gives 0.0525, so the preset is 0.0547222.
    pre = bm_flip_drift_preset(0.0627778f, 0.3f, 1, &t);
    CHECK_NEAR(bm_approach(pre, 0.0f, step1), 0.0525000, TOL);
}

/* Tests the backward step of a looping animation and the sound triggers it passes. */
static void anim(void) {
    const float even[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float mixed[4] = {1.0f, 2.0f, 0.5f, 1.0f};
    bm_anim a;

    // Within a key, 0.6 back by 0.5 is 0.1.
    a.time = 0.6f;
    a.frame = 2;
    a.next = 3;
    CHECK_EQ(bm_anim_step_back(&a, 0.5f, even, 4), 0);
    CHECK_NEAR(a.time, 0.1, TOL);

    // Past a key, 0.3 - 0.5 = -0.2 becomes 0.8 of key 1, toward key 2.
    a.time = 0.3f;
    a.frame = 2;
    a.next = 3;
    CHECK_EQ(bm_anim_step_back(&a, 0.5f, even, 4), 1);
    CHECK_EQ(a.frame, 1);
    CHECK_EQ(a.next, 2);
    CHECK_NEAR(a.time, 0.8, TOL);

    // With rates, at key 1 (rate 2) 0.1 - 0.5 x 2 = -0.9, which at key 0 (rate 1) is -0.9 / 2 + 1 =
    // 0.55.
    a.time = 0.1f;
    a.frame = 1;
    a.next = 2;
    bm_anim_step_back(&a, 0.5f, mixed, 4);
    CHECK_EQ(a.frame, 0);
    CHECK_NEAR(a.time, 0.55, TOL);

    // Across the loop's start, 0.1 - 0.5 at key 0 becomes 0.6 of key 3, toward key 0.
    a.time = 0.1f;
    a.frame = 0;
    a.next = 1;
    bm_anim_step_back(&a, 0.5f, even, 4);
    CHECK_EQ(a.frame, 3);
    CHECK_EQ(a.next, 0);
    CHECK_NEAR(a.time, 0.6, TOL);

    // Key 2 at 0.5 is place 40.
    a.time = 0.5f;
    a.frame = 2;
    CHECK_EQ(bm_anim_place(&a), 40);

    // Stepping from 40 back to 30 passes a trigger at 35 or 40, but not one at 30 or 41.
    CHECK_EQ(bm_anim_crossed_back(40, 30, 35, 0), 1);
    CHECK_EQ(bm_anim_crossed_back(40, 30, 40, 0), 1);
    CHECK_EQ(bm_anim_crossed_back(40, 30, 30, 0), 0);
    CHECK_EQ(bm_anim_crossed_back(40, 30, 41, 0), 0);

    // Stepping from 5 back across the start to 60 passes 3 and 62, but not 30.
    CHECK_EQ(bm_anim_crossed_back(5, 60, 3, 1), 1);
    CHECK_EQ(bm_anim_crossed_back(5, 60, 62, 1), 1);
    CHECK_EQ(bm_anim_crossed_back(5, 60, 30, 1), 0);
}

/* Tests the gravity's part: normal gravity, and a surface's outside the Magneboots' own states. */
static void gravity(void) {
    // Normal gravity strafes in any state.
    CHECK_EQ(bm_strafe_gravity_ok(0, RAC1_STATE_RUN), 1);
    CHECK_EQ(bm_strafe_gravity_ok(0, RAC1_STATE_MAGNET_WALK), 1);

    // A surface's gravity in the plain run, idle and jump (Gravity Boots played as plain ground).
    CHECK_EQ(bm_strafe_gravity_ok(1, RAC1_STATE_RUN), 1);
    CHECK_EQ(bm_strafe_gravity_ok(1, RAC1_STATE_IDLE), 1);
    CHECK_EQ(bm_strafe_gravity_ok(1, RAC1_STATE_JUMP), 1);

    // The Magneboots' own walk, wrench swing and hop do not.
    CHECK_EQ(bm_strafe_gravity_ok(1, RAC1_STATE_MAGNET_WALK), 0);
    CHECK_EQ(bm_strafe_gravity_ok(1, RAC1_STATE_MAGNET_WRENCH), 0);
    CHECK_EQ(bm_strafe_gravity_ok(1, RAC1_STATE_MAGNET_HOP), 0);

    // Any other gravity mode does not either.
    CHECK_EQ(bm_strafe_gravity_ok(2, RAC1_STATE_RUN), 0);
}

/* Tests the velocity along the stick on a surface. */
static void surface(void) {
    float v[3] = {9.0f, 9.0f, 9.0f};

    // On a wall facing +x (its up is +x), a stick up and to the side keeps both parts, at speed 2.
    const float wall[3] = {1.0f, 0.0f, 0.0f};
    const float stick[3] = {0.5f, 0.0f, 1.0f};
    CHECK_EQ(bm_surface_strafe_velocity(stick, wall, 2.0f, v), 1);
    CHECK_NEAR(v[0], 0.0f, 1e-5f);
    CHECK_NEAR(v[1], 0.0f, 1e-5f);
    CHECK_NEAR(v[2], 2.0f, 1e-5f);

    // A sideways stick on the floor stays sideways, whatever the normal's length.
    const float floor2[3] = {0.0f, 0.0f, 2.0f};
    const float side[3] = {0.0f, -0.3f, 0.4f};
    CHECK_EQ(bm_surface_strafe_velocity(side, floor2, 1.0f, v), 1);
    CHECK_NEAR(v[1], -1.0f, 1e-5f);
    CHECK_NEAR(v[2], 0.0f, 1e-5f);

    // No normal, or a stick straight into the surface, leaves the velocity alone.
    const float none[3] = {0.0f, 0.0f, 0.0f};
    v[0] = 7.0f;
    CHECK_EQ(bm_surface_strafe_velocity(side, none, 1.0f, v), 0);
    CHECK_EQ(bm_surface_strafe_velocity(wall, wall, 1.0f, v), 0);
    CHECK_NEAR(v[0], 7.0f, 0.0f);
}

int main(void) {
    sectors();
    sides();
    moves();
    drift();
    anim();
    gravity();
    surface();
    CHECK_DONE("strafe");
}
