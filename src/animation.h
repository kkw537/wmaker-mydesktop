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

#include <time.h>

#include "screen.h"

/*
 * Animations are defined by their total duration, not by a number of
 * frames: every frame is drawn at the position matching the time elapsed
 * since the start, and frames are paced at the refresh rate of the
 * fastest active monitor. A 120Hz monitor therefore gets twice as many
 * frames as a 60Hz one, and both finish in the same time.
 */
typedef struct WAnimation {
	struct timespec start;
	unsigned long frame;		/* index of the frame on screen */
	unsigned int interval;		/* time between frames, in microseconds */
} WAnimation;

/* time between two frames, in microseconds, for the screen's monitors */
unsigned int wAnimationFrameInterval(WScreen *scr);

/* forget the cached refresh rate (the monitor configuration changed) */
void wAnimationResetFrameRate(void);

void wAnimationStart(WAnimation *anim, WScreen *scr);

/* microseconds elapsed since wAnimationStart() */
unsigned long wAnimationElapsed(const WAnimation *anim);

/* elapsed time as a fraction of duration (in microseconds), from 0 to 1 */
float wAnimationProgress(const WAnimation *anim, unsigned long duration);

/* sleep until the next frame is due */
void wAnimationWaitFrame(WAnimation *anim);

#endif /* WMANIMATION_H_ */
