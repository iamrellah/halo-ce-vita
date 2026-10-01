/*
STATIC_CAMERA.H

header included in hcex build.
*/

#ifndef __STATIC_CAMERA_H
#define __STATIC_CAMERA_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "math/real_math.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

struct static_camera
{
	real_point3d position;
	long location;
	real_vector3d forward;
	real_vector3d up;
	real field_of_view;
	long timer;
	long flags;
	boolean initialized;
};

struct camera_command
{
	long flags;
	real_point3d position;
	real_vector3d offset;
	real depth;
	real field_of_view;
	real_vector3d forward;
	real_vector3d up;
	real_vector3d velocity;
	real timer;
	byte position_flags;
	byte offset_flags;
	byte distance_flags;
	byte field_of_view_flags;
	byte orientation_flags;
	byte pad51[3];
	real position_timer;
	real offset_timer;
	real distance_timer;
	real field_of_view_timer;
	real orientation_timer;
};
/* The director allocates observer_command with this full layout. */
typedef char camera_command_size_assert[sizeof(struct camera_command) == 0x68 ? 1 : -1];
typedef char camera_command_flags_offset_assert[offsetof(struct camera_command, position_flags) == 0x4c ? 1 : -1];
typedef char camera_command_timers_offset_assert[offsetof(struct camera_command, position_timer) == 0x54 ? 1 : -1];


/* ---------- prototypes/STATIC_CAMERA.C */

void static_camera_new(
	struct static_camera *camera,
	real_point3d const *position,
	long location,
	real_vector3d const *forward,
	real_vector3d const *up,
	real field_of_view,
	long timer,
	long flags);
void static_camera_update(
	struct static_camera *camera,
	void const *action,
	struct camera_command *result);

/* ---------- globals */

/* ---------- public code */

#endif // __STATIC_CAMERA_H
