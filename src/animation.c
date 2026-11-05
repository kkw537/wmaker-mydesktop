/* animation.c - frame pacing for time-based animations
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

#include "wconfig.h"

#include <X11/Xlib.h>

#include "WindowMaker.h"
#include "animation.h"

#ifdef USE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

/* used when the refresh rate cannot be queried */
#define DEFAULT_REFRESH_RATE	60
#define MIN_REFRESH_RATE	30
#define MAX_REFRESH_RATE	360

static unsigned int cached_interval = 0;

#ifdef USE_RANDR
/* refresh rate, in Hz, of the fastest active CRTC, or 0 if unknown */
static double queryRefreshRate(WScreen *scr)
{
	XRRScreenResources *sr;
	double best = 0;
	int i, j;

	if (!w_global.xext.randr.supported)
		return 0;

	sr = XRRGetScreenResourcesCurrent(dpy, scr->root_win);
	if (!sr)
		return 0;

	for (i = 0; i < sr->ncrtc; i++) {
		XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, sr, sr->crtcs[i]);

		if (!ci)
			continue;

		if (ci->mode != None) {
			for (j = 0; j < sr->nmode; j++) {
				XRRModeInfo *mi = &sr->modes[j];
				double rate;

				if (mi->id != ci->mode || mi->hTotal == 0 || mi->vTotal == 0)
					continue;

				rate = (double)mi->dotClock / ((double)mi->hTotal * (double)mi->vTotal);
				if (mi->modeFlags & RR_DoubleScan)
					rate /= 2;
				if (mi->modeFlags & RR_Interlace)
					rate *= 2;
				if (rate > best)
					best = rate;
				break;
			}
		}
		XRRFreeCrtcInfo(ci);
	}
	XRRFreeScreenResources(sr);

	return best;
}
#endif

unsigned int wAnimationFrameInterval(WScreen *scr)
{
	double rate = 0;

	if (cached_interval)
		return cached_interval;

#ifdef USE_RANDR
	rate = queryRefreshRate(scr);
#else
	(void)scr;
#endif
	if (rate < 1)
		rate = DEFAULT_REFRESH_RATE;
	else if (rate < MIN_REFRESH_RATE)
		rate = MIN_REFRESH_RATE;
	else if (rate > MAX_REFRESH_RATE)
		rate = MAX_REFRESH_RATE;

	cached_interval = (unsigned int)(1000000.0 / rate + 0.5);

	return cached_interval;
}

void wAnimationResetFrameRate(void)
{
	cached_interval = 0;
}

static void getTime(struct timespec *ts)
{
#ifdef CLOCK_MONOTONIC
	if (clock_gettime(CLOCK_MONOTONIC, ts) == 0)
		return;
#endif
	clock_gettime(CLOCK_REALTIME, ts);
}

void wAnimationStart(WAnimation *anim, WScreen *scr)
{
	getTime(&anim->start);
	anim->frame = 0;
	anim->interval = wAnimationFrameInterval(scr);
}

unsigned long wAnimationElapsed(const WAnimation *anim)
{
	struct timespec now;
	long sec, nsec;

	getTime(&now);
	sec = now.tv_sec - anim->start.tv_sec;
	nsec = now.tv_nsec - anim->start.tv_nsec;
	if (nsec < 0) {
		sec--;
		nsec += 1000000000L;
	}
	if (sec < 0)
		return 0;

	return (unsigned long)sec * 1000000UL + (unsigned long)(nsec / 1000);
}

float wAnimationProgress(const WAnimation *anim, unsigned long duration)
{
	unsigned long elapsed;

	if (duration == 0)
		return 1.0F;

	elapsed = wAnimationElapsed(anim);
	if (elapsed >= duration)
		return 1.0F;

	return (float)elapsed / (float)duration;
}

void wAnimationWaitFrame(WAnimation *anim)
{
	unsigned long elapsed = wAnimationElapsed(anim);
	unsigned long due;

	/* if drawing took longer than a frame, skip the frames that were missed
	 * instead of slowing the whole animation down */
	if (elapsed / anim->interval > anim->frame)
		anim->frame = elapsed / anim->interval;

	anim->frame++;
	due = anim->frame * anim->interval;
	if (due > elapsed)
		wusleep(due - elapsed);
}
