/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The game globals the mod reads and the game functions it calls, for each disc it supports.
 * Functions go by the name each disc's build of the port gives them (openrac_api.function).
 * - PAL v2.00 (SCES-50916, "pal"): rac1-decomp's addresses and names (include/names/). OpenRAC's
 *   PAL build names functions as rac1-decomp does.
 * - NTSC-U v1.00 (SCUS-97199, "ntsc"): the same globals and functions at the US addresses, from
 *   OpenRAC's port/game/rac1-ntsc/host/rac1_version.h, rac1-decomp's config/names_ntsc.tsv and
 *   config/overlays/us_map.tsv, and the NTSC decomp's C (DESIGN.md section 6).
 */
#ifndef RAC1_ENHANCED_MOVEMENT_DISCS_H
#define RAC1_ENHANCED_MOVEMENT_DISCS_H

#include <stdint.h>

/* One disc's addresses of the globals and names of the functions the mod uses. */
typedef struct bm_disc {
    const char* version; /* openrac_api.version */

    /* Globals of the executable, used as they are. */
    uint32_t hero; /* g_hero */
    uint32_t pad;  /* g_controller_state: the first pad's PadState */

    /* Globals of the executable, found through relocate_data (set_video_timing writes them). */
    uint32_t frame_scale_sq; /* g_frame_scale_sq: scales the turn spring coefficients */
    uint32_t frame_time;     /* g_frame_time: seconds per tick */
    uint32_t frame_time_sq;  /* g_frame_time_sq */
    uint32_t frame_scale;    /* frame_scale: scales rac1's tick counts (scale_ticks) */
    uint32_t cam_option;     /* opt_cam_yaw_rate: the camera speed option, 0 to 2 */

    /* Globals of level 0's program, found through relocate_level_data(0, ...). */
    uint32_t cinematic_mode; /* 0 in play, else a movie, a scene or the ship's entry */
    uint32_t camera_yaw;     /* yaw of the camera in use, which the stick is read against */
    uint32_t follow_cam;     /* g_current_follow_cam: the camera record in use (kind at +0x86) */
    uint32_t blend_weight;   /* follow_cam_turn_blend_weight: weight of the turn to his heading */
    uint32_t pose_table;     /* hero_eases: the hero's joint pose records, 0xB0 bytes each */

    /* The follow camera's gain, also in level 0's program (DESIGN.md section 8). */
    uint32_t cam_gain; /* follow_cam_setting_value: the turn gain, copied from the yaw rate table */
    uint32_t cam_matrix; /* player_camera_matrix: its rows; stick_target turns the stick by them, so
                            row 0 is the way the stick's forward goes */

    /* Game functions the mod calls, each one function in every level (us_map.tsv). */
    const char* orbit_clamp;    /* void (int): set_follow_camera_orbit_clamp_mode */
    const char* start_slide;    /* void (void): follow_camera_start_slide */
    const char* lateral_spring; /* void (float, float): follow_camera_set_lateral_spring */
    const char* play_sound;     /* int (int sound, int flags, u32 moby): Moby_PlaySound */
} bm_disc;

/* The supported discs, one row each. */
static const bm_disc k_bm_discs[] = {
    {
        "pal",
        0x0013F450u, /* g_hero */
        0x0013CA40u, /* g_controller_state */
        0x0015EE64u, /* g_frame_scale_sq */
        0x0015EE6Cu, /* g_frame_time */
        0x0015EE70u, /* g_frame_time_sq */
        0x0015EE60u, /* frame_scale */
        0x0015EEE4u, /* opt_cam_yaw_rate (follow_camera_update's vendor_stock[5]) */
        0x0015F6A8u, /* D_L00_0015F6A8 cinematic_mode */
        0x00166ED8u, /* D_L00_00166ED8 camera_yaw */
        0x00166F00u, /* D_L00_00166F00 g_current_follow_cam */
        0x0015F040u, /* D_L00_0015F040 follow_cam_turn_blend_weight */
        0x0017A780u, /* D_L00_0017A780 hero_eases */
        0x0015F044u, /* D_L00_0015F044 follow_cam_setting_value */
        0x001670D0u, /* D_L00_001670D0 player_camera_matrix */
        "func_L00_002E9A18",
        "func_L00_002E9AF8",
        "func_L00_002E9A40",
        "func_0022ED80",
    },
    {
        "ntsc",
        0x0013F350u,
        0x0013C940u,
        0x0015ED64u,
        0x0015ED6Cu,
        0x0015ED70u,
        0x0015ED60u,
        0x0015EDE4u, /* opt_cam_yaw_rate (NTSC decomp: rnc/game/options.h) */
        0x0015F5C4u,
        0x00166DD8u,
        0x00166E00u,
        0x0015EF40u,
        0x0017A680u,
        0x0015EF44u, /* D_L00_0015EF44 (NTSC decomp: follow_camera_update) */
        0x00166FD0u, /* player_camera_matrix (PAL's less 0x100, as camera_lateral_axis) */
        "FUN_L00_002e8568",
        "FUN_L00_002e8648",
        "FUN_L00_002e8590",
        "FUN_0022da68",
    },
};

#endif /* RAC1_ENHANCED_MOVEMENT_DISCS_H */
