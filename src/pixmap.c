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

	/* draw the built-in image with the GUI scale factor applied */
	{
		RImage *scaled = WMScaleImageByFactor(image);

		if (scaled) {
			RReleaseImage(image);
			image = scaled;
		}
	}

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
 * wScaleBitmapData--
 * 	Scales the bits of a XBM bitmap by the GUI scale factor
 * (GSScaleFactor), so that each pixel of the bitmap is drawn as a block
 * of scale factor sized pixels.
 *
 * Returns:
 * 	The new bitmap data (to be freed with wfree()), and its size in
 * swidth/sheight.
 *----------------------------------------------------------------------
 */
char *wScaleBitmapData(const char *data, int width, int height, int *swidth, int *sheight)
{
	int sw = WMScaled(width);
	int sh = WMScaled(height);
	int src_bpl = (width + 7) / 8;
	int dst_bpl = (sw + 7) / 8;
	char *bits;
	int x, y;

	bits = wmalloc(dst_bpl * sh);
	for (y = 0; y < sh; y++) {
		int sy = y * height / sh;

		for (x = 0; x < sw; x++) {
			int sx = x * width / sw;

			if (data[sy * src_bpl + sx / 8] & (1 << (sx % 8)))
				bits[y * dst_bpl + x / 8] |= (1 << (x % 8));
		}
	}

	*swidth = sw;
	*sheight = sh;

	return bits;
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
	char *sdata, *smask = NULL;
	int swidth, sheight;

	/* the bitmap is drawn with the GUI scale factor applied */
	sdata = wScaleBitmapData(data, width, height, &swidth, &sheight);
	if (mask)
		smask = wScaleBitmapData(mask, width, height, &swidth, &sheight);

	pix = wmalloc(sizeof(WPixmap));
	pix->image = XCreatePixmapFromBitmapData(dpy, scr->w_win, sdata, swidth, sheight, fg, bg, scr->w_depth);
	if (pix->image == None) {
		wfree(sdata);
		if (smask)
			wfree(smask);
		wfree(pix);
		return NULL;
	}
	if (smask) {
		pix->mask = XCreateBitmapFromData(dpy, scr->w_win, smask, swidth, sheight);
		wfree(smask);
	} else {
		pix->mask = None;
	}
	wfree(sdata);
	pix->width = swidth;
	pix->height = sheight;
	pix->depth = scr->w_depth;
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
