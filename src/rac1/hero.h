/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Offsets into the game's structures that the mod reads and writes, the same on both discs. Each is
 * seen in rac1-decomp's code (include/structs/hero.h, pad.h, moby.h and the functions DESIGN.md
 * cites), except the roles of pose records 0 and 1, which were seen in play.
 */
#ifndef RAC1_ENHANCED_MOVEMENT_HERO_H
#define RAC1_ENHANCED_MOVEMENT_HERO_H

/* The hero (g_hero). */
enum {
    BM_HERO_VELOCITY = 0xE0,         /* the run's velocity, set by set_planar_vel */
    BM_HERO_FACING = 0x98,           /* motion.rot.z */
    BM_HERO_MOMENTUM = 0x150,        /* motion.momentum, a vector: idle slides out on it */
    BM_HERO_SPEED_XY = 0x160,        /* motion.speed_xy: over 2.7 the run exits to STOP */
    BM_HERO_FLAT_SPEED = 0x164,      /* motion.flat_speed */
    BM_HERO_STICK_DIR = 0x170,       /* the stick's way in the world (stick_target); turn_to turns
                                         to it, not to the move yaw, under a surface's gravity */
    BM_HERO_MOVE_YAW = 0x180,        /* motion.move_yaw: stick yaw plus camera yaw */
    BM_HERO_YAW_RATE = 0x184,        /* motion.yaw_rate */
    BM_HERO_YAW_ERROR = 0x188,       /* motion.yaw_error: move yaw minus facing, after the turn */
    BM_HERO_TARGET_SPEED = 0x190,    /* target_speed */
    BM_HERO_GROUND_SPEED = 0x194,    /* ground_speed */
    BM_HERO_STATE_TIMER = 0x198,     /* state_timer: ticks in the current state */
    BM_HERO_GROUND_NORMAL = 0x270,   /* ground_normal: the surface's up under a surface's gravity */
    BM_HERO_GROUND_DISTANCE = 0x2DC, /* ground_distance: height above the ground */
    BM_HERO_UNK308 = 0x308,          /* s16: magnet-ground mode, tested before the stick steers */
    BM_HERO_AIR_FRAMES = 0x30E,      /* s16: ticks off the ground */
    BM_HERO_TURNAROUND = 0x3BE,      /* s16 unk3BE: the run's turnaround flag */
    BM_HERO_PAST_APEX = 0x41E,       /* s16 past_jump_apex */
    BM_HERO_RAIL_LAND = 0x418,       /* s32 rail_land_frames: the wrench reads Square as on foot */
    BM_HERO_TAKEOFF_FRAME = 0x420,   /* takeoff_frame */
    BM_HERO_AIR_STICK_SPEED = 0x424, /* air_stick_speed */
    BM_HERO_APEX_FRAME = 0x440,      /* apex_anim_frame: jump animation frame at the apex */
    BM_HERO_TARGET_FRAME = 0x444,    /* target_anim_frame: jump animation frame at landing */
    BM_HERO_FRAMES_TO_LAND = 0x44C,  /* frames_to_land */
    BM_HERO_AIR_TARGET_SPEED = 0x454, /* air_target_speed: state 0xB's push along takeoff yaw */
    BM_HERO_UNK450 = 0x450,           /* s32 unk450: 3 for a high jump whose Square opens later */
    BM_HERO_DRIFT_SPEED = 0x458,      /* high_jump_drift_speed */
    BM_HERO_PUSH = 0x930,          /* f32 unk930: a push (a conveyor) added to the run's target */
    BM_HERO_MELEE_TARGET = 0xA54,  /* melee_target: the moby a swing or the slam aims at */
    BM_HERO_ATTACK_RECORD = 0xA60, /* s32 attack_record: the swing's row in attack_records */
    BM_HERO_ANIM_SPEED = 0xA90,    /* anim_speed */
    BM_HERO_ANIM_RATE = 0xA94,     /* anim_frame_rate: the current key's rate */
    BM_HERO_ANIM_FLAGS = 0xA98,    /* s32 anim_flags: 2 set when the animation ended this tick */
    BM_HERO_ANIM_CHANGING = 0xA9C, /* s32 anim_changing: a new animation is blending in */
    BM_HERO_ANIM_BLEND_CURVE = 0xAA0, /* s32: -1 unless a scripted blend runs */
    BM_HERO_ANIM_FRAME = 0xAA8,       /* f32 anim_frame: recomputed each tick */
    BM_HERO_ANIM_LOOP_FRAME = 0xAB0,  /* s32: -1 unless a loop is requested */
    BM_HERO_ANIM_QUEUED = 0xAB8,      /* s32 queued_frame_request: 0 unless a frame is queued */
    BM_HERO_ITEM_MOBY = 0x1090,       /* item slot 0's moby: 0 with no item in hand */
    BM_HERO_ITEM_THROWN = 0x10AA,  /* u8 slot 0's thrown: the wrench is out (launch_hero_wrench) */
    BM_HERO_ITEM_STATE = 0x10B4,   /* slot 0's state: 2 with the item in hand (hero_slot_item_id) */
    BM_HERO_ITEM_ID = 0x10B8,      /* item slot 0's item id */
    BM_HERO_ON_ICE = 0x12E2,       /* u8 on_ice_ground */
    BM_HERO_ON_SLIP = 0x12EA,      /* u8 on_slip_ground: the wrench takes no Square there */
    BM_HERO_STICK_X = 0x1D20,      /* stick x, positive right */
    BM_HERO_STICK_Y = 0x1D24,      /* stick y, positive down */
    BM_HERO_MOBY = 0x2080,         /* the hero's moby */
    BM_HERO_STATE = 0x2084,        /* state.current */
    BM_HERO_STEP = 0x2088,         /* state.step: in the run, 1 is the run band */
    BM_HERO_CONTROL_MODE = 0x208C, /* state.control_mode */
    BM_HERO_PREV_STATE = 0x2090,   /* state.prev */
    BM_HERO_VEHICLE_MODE = 0x20A4, /* u8: 0 on foot */
    BM_HERO_ITEM_ANIM = 0x20A8,    /* u8 item_anim_active */
    BM_HERO_ITEM_FLAG = 0x20AA,    /* u8 unk20AA: an item locks the turn */
    BM_HERO_GRAVITY_MODE = 0x20B3, /* u8: 0 under normal gravity */
    BM_HERO_STICK = 0x229C,        /* stick_magnitude, 0 to 1 */
};

/* The pad (PadState). */
enum {
    BM_PAD_ANALOG = 0x100,   /* f32[16]: [14] L2 pressure, [15] R2 pressure */
    BM_PAD_HIST_IDX = 0x18E, /* s16: the next slot of the 30-poll history */
    BM_PAD_HIST_COUNT = 0x190,
    BM_PAD_HELD = 0x1A0, /* pad_held */
    BM_PAD_PRESSED = 0x1A4,
    BM_PAD_RELEASED = 0x1A8,
    BM_PAD_PREV_HELD = 0x1AC,
    BM_PAD_RAW_HELD = 0x1B0,
    BM_PAD_RAW_PRESSED = 0x1B4,
    BM_PAD_RAW_RELEASED = 0x1B8,
    BM_PAD_HIST_BTN = 0x1E0, /* s32[30]: buttons pressed, per poll */
    BM_PAD_HIST_ANG = 0x258, /* f32[30]: stick angle, per poll */
    BM_PAD_LATCHED = 0x348,  /* unk348: held, latched */
    BM_PAD_HIST_LEN = 30,
};

/* Pad buttons, in DualShock bit order, and the bit the pad sets for a stick flick. */
enum {
    BM_PAD_L2 = 0x0001,
    BM_PAD_R2 = 0x0002,
    BM_PAD_R1 = 0x0008,
    BM_PAD_CROSS = 0x0040,
    BM_PAD_SQUARE = 0x0080,
    BM_PAD_FLICK = 0x10000,
};

/* Offsets in a moby, its class and its animation sequences. */
enum {
    BM_MOBY_CLASS = 0x24,      /* pclass */
    BM_MOBY_FRAME = 0x50,      /* u8: the key the blend starts from */
    BM_MOBY_NEXT = 0x51,       /* u8: the key it blends to (prev_frame) */
    BM_MOBY_SEQ = 0x52,        /* u8 */
    BM_MOBY_PREV_SEQ = 0x53,   /* u8: equals seq unless a sequence blend is pending */
    BM_MOBY_ANIM_TIME = 0x54,  /* f32: 0 to 1 between the keys */
    BM_MOBY_FRAME_DATA = 0x68, /* the key's frame data, whose first float is its rate */
    BM_MOBY_NEXT_DATA = 0x6C,
    BM_CLASS_SEQS = 0x48,        /* in the class: the sequences, by id */
    BM_SEQ_FRAME_COUNT = 0x10,   /* u8 */
    BM_SEQ_TRIGGER_COUNT = 0x12, /* u8 */
    BM_SEQ_FRAMES = 0x1C, /* frame data pointers, then sound triggers (sound | place << 16) */
    BM_SEQ_WALK = 3,      /* the walk, which run band 0 plays */
    BM_SEQ_RUN = 4,       /* the run */
};

/* The follow camera's moby and the camera record in use, then the hero's joint pose records. */
enum {
    /* In the camera's moby: its variables, with the recentre timers at +0x12, +0x14 and +0x16. */
    BM_CAMERA_VARS = 0x70,
    /* In the variables: the turn flags (+0x1A8 + 0x10, follow_camera_read_stick). 1 stops the stick
     * turning the camera, 2 stops it pitching. */
    BM_CAMERA_TURN_FLAGS = 0x1B8,
    BM_CAMERA_KIND = 0x86,     /* s16, in the record: 0 for the follow camera */
    BM_POSE_RECORD = 0xB0,     /* size of one pose record */
    BM_POSE_TARGET_YAW = 0x68, /* the target's z, a yaw (targets start at +0x60) */
    BM_POSE_ACCEL = 0xA4,      /* the spring's accel */
    BM_POSE_DAMP = 0xA8,       /* the spring's damping */
    /* Record 0 is his pelvis (legs and tail), record 1 the body above it, as in rac2 (seen in play:
     * PLAYTEST.md). */
    BM_POSE_HIPS = 0,
    BM_POSE_BODY = 1,
};

#endif /* RAC1_ENHANCED_MOVEMENT_HERO_H */
