/* appscale.c - pass the GUI scale factor on to the other applications
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

/*
 * The GUI scale factor (GSScaleFactor) is only known by Window Maker and
 * the WINGs applications. The other toolkits each have their own way to
 * learn it, and most of them can learn it twice: GTK, and the Chromium
 * based applications (Electron) which take it from GTK, multiply their
 * window scale (GDK_SCALE) by the font resolution, so a 192 dpi resolution
 * with a window scale of 2 makes them 4 times bigger.
 *
 * The factor is published the way GNOME does it, so that each toolkit
 * applies it exactly once:
 *
 *  - as the XSETTINGS manager, when there is no other one, with
 *    Gdk/WindowScalingFactor (the integer part of the factor),
 *    Gdk/UnscaledDPI (the font resolution to use with that window scale,
 *    which covers the fractional part of the factor) and Xft/DPI (the
 *    resolution with the whole factor applied): GTK 3 and 4, and the
 *    Chromium and Electron applications going through GTK;
 *
 *  - as the Xft.dpi X resource (96 dpi times the factor): Qt, which then
 *    scales the whole interface and not only its fonts with
 *    QT_ENABLE_HIGHDPI_SCALING, and the Xft based applications like xterm.
 *    The previous value is saved on the root window, and put back when
 *    Window Maker exits or the factor goes back to 1;
 *
 *  - QT_ENABLE_HIGHDPI_SCALING (and QT_SCALE_FACTOR_ROUNDING_POLICY for a
 *    fractional factor) in the environment of the applications started by
 *    Window Maker, when they are not already set.
 */

#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WindowMaker.h"
#include "screen.h"
#include "appscale.h"

#define BASE_DPI	96.0

/* root window property keeping the Xft.dpi resource as it was before it
 * was changed: the whole resource line, or an empty string if there was
 * none */
#define SAVED_DPI_PROPERTY	"_WINDOWMAKER_SAVED_XFT_DPI"

#define XSETTINGS_TYPE_INT	0

/* ---[ Xft.dpi resource ]-------------------------------------------------- */

static char *getStringProperty(Window win, Atom property, Bool *exists)
{
	Atom type;
	int format;
	unsigned long count, remaining;
	unsigned char *data = NULL;
	char *result;

	*exists = False;
	if (XGetWindowProperty(dpy, win, property, 0, 0x100000, False, XA_STRING, &type, &format,
			       &count, &remaining, &data) != Success)
		return NULL;

	if (type != XA_STRING || format != 8) {
		if (data)
			XFree(data);
		return NULL;
	}

	*exists = True;
	result = wmalloc(count + 1);
	if (count > 0)
		memcpy(result, data, count);
	result[count] = 0;
	XFree(data);

	return result;
}

/* True if line (not nul terminated) sets the Xft.dpi resource */
static Bool isDpiLine(const char *line, size_t length)
{
	static const char name[] = "Xft.dpi";
	size_t i = 0;

	while (i < length && (line[i] == ' ' || line[i] == '\t'))
		i++;
	if (length - i < sizeof(name) - 1 || strncmp(line + i, name, sizeof(name) - 1) != 0)
		return False;
	i += sizeof(name) - 1;
	while (i < length && (line[i] == ' ' || line[i] == '\t'))
		i++;

	return i < length && line[i] == ':';
}

/*
 * Replaces the Xft.dpi line of the resources by newLine (nothing if NULL)
 * and returns the line that was there in *oldLine (NULL if none).
 */
static char *replaceDpiLine(const char *resources, const char *newLine, char **oldLine)
{
	const char *p = resources ? resources : "";
	size_t size = strlen(p) + (newLine ? strlen(newLine) : 0) + 2;
	char *result = wmalloc(size);
	size_t used = 0;

	*oldLine = NULL;
	while (*p) {
		const char *end = strchr(p, '\n');
		size_t length = end ? (size_t)(end - p) : strlen(p);

		if (isDpiLine(p, length)) {
			if (!*oldLine) {
				*oldLine = wmalloc(length + 1);
				memcpy(*oldLine, p, length);
				(*oldLine)[length] = 0;
			}
		} else {
			memcpy(result + used, p, length);
			used += length;
			result[used++] = '\n';
		}
		p += length;
		if (*p == '\n')
			p++;
	}
	if (newLine) {
		strcpy(result + used, newLine);
		used += strlen(newLine);
		result[used++] = '\n';
	}
	result[used] = 0;

	return result;
}

static void setResources(Window root, const char *resources)
{
	XChangeProperty(dpy, root, XA_RESOURCE_MANAGER, XA_STRING, 8, PropModeReplace,
			(const unsigned char *)resources, strlen(resources));
}

static void setDpiResource(int dpi)
{
	Window root = DefaultRootWindow(dpy);
	Atom saved = XInternAtom(dpy, SAVED_DPI_PROPERTY, False);
	char line[64], *resources, *updated, *old;
	Bool exists, saved_exists;

	XGrabServer(dpy);

	resources = getStringProperty(root, XA_RESOURCE_MANAGER, &exists);
	snprintf(line, sizeof(line), "Xft.dpi:\t%i", dpi);
	updated = replaceDpiLine(resources, line, &old);
	setResources(root, updated);

	/* keep the value from before Window Maker changed it the first time,
	 * not the one set by a previous instance */
	wfree(getStringProperty(root, saved, &saved_exists));
	if (!saved_exists)
		XChangeProperty(dpy, root, saved, XA_STRING, 8, PropModeReplace,
				(const unsigned char *)(old ? old : ""), old ? strlen(old) : 0);

	XUngrabServer(dpy);

	if (old)
		wfree(old);
	wfree(updated);
	if (resources)
		wfree(resources);
}

static void restoreDpiResource(void)
{
	Window root = DefaultRootWindow(dpy);
	Atom saved = XInternAtom(dpy, SAVED_DPI_PROPERTY, True);
	char *line, *resources, *updated, *old;
	Bool exists;

	if (saved == None)
		return;

	XGrabServer(dpy);

	line = getStringProperty(root, saved, &exists);
	if (exists) {
		resources = getStringProperty(root, XA_RESOURCE_MANAGER, &exists);
		updated = replaceDpiLine(resources, *line ? line : NULL, &old);
		setResources(root, updated);
		XDeleteProperty(dpy, root, saved);

		if (old)
			wfree(old);
		wfree(updated);
		if (resources)
			wfree(resources);
		wfree(line);
	}

	XUngrabServer(dpy);
}

/* ---[ XSETTINGS ]--------------------------------------------------------- */

typedef struct {
	const char *name;
	int value;
} IntSetting;

static size_t pad4(size_t length)
{
	return (length + 3) & ~(size_t)3;
}

static void putCard16(unsigned char *p, unsigned int value)
{
	uint16_t v = value;

	memcpy(p, &v, 2);
}

static void putCard32(unsigned char *p, unsigned long value)
{
	uint32_t v = value;

	memcpy(p, &v, 4);
}

static void setXSettings(Window win, const IntSetting *settings, int count)
{
	Atom property = XInternAtom(dpy, "_XSETTINGS_SETTINGS", False);
	unsigned char *data, *p;
	size_t size = 12;
	int i;

	for (i = 0; i < count; i++)
		size += 4 + pad4(strlen(settings[i].name)) + 4 + 4;

	data = wmalloc(size);
	memset(data, 0, size);

	/* in the byte order of this machine, which is given first */
	{
		unsigned int one = 1;

		data[0] = (*(unsigned char *)&one == 1) ? LSBFirst : MSBFirst;
	}
	putCard32(data + 4, 0);	/* serial */
	putCard32(data + 8, count);

	p = data + 12;
	for (i = 0; i < count; i++) {
		size_t length = strlen(settings[i].name);

		p[0] = XSETTINGS_TYPE_INT;
		putCard16(p + 2, length);
		memcpy(p + 4, settings[i].name, length);
		p += 4 + pad4(length);
		putCard32(p, 0);	/* serial of the last change */
		putCard32(p + 4, (uint32_t) settings[i].value);
		p += 8;
	}

	XChangeProperty(dpy, win, property, property, 8, PropModeReplace, data, size);
	wfree(data);
}

/* become the XSETTINGS manager of the screen, if there is none */
static void startXSettings(WScreen *scr, float factor)
{
	char name[32];
	Atom selection;
	XSetWindowAttributes attr;
	XClientMessageEvent ev;
	IntSetting settings[3];
	int scale;

	snprintf(name, sizeof(name), "_XSETTINGS_S%i", scr->screen);
	selection = XInternAtom(dpy, name, False);

	if (XGetSelectionOwner(dpy, selection) != None) {
		wwarning(_("an XSETTINGS manager is already running, the GUI scale factor is not passed on"
			   " to the GTK and Chromium based applications through it"));
		return;
	}

	/* window scale for GTK, an integer: the rest of the factor is
	 * applied to the fonts */
	scale = (int)floorf(factor + 0.01F);
	if (scale < 1)
		scale = 1;

	settings[0].name = "Gdk/WindowScalingFactor";
	settings[0].value = scale;
	settings[1].name = "Gdk/UnscaledDPI";
	settings[1].value = (int)(BASE_DPI * factor / scale * 1024 + 0.5);
	settings[2].name = "Xft/DPI";
	settings[2].value = (int)(BASE_DPI * factor * 1024 + 0.5);

	attr.override_redirect = True;
	scr->xsettings_win = XCreateWindow(dpy, scr->root_win, -10, -10, 1, 1, 0, CopyFromParent,
					   InputOnly, CopyFromParent, CWOverrideRedirect, &attr);
	setXSettings(scr->xsettings_win, settings, wlengthof(settings));

	XSetSelectionOwner(dpy, selection, scr->xsettings_win, CurrentTime);
	if (XGetSelectionOwner(dpy, selection) != scr->xsettings_win) {
		XDestroyWindow(dpy, scr->xsettings_win);
		scr->xsettings_win = None;
		return;
	}

	/* tell the clients that there is a new manager */
	memset(&ev, 0, sizeof(ev));
	ev.type = ClientMessage;
	ev.window = scr->root_win;
	ev.message_type = XInternAtom(dpy, "MANAGER", False);
	ev.format = 32;
	ev.data.l[0] = CurrentTime;
	ev.data.l[1] = selection;
	ev.data.l[2] = scr->xsettings_win;
	XSendEvent(dpy, scr->root_win, False, StructureNotifyMask, (XEvent *) &ev);
}

/* ---[ Environment ]------------------------------------------------------- */

static void setEnvironment(float factor)
{
	/* scale factors set by hand for one toolkit come on top of the one
	 * given here, the applications started from Window Maker would be
	 * scaled twice (4 times as big with 2 for both) */
	static const char *const manual[] = {
		"GDK_SCALE", "GDK_DPI_SCALE", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS"
	};
	int i;

	for (i = 0; i < wlengthof(manual); i++)
		unsetenv(manual[i]);

	/* Qt 5.14 and later (always on with Qt 6): scale the interface from
	 * the Xft.dpi resource */
	setenv("QT_ENABLE_HIGHDPI_SCALING", "1", 0);

	if (factor != floorf(factor))
		setenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "PassThrough", 0);
}

/* ------------------------------------------------------------------------- */

void wAppScaleInit(WScreen *scr)
{
	float factor = WMGetScaleFactor();

	scr->xsettings_win = None;

	if (!wPreferences.scale_applications || factor == 1.0F) {
		/* undo what a previous instance did */
		if (scr->screen == DefaultScreen(dpy))
			restoreDpiResource();
		return;
	}

	if (scr->screen == DefaultScreen(dpy)) {
		setDpiResource((int)(BASE_DPI * factor + 0.5));
		setEnvironment(factor);
	}

	startXSettings(scr, factor);
}

void wAppScaleRestore(WScreen *scr)
{
	if (scr->screen == DefaultScreen(dpy))
		restoreDpiResource();

	if (scr->xsettings_win != None) {
		XDestroyWindow(dpy, scr->xsettings_win);
		scr->xsettings_win = None;
	}
}
