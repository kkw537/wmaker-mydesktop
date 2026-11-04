/*
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
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
#include <X11/Xutil.h>

#include <wraster.h>

#include <stdlib.h>
#include <string.h>
#include "WindowMaker.h"
#include "pixmap.h"

/*
 * Resizes an image of the built-in artwork (titlebar buttons, indicators...)
 * by the global scale factor. Takes ownership of the image and returns the
 * one to use. Integer factors keep the pixel art sharp.
 */
RImage *wPixmapScaleImage(RImage *image)
{
	RImage *scaled;
	int width, height;

	if (!image || WMScaleFactor == 1.0)
		return image;

	width = WMScale(image->width);
	height = WMScale(image->height);
	if (width < 1 || height < 1)
		return image;

	if (WMScaleFactor == (double)(int)WMScaleFactor)
		scaled = RScaleImage(image, width, height);
	else
		scaled = RSmoothScaleImage(image, width, height);

	if (!scaled)
		return image;

	RReleaseImage(image);
	return scaled;
}

/* Nearest neighbour resize of XBM bitmap data (LSB first, padded rows) */
static char *scaleXBMData(const char *data, int width, int height, int swidth, int sheight)
{
	int bpl = (width + 7) / 8, sbpl = (swidth + 7) / 8;
	unsigned char *out;
	int x, y;

	out = wmalloc(sbpl * sheight);
	for (y = 0; y < sheight; y++) {
		int sy = y * height / sheight;

		for (x = 0; x < swidth; x++) {
			int sx = x * width / swidth;

			if (((unsigned char)data[sy * bpl + sx / 8]) & (1 << (sx % 8)))
				out[y * sbpl + x / 8] |= 1 << (x % 8);
		}
	}

	return (char *)out;
}

/*
 *----------------------------------------------------------------------
 * wPixmapCreateFromXPMData--
 * 	Creates a WPixmap structure and initializes it with the supplied
 * XPM structure data.
 *
 * Returns:
 * 	A WPixmap structure or NULL on failure.
 *
 * Notes:
 * 	DEF_XPM_CLOSENESS specifies the XpmCloseness
 *----------------------------------------------------------------------
 */
WPixmap *wPixmapCreateFromXPMData(WScreen * scr, char **data)
{
	RImage *image;
	WPixmap *pix;

	image = RGetImageFromXPMData(scr->rcontext, data);
	if (!image)
		return NULL;

	image = wPixmapScaleImage(image);

	pix = wmalloc(sizeof(WPixmap));

	RConvertImageMask(scr->rcontext, image, &pix->image, &pix->mask, 128);

	pix->width = image->width;
	pix->height = image->height;
	pix->depth = scr->w_depth;

	RReleaseImage(image);

	return pix;
}

/*
 *----------------------------------------------------------------------
 * wPixmapCreateFromXBMData--
 * 	Creates a WPixmap structure and initializes it with the supplied
 * XBM structure data, size and mask.
 *
 * Returns:
 * 	A WPixmap structure or NULL on failure.
 *
 *----------------------------------------------------------------------
 */
WPixmap *wPixmapCreateFromXBMData(WScreen * scr, char *data, char *mask,
				  int width, int height, unsigned long fg, unsigned long bg)
{
	WPixmap *pix;
	char *sdata = NULL, *smask = NULL;

	if (WMScaleFactor != 1.0) {
		int swidth = WMScale(width), sheight = WMScale(height);

		sdata = scaleXBMData(data, width, height, swidth, sheight);
		if (mask)
			smask = (mask == data) ? sdata : scaleXBMData(mask, width, height, swidth, sheight);
		data = sdata;
		mask = smask;
		width = swidth;
		height = sheight;
	}

	pix = wmalloc(sizeof(WPixmap));
	pix->image = XCreatePixmapFromBitmapData(dpy, scr->w_win, data, width, height, fg, bg, scr->w_depth);
	if (pix->image == None) {
		wfree(pix);
		if (smask && smask != sdata)
			wfree(smask);
		if (sdata)
			wfree(sdata);
		return NULL;
	}
	if (mask) {
		pix->mask = XCreateBitmapFromData(dpy, scr->w_win, mask, width, height);
	} else {
		pix->mask = None;
	}
	pix->width = width;
	pix->height = height;
	pix->depth = scr->w_depth;

	if (smask && smask != sdata)
		wfree(smask);
	if (sdata)
		wfree(sdata);

	return pix;
}

WPixmap *wPixmapCreate(Pixmap image, Pixmap mask)
{
	WPixmap *pix;
	Window foo;
	int bar;
	unsigned int width, height, depth, baz;

	pix = wmalloc(sizeof(WPixmap));
	pix->image = image;
	pix->mask = mask;
	if (!XGetGeometry(dpy, image, &foo, &bar, &bar, &width, &height, &baz, &depth)) {
		wwarning("XGetGeometry() failed during wPixmapCreate()");
		wfree(pix);
		return NULL;
	}
	pix->width = width;
	pix->height = height;
	pix->depth = depth;
	return pix;
}

/*
 *----------------------------------------------------------------------
 * wPixmapDestroy--
 * 	Destroys a WPixmap structure and the pixmap/mask it holds.
 *
 * Returns:
 * 	None
 *----------------------------------------------------------------------
 */
void wPixmapDestroy(WPixmap * pix)
{
	if (!pix->shared) {
		if (pix->mask && !pix->client_owned_mask) {
			XFreePixmap(dpy, pix->mask);
		}

		if (pix->image && !pix->client_owned) {
			XFreePixmap(dpy, pix->image);
		}
	}
	wfree(pix);
}
