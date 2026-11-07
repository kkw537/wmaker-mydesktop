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

#include <X11/extensions/dpms.h>

#ifdef USE_XPRESENT
#include <poll.h>
#include <X11/extensions/Xpresent.h>
#endif

/* used when the refresh rate cannot be queried */
#define DEFAULT_REFRESH_RATE	60
#define MIN_REFRESH_RATE	30
#define MAX_REFRESH_RATE	360

/* longest wait for a vertical blank before the animation is given up, in
 * microseconds (the monitor may have been switched off meanwhile) */
#define VBLANK_TIMEOUT		100000

/* how much longer than its duration an animation may run, whatever the
 * time of its frames says, in microseconds */
#define ANIMATION_OVERRUN	200000

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

Bool wAnimationDisplayOff(void)
{
	static int dpms_supported = -1;
	CARD16 level;
	BOOL enabled;

	if (dpms_supported < 0) {
		int event_base, error_base;

		dpms_supported = DPMSQueryExtension(dpy, &event_base, &error_base) && DPMSCapable(dpy);
	}
	if (!dpms_supported)
		return False;

	if (!DPMSInfo(dpy, &level, &enabled) || !enabled)
		return False;

	return level != DPMSModeOn;
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
	anim->time = 0;
	anim->interval = wAnimationFrameInterval(scr);
	anim->synced = False;
	anim->window = None;
	anim->eid = 0;
	anim->serial = 0;
	anim->msc = 0;
	anim->ust = 0;
	anim->start_ust = 0;
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
	if (duration == 0 || anim->time >= duration)
		return 1.0F;

	/* the callers keep the server grabbed while they animate: whatever
	 * went wrong with the time of the frames, end in bounded time */
	if (wAnimationElapsed(anim) >= duration + ANIMATION_OVERRUN)
		return 1.0F;

	return (float)anim->time / (float)duration;
}

#ifdef USE_XPRESENT
/* major opcode of the Present extension, 0 when it is not usable */
static int present_opcode = -1;

static Bool presentSupported(void)
{
	if (present_opcode < 0) {
		int opcode, event_base, error_base;
		int major = 1, minor = 0;

		if (XPresentQueryExtension(dpy, &opcode, &event_base, &error_base)
		    && XPresentQueryVersion(dpy, &major, &minor))
			present_opcode = opcode;
		else
			present_opcode = 0;
	}

	return present_opcode > 0;
}

static Bool isPresentComplete(Display *d, XEvent *ev, XPointer arg)
{
	(void)d;
	(void)arg;

	return ev->type == GenericEvent && ev->xcookie.extension == present_opcode
		&& ev->xcookie.evtype == PresentCompleteNotify;
}

/*
 * Ask to be notified at the vertical blank number target (0 for "now"),
 * and wait for it. Returns False if the notification did not come in time.
 */
static Bool waitVBlank(WAnimation *anim, uint64_t target, uint64_t *ust, uint64_t *msc)
{
	unsigned long limit;

	anim->serial++;
	XPresentNotifyMSC(dpy, anim->window, anim->serial, target, 0, 0);
	XFlush(dpy);

	limit = wAnimationElapsed(anim) + VBLANK_TIMEOUT;
	for (;;) {
		struct pollfd pfd;
		unsigned long now;
		XEvent ev;

		while (XCheckIfEvent(dpy, &ev, isPresentComplete, NULL)) {
			XPresentCompleteNotifyEvent *ce;
			Bool ours = False;

			if (!XGetEventData(dpy, &ev.xcookie))
				continue;

			ce = (XPresentCompleteNotifyEvent *) ev.xcookie.data;
			if (ce->eid == anim->eid && ce->serial_number == anim->serial
			    && ce->kind == PresentCompleteKindNotifyMSC) {
				*ust = ce->ust;
				*msc = ce->msc;
				ours = True;
			}
			XFreeEventData(dpy, &ev.xcookie);
			if (ours)
				return True;
		}

		now = wAnimationElapsed(anim);
		if (now >= limit)
			return False;

		pfd.fd = ConnectionNumber(dpy);
		pfd.events = POLLIN;
		pfd.revents = 0;
		poll(&pfd, 1, (int)((limit - now + 999) / 1000));
	}
}

static void stopSync(WAnimation *anim)
{
	if (!anim->synced)
		return;

	XPresentFreeInput(dpy, anim->window, anim->eid);
	anim->synced = False;
}
#endif

void wAnimationStartSynced(WAnimation *anim, WScreen *scr)
{
	wAnimationStart(anim, scr);

#ifdef USE_XPRESENT
	if (presentSupported()) {
		uint64_t ust, msc;

		anim->window = scr->root_win;
		anim->serial = 0;
		anim->eid = XPresentSelectInput(dpy, anim->window, PresentCompleteNotifyMask);
		anim->synced = True;

		/* read the counter and time of the current vertical blank */
		if (waitVBlank(anim, 0, &ust, &msc)) {
			anim->start_ust = ust;
			anim->ust = ust;
			anim->msc = msc;
		} else {
			stopSync(anim);
		}
	}
#endif
}

void wAnimationEnd(WAnimation *anim)
{
#ifdef USE_XPRESENT
	stopSync(anim);
#else
	(void)anim;
#endif
}

Bool wAnimationWaitFrame(WAnimation *anim)
{
	unsigned long elapsed;
	unsigned long due;

#ifdef USE_XPRESENT
	if (anim->synced) {
		uint64_t ust, msc;

		/* if drawing took longer than a frame, the vertical blank asked
		 * for is already past and the notification comes at once, for
		 * the current one: the missed frames are skipped */
		if (!waitVBlank(anim, anim->msc + 1, &ust, &msc)) {
			/* no vertical blank: the monitor was most likely switched
			 * off, there is no point in drawing the rest of the
			 * animation */
			stopSync(anim);
			return False;
		}

		if (msc > anim->msc && ust > anim->ust) {
			anim->frame += msc - anim->msc;
			anim->msc = msc;
			anim->ust = ust;
			anim->time = ust - anim->start_ust;
			return True;
		}

		/* the server could not schedule the vertical blank (a failed
		 * TearFree flip, for example) and answered at once with the
		 * counter and time of the last one: time would not move on and
		 * the animation would never end. Pace the remaining frames with
		 * the timer, which follows the clock */
		stopSync(anim);
		anim->frame = wAnimationElapsed(anim) / anim->interval;
	}
#endif

	elapsed = wAnimationElapsed(anim);

	/* if drawing took longer than a frame, skip the frames that were missed
	 * instead of slowing the whole animation down */
	if (elapsed / anim->interval > anim->frame)
		anim->frame = elapsed / anim->interval;

	anim->frame++;
	due = anim->frame * anim->interval;
	if (due > elapsed)
		wusleep(due - elapsed);

	anim->time = wAnimationElapsed(anim);

	return True;
}
