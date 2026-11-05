/* animation.h - frame pacing for time-based animations
 *
 *  Window Maker window manager
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef WMANIMATION_H_
#define WMANIMATION_H_

#include <stdint.h>
#include <time.h>

#include "screen.h"

/*
 * Animations are defined by their total duration, not by a number of
 * frames: every frame is drawn at the position matching the time elapsed
 * since the start, and frames are paced at the refresh rate of the
 * fastest active monitor. A 120Hz monitor therefore gets twice as many
 * frames as a 60Hz one, and both finish in the same time.
 *
 * When the X server supports the Present extension, a synchronised
 * animation (wAnimationStartSynced) waits for the vertical blank of the
 * monitor instead of sleeping, and its frames are timed with the time of
 * that vertical blank: each frame is drawn right after a vertical blank,
 * so it is shown exactly once and at a regular interval.
 */
typedef struct WAnimation {
	struct timespec start;
	unsigned long frame;		/* index of the frame on screen */
	unsigned int interval;		/* time between frames, in microseconds */
	unsigned long time;		/* time of the frame being drawn, in microseconds */

	/* synchronisation with the vertical blank, through Present */
	Bool synced;
	Window window;
	unsigned long eid;
	uint32_t serial;
	uint64_t msc;			/* vertical blank counter of the last frame */
	uint64_t start_ust;		/* time of the first vertical blank */
} WAnimation;

/* time between two frames, in microseconds, for the screen's monitors */
unsigned int wAnimationFrameInterval(WScreen *scr);

/* forget the cached refresh rate (the monitor configuration changed) */
void wAnimationResetFrameRate(void);

/* start an animation paced by a timer */
void wAnimationStart(WAnimation *anim, WScreen *scr);

/* start an animation synchronised with the vertical blank when possible,
 * it must be ended with wAnimationEnd() */
void wAnimationStartSynced(WAnimation *anim, WScreen *scr);

void wAnimationEnd(WAnimation *anim);

/* microseconds elapsed since the start, read from the clock */
unsigned long wAnimationElapsed(const WAnimation *anim);

/* time of the frame being drawn as a fraction of duration (in
 * microseconds), from 0 to 1 */
float wAnimationProgress(const WAnimation *anim, unsigned long duration);

/* wait until the next frame is due: the next vertical blank for a
 * synchronised animation, else sleep for one frame interval */
void wAnimationWaitFrame(WAnimation *anim);

#endif /* WMANIMATION_H_ */
