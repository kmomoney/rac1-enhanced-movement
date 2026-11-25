/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * rac1's movement numbers that the mod compares with or replaces. Speeds, accelerations and turn
 * limits are per second; rac1 multiplies them by g_frame_time or g_frame_time_sq. Tick and poll
 * counts are before scale_ticks (bm_scale_ticks), which raises them on PAL. Each number comes from
 * the rac1-decomp function named above it (PAL names; DESIGN.md sections 2 and 3), or, where marked
 * as level data, from the game's level programs.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_RAC1_H
#define RAC1_ENHANCED_MOVEMENT_RAC1_H

/* hero_update_movement's run: speed_step(7.5 * k, 8.5), braking at 17 in a turnaround. */
#define RAC1_RUN_BRAKE 8.5f
#define RAC1_TURNAROUND_BRAKE 17.0f
/* The idle group's speed_step(0, down): idle 12.6, STOP 12 * t, crouch 6.48 (1.7 on ice). */
#define RAC1_IDLE_BRAKE 12.6f
#define RAC1_STOP_BRAKE 12.0f
#define RAC1_CROUCH_BRAKE 6.48f
/* HeroEnterMoveState's jump case: air_stick_speed is 5.7, or 3.5 in the double jump. */
#define RAC1_AIR_STICK_SPEED 5.7f
#define RAC1_DOUBLE_JUMP_AIR_SPEED 3.5f
/* hero_update_transitions, in the run: STOP when speed_xy is over 2.7, else idle after 25 ticks. */
#define RAC1_EXIT_STOP_SPEED 2.7f
#define RAC1_EXIT_IDLE_TICKS 25
/* hero_move_state_velocity, state 0xB: the drift eases toward 4.2 at an acceleration of 8, or
 * toward 0 within 12 ticks of landing once past the apex. */
#define RAC1_FLIP_DRIFT 4.2f
#define RAC1_FLIP_DRIFT_STEP 8.0f
#define RAC1_FLIP_DRIFT_LANDING_TICKS 12
/* hero_update_transitions: X within 9 ticks jumps in the run, within 7 standing (1 with
 * jump_buffer_lockout). */
#define RAC1_RUN_JUMP_WINDOW 9
#define RAC1_IDLE_JUMP_WINDOW 7
/* hero_update_transitions, in the run: X jumps only within 4 ticks of leaving the ground. */
#define RAC1_RUN_JUMP_AIR_TICKS 4

/* ---- The wrench ---- */

/* process_hero_equipped_item_input (DESIGN.md section 5): Square starts a swing within 7 polls when
 * standing or running. In a jump it counts after the 10th tick, within 10 polls (18 in the high and
 * double jumps). In a swing the window is (frame - first) * 2 polls, 1 to 15. The throw repeats
 * after 30 ticks, within 15 polls. The crouch's throw (hero_update_transitions) takes 15. */
#define RAC1_WRENCH_WINDOW 7
#define RAC1_WRENCH_AIR_TICKS 10
#define RAC1_WRENCH_AIR_WINDOW 10
#define RAC1_WRENCH_AIR_LATE_WINDOW 18
#define RAC1_WRENCH_CHAIN_WINDOW 15
#define RAC1_THROW_AGAIN_TICKS 30
/* hero_update_movement's swings, slam and throw: the lock-on turn_to(0.05, 0.2, 15.01). */
#define RAC1_WRENCH_TURN_ACCEL 0.05f
#define RAC1_WRENCH_TURN_DAMP 0.2f
#define RAC1_WRENCH_TURN_LIMIT 15.009831f
/* attack_records (rac1-decomp D_L00_0017BD28; NTSC-U 0x0017BC28): the frames of the wrench
 * animation the transitions compare with. +0xC first (the windows count from it), +0x10 open
 * (Square chains from it), +0x14 jump (X cancels after it), +0x18 exit (the swing ends after it).
 * Records 0 to 2 are the combo's three swings, 3 is the throw (state 0x15). Level data, not code:
 * read from the PAL disc's level programs, the same in all 14 copies, and from the NTSC-U game at
 * run time in 8 levels (DESIGN.md 5.1). */
#define RAC1_REC0_FIRST 19
#define RAC1_REC0_OPEN 26
#define RAC1_REC0_JUMP 24
#define RAC1_REC0_EXIT 31
#define RAC1_REC1_FIRST 7
#define RAC1_REC1_OPEN 12
#define RAC1_REC1_JUMP 13
#define RAC1_REC1_EXIT 23
#define RAC1_REC2_FIRST 16
#define RAC1_REC2_OPEN 23
#define RAC1_REC2_JUMP 19
#define RAC1_REC2_EXIT 21
#define RAC1_REC3_FIRST 83
#define RAC1_REC3_OPEN 88
#define RAC1_REC3_JUMP 87
#define RAC1_REC3_EXIT 91
/* The wrench's item id (items[0].item_id). */
#define RAC1_ITEM_WRENCH 8

/* ---- The follow camera ---- */

/* follow_camera_yaw_rate_table (rac1-decomp D_L00_00161E80; NTSC-U 0x00161DB0; DESIGN.md
 * section 8): the follow camera's yaw per tick at full right stick, in radians, for each camera
 * speed option (opt_cam_yaw_rate, 0 to 2, default 1): 1.0, 1.3 and 1.6 degrees, not scaled at
 * 50 Hz. follow_camera_update copies the entry to follow_cam_setting_value each tick. Level data:
 * the NTSC decomp declares two entries but the game has three, read in the running game on both
 * discs (PLAYTEST.md). */
#define RAC1_CAMERA_SPEED_SLOW 0.017453292f
#define RAC1_CAMERA_SPEED_MEDIUM 0.02268928f
#define RAC1_CAMERA_SPEED_FAST 0.027925268f
/* follow_camera_turn_and_pitch: right stick y past 0.3 (rescaled by 1 / 0.7) aims the pitch within
 * 40 degrees. The aim moves at most 0.02 a tick away from the centre and 0.01 back
 * (follow_camera_pitch_input_tracking, return_step_limit), and the pitch at most 1.75 degrees a
 * tick (follow_camera_pitch_angle_step_limit). Level data, read in the running game. */
#define RAC1_CAMERA_PITCH_DEAD 0.3f
#define RAC1_CAMERA_PITCH_TRACKING 0.02f
#define RAC1_CAMERA_PITCH_RETURN 0.01f
#define RAC1_CAMERA_PITCH_STEP 0.030543262f
/* follow_camera_orbit: the eye's yaw and pitch follow the orbit the stick turns, through
 * rope_spring_turn at 0.015 / 0.2 (orbit_yaw_spring_stiffness and damping, orbit_pitch_spring_*).
 * The eye reaches half the stick's rate after 11 ticks and 90% after 24, and coasts about 0.19
 * seconds after the stick is let go. Level data, read in the running game. */
#define RAC1_CAMERA_SPRING_ACCEL 0.015f
#define RAC1_CAMERA_SPRING_DAMP 0.2f

/* ---- Clank ---- */

/* hero_update_movement, Clank's run (0x44 and 0x50; DESIGN.md section 9): stick_target(3.0), the
 * stick times 3 units a second, and speed_step(7.5 * k, 8.5) with k = 1 - 0.3 * |facing - move
 * yaw|. The turn, with t = stick + 0.35, is turn_to(0.035 t, 0.15, 9.599311 t) under half the run
 * speed and turn_to(0.008 t, 0.15, 9.948377 t) above. He never moves along the stick:
 * HeroEnterMoveState clears unk3B8 for 0x44 and nothing sets it. */
#define RAC1_CLANK_RUN_SPEED 3.0f
#define RAC1_CLANK_RUN_ACCEL 7.5f
#define RAC1_CLANK_RUN_BRAKE 8.5f
#define RAC1_CLANK_RUN_K_PER_RADIAN 0.3f
#define RAC1_CLANK_TURN_STICK_BIAS 0.35f
#define RAC1_CLANK_TURN_SLOW_SHARE 0.5f
#define RAC1_CLANK_TURN_SLOW_ACCEL 0.035f
#define RAC1_CLANK_TURN_SLOW_LIMIT 9.599311f
#define RAC1_CLANK_TURN_FAST_ACCEL 0.008f
#define RAC1_CLANK_TURN_FAST_LIMIT 9.948377f
#define RAC1_CLANK_TURN_DAMP 0.15f
/* Clank's idle (0x43): speed_step(0, 12.6) with the velocity cleared, so he stops when the stick is
 * let go. His punch (0x51): speed_step(37, 28), the wrench's lock-on turn_to(0.05, 0.2, 15.01), and
 * a lunge of 3.5 units a second under frame 5.5. His glide (0x4F): stick_target(2.0),
 * turn_to(0.04, 0.2, 8.726646), speed_step(15, 7). His fall (0x45): stick_target(3.0). */
#define RAC1_CLANK_IDLE_BRAKE 12.6f
#define RAC1_CLANK_PUNCH_ACCEL 37.0f
#define RAC1_CLANK_PUNCH_BRAKE 28.0f
#define RAC1_CLANK_GLIDE_SPEED 2.0f
#define RAC1_CLANK_GLIDE_TURN_LIMIT 8.726646f
/* HeroEnterMoveState, Clank's jump (0x49): height 1.3 tapped and 1.35 held, takeoff after
 * scale_ticks(5), the animation's apex at frame 18 and landing at 27, gravity 25, and air stick
 * speed 2.0 (3.0 in the fall, as in the run). */
#define RAC1_CLANK_JUMP_HEIGHT 1.3f
#define RAC1_CLANK_JUMP_HEIGHT_HELD 1.35f
#define RAC1_CLANK_TAKEOFF 5
#define RAC1_CLANK_APEX_FRAME 18.0f
#define RAC1_CLANK_TARGET_FRAME 27.0f
#define RAC1_CLANK_JUMP_GRAVITY 25.0f
#define RAC1_CLANK_AIR_STICK_SPEED 2.0f
/* hero_update_transitions, Clank's idle and run: X within scale_ticks(8) jumps and Square within
 * scale_ticks(9) punches. Landing from a jump (past the apex, under 0.2 above the ground), Square
 * within scale_ticks(5) punches. */
#define RAC1_CLANK_JUMP_WINDOW 8
#define RAC1_CLANK_SQUARE_WINDOW 9
#define RAC1_CLANK_LAND_SQUARE_WINDOW 5
#define RAC1_CLANK_LAND_SQUARE_HEIGHT 0.2f
/* attack_records 7 and 8, Clank's two punches. HeroEnterMoveState's 0x51 takes record 7 + chain,
 * where chain picks the other record while the last punch's frame is under +0x8. The windows count
 * from +0xC, Square chains from +0x10, X jumps out after +0x14 within (frame - first) * 2 polls, he
 * stands after +0x18 (the punch animation plays on in idle) and hits from +0x1C to +0x20. Level
 * data from the PAL disc's levels 0, 6, 10, 15 and 17 and the NTSC-U game's level 0 (DESIGN.md
 * 9.1); it is the same in each, and the same as rac2's (rac2.h). +0x10 (12) lies past +0x18 (10),
 * so the next punch is chained from idle, whose Square window takes it under punch frame 16. */
#define RAC1_CLANK_REC_A 7
#define RAC1_CLANK_REC_B 8
#define RAC1_CLANK_REC_CHAIN 16
#define RAC1_CLANK_REC_FIRST 3
#define RAC1_CLANK_REC_OPEN 12
#define RAC1_CLANK_REC_JUMP 8
#define RAC1_CLANK_REC_EXIT 10
#define RAC1_CLANK_REC_HIT_FROM 3
#define RAC1_CLANK_REC_HIT_TO 6

/* Vehicle modes (HeroVehicleMode, +0x20A4): Clank alone, and Giant Clank. */
enum {
    RAC1_VEHICLE_CLANK = 1,
    RAC1_VEHICLE_GIANT_CLANK = 2,
};

/* Clank's states (HeroStateId's VEHICLE_MODE_1_* and the unnamed ones his transitions enter). */
enum {
    RAC1_STATE_CLANK_IDLE = 0x43,
    RAC1_STATE_CLANK_RUN = 0x44,
    RAC1_STATE_CLANK_FALL = 0x45,
    RAC1_STATE_CLANK_JUMP = 0x49,
    RAC1_STATE_CLANK_LEDGE_JUMP = 0x4C,
    RAC1_STATE_CLANK_GLIDE = 0x4F,
    RAC1_STATE_CLANK_RUN_ALT = 0x50, /* shares his run's code and transitions; nothing enters it */
    RAC1_STATE_CLANK_PUNCH = 0x51,
};

/* Hero states (enums/hero_state.h). */
enum {
    RAC1_STATE_IDLE = 0x0,
    RAC1_STATE_LOOK = 0x1,
    RAC1_STATE_RUN = 0x2,
    RAC1_STATE_STOP = 0x3,
    RAC1_STATE_CROUCH = 0x4,
    RAC1_STATE_FALL = 0x6,
    RAC1_STATE_JUMP = 0x7,
    RAC1_STATE_RUN_JUMP = 0x9,
    RAC1_STATE_PACK_JUMP = 0xA,
    RAC1_STATE_HIGH_JUMP = 0xB,
    RAC1_STATE_LAND = 0x8,
    RAC1_STATE_DOUBLE_JUMP = 0xE,
    RAC1_STATE_PACK_LONG_JUMP = 0x10,
    RAC1_STATE_WRENCH_COMBO = 0x13,
    RAC1_STATE_WRENCH_SLAM = 0x14,
    RAC1_STATE_CROUCH_SWING = 0x15, /* the throw */
    RAC1_STATE_ITEM_BUTTON_RELEASE_WAIT = 0x17,
    RAC1_STATE_LOOK_RELEASE_WAIT = 0x1E,
    RAC1_STATE_ITEM_RELEASE_FALL = 0x2D,
    RAC1_STATE_MAGNET_WALK = 0x3F, /* the Magneboots' own walk, its wrench swing and its hop */
    RAC1_STATE_MAGNET_WRENCH = 0x70,
    RAC1_STATE_MAGNET_HOP = 0x71,
    RAC1_STATE_RUN_WADING = 0x73,
    RAC1_STATE_OWN_STRAFE = 0x81,
};

/* Control modes (HeroState.control_mode) on foot, where the strafe may take L2/R2. */
enum {
    RAC1_MODE_IDLE = 0x0,
    RAC1_MODE_RUN = 0x1,
    RAC1_MODE_FALL = 0x2,
    RAC1_MODE_JUMP = 0x4,
    RAC1_MODE_LAND = 0x5,
    RAC1_MODE_ATTACK = 0x6,
    RAC1_MODE_ITEM_RELEASE_WAIT = 0x8,
    RAC1_MODE_CROUCH = 0xC,
};

#endif /* RAC1_ENHANCED_MOVEMENT_RAC1_H */
