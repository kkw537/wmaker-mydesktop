
#include "WINGsP.h"

#include <wraster.h>
#include <ctype.h>

void
W_DrawRelief(W_Screen * scr, Drawable d, int x, int y, unsigned int width,
	     unsigned int height, WMReliefType relief)
{
	W_DrawReliefWithGC(scr, d, x, y, width, height, relief,
			   WMColorGC(scr->black), WMColorGC(scr->darkGray),
			   WMColorGC(scr->gray), WMColorGC(scr->white));
}

static void fillLine(Display *dpy, Drawable d, GC gc, int x, int y, int width, int height)
{
	if (width > 0 && height > 0)
		XFillRectangle(dpy, d, gc, x, y, width, height);
}

/*
 * The relief lines are drawn as filled rectangles of GSScaleFactor pixels
 * thickness, so that with a scale factor of 2 the relief looks exactly like
 * the regular one drawn with 2x2 sized pixels.
 */
void
W_DrawReliefWithGC(W_Screen * scr, Drawable d, int x, int y, unsigned int width,
		   unsigned int height, WMReliefType relief, GC black, GC dark, GC light, GC white)
{
	Display *dpy = scr->display;
	GC bgc;
	GC wgc;
	GC lgc;
	GC dgc;
	int t = WMScaled(1);	/* thickness of a line */
	int w = width;
	int h = height;

	switch (relief) {
	case WRSimple:
		fillLine(dpy, d, black, x, y, w, t);
		fillLine(dpy, d, black, x, y + h - t, w, t);
		fillLine(dpy, d, black, x, y, t, h);
		fillLine(dpy, d, black, x + w - t, y, t, h);
		return;

	case WRRaised:
		bgc = black;
		dgc = dark;
		wgc = white;
		lgc = light;
		break;

	case WRSunken:
		wgc = dark;
		lgc = black;
		bgc = white;
		dgc = light;
		break;

	case WRPushed:
		lgc = wgc = black;
		dgc = bgc = white;
		break;

	case WRRidge:
		lgc = bgc = dark;
		dgc = wgc = white;
		break;

	case WRGroove:
		wgc = dgc = dark;
		lgc = bgc = white;
		break;

	default:
		return;
	}
	/* top left */
	fillLine(dpy, d, wgc, x, y, w, t);
	if (w > 2 * t && relief != WRRaised && relief != WRPushed) {
		fillLine(dpy, d, lgc, x + t, y + t, w - 3 * t, t);
	}

	fillLine(dpy, d, wgc, x, y, t, h);
	if (h > 2 * t && relief != WRRaised && relief != WRPushed) {
		fillLine(dpy, d, lgc, x + t, y + t, t, h - 3 * t);
	}

	/* bottom right */
	fillLine(dpy, d, bgc, x, y + h - t, w, t);
	if (w > 2 * t && relief != WRPushed) {
		fillLine(dpy, d, dgc, x + t, y + h - 2 * t, w - 2 * t, t);
	}

	fillLine(dpy, d, bgc, x + w - t, y, t, h);
	if (h > 2 * t && relief != WRPushed) {
		fillLine(dpy, d, dgc, x + w - 2 * t, y + t, t, h - 2 * t);
	}
}

void W_DrawScaledLine(W_Screen *scr, Drawable d, GC gc, int x1, int y1, int x2, int y2)
{
	Display *dpy = scr->display;
	int t = WMScaled(1);
	int i, j;

	if (t <= 1) {
		XDrawLine(dpy, d, gc, x1, y1, x2, y2);
		return;
	}

	/* horizontal and vertical lines are simple rectangles */
	if (y1 == y2 || x1 == x2) {
		XFillRectangle(dpy, d, gc, WMIN(x1, x2), WMIN(y1, y2),
			       abs(x2 - x1) + t, abs(y2 - y1) + t);
		return;
	}

	/* every pixel of the line becomes a t x t block */
	for (i = 0; i < t; i++)
		for (j = 0; j < t; j++)
			XDrawLine(dpy, d, gc, x1 + i, y1 + j, x2 + i, y2 + j);
}

static int findNextWord(const char *text, int limit)
{
	int pos, len;

	len = strcspn(text, " \t\n\r");
	pos = len + strspn(text + len, " \t\n\r");
	if (pos > limit)
		pos = limit;

	return pos;
}

static int fitText(const char *text, WMFont * font, int width, int wrap)
{
	int i, w, beforecrlf, word1, word2;

	/* text length before first cr/lf */
	beforecrlf = strcspn(text, "\n");

	if (!wrap || beforecrlf == 0)
		return beforecrlf;

	w = WMWidthOfString(font, text, beforecrlf);
	if (w <= width) {
		/* text up to first crlf fits */
		return beforecrlf;
	}

	word1 = 0;
	while (1) {
		word2 = word1 + findNextWord(text + word1, beforecrlf - word1);
		if (word2 >= beforecrlf)
			break;
		w = WMWidthOfString(font, text, word2);
		if (w > width)
			break;
		word1 = word2;
	}

	/* Advance character by character (not byte by byte) */
	i = word1;
	while (i < word2) {
		int next_i = i + oneUTF8CharForward(text + i, word2 - i);
		w = WMWidthOfString(font, text, next_i);
		if (w > width)
			break;
		i = next_i;
	}

	/* keep words complete if possible */
	if (!isspace(text[i]) && word1 > 0) {
		i = word1;
	} else if (isspace(text[i]) && i < beforecrlf) {
		/* keep space on current row, so new row has next word in column 1 */
		i++;
	}

	return i;
}

int W_GetTextHeight(WMFont * font, const char *text, int width, int wrap)
{
	const char *ptr = text;
	int count;
	int length = strlen(text);
	int h;
	int fheight = WMFontHeight(font);

	h = 0;
	while (length > 0) {
		count = fitText(ptr, font, width, wrap);

		h += fheight;

		if (isspace(ptr[count]))
			count++;

		ptr += count;
		length -= count;
	}
	return h;
}

void
W_PaintText(W_View * view, Drawable d, WMFont * font, int x, int y,
	    int width, WMAlignment alignment, WMColor * color, int wrap,
	    const char *text, int length)
{
	const char *ptr = text;
	int line_width;
	int line_x;
	int count;
	int fheight = WMFontHeight(font);

	while (length > 0) {
		count = fitText(ptr, font, width, wrap);

		line_width = WMWidthOfString(font, ptr, count);
		if (alignment == WALeft)
			line_x = x;
		else if (alignment == WARight)
			line_x = x + width - line_width;
		else
			line_x = x + (width - line_width) / 2;

		WMDrawString(view->screen, d, color, font, line_x, y, ptr, count);

		if (wrap && ptr[count] != '\n')
			y += fheight;

		while (ptr[count] && ptr[count] == '\n') {
			y += fheight;
			count++;
		}

		ptr += count;
		length -= count;
	}
}

void
W_PaintTextAndImage(W_View * view, int wrap, WMColor * textColor, W_Font * font,
		    WMReliefType relief, const char *text,
		    WMAlignment alignment, W_Pixmap * image,
		    WMImagePosition position, WMColor * backColor, int ofs)
{
	W_Screen *screen = view->screen;
	int ix, iy;
	int x, y, w, h;
	Drawable d = view->window;

#ifdef DOUBLE_BUFFER
	d = XCreatePixmap(screen->display, view->window, view->size.width, view->size.height, screen->depth);
#endif

	/* background */
	if (backColor) {
		XFillRectangle(screen->display, d, WMColorGC(backColor),
			       0, 0, view->size.width, view->size.height);
	} else {
		if (view->attribs.background_pixmap) {
#ifndef DOUBLE_BUFFER
			XClearWindow(screen->display, d);
#else
			XCopyArea(screen->display, view->attribs.background_pixmap, d, screen->copyGC, 0, 0, view->size.width, view->size.height, 0, 0);
#endif
		} else {
#ifndef DOUBLE_BUFFER
			XClearWindow(screen->display, d);
#else
			XSetForeground(screen->display, screen->copyGC, view->attribs.background_pixel);
			XFillRectangle(screen->display, d, screen->copyGC, 0, 0, view->size.width, view->size.height);
#endif
		}
	}

	if (relief == WRFlat) {
		x = 0;
		y = 0;
		w = view->size.width;
		h = view->size.height;
	} else {
		x = WMScaled(1);
		y = WMScaled(1);
		w = view->size.width - WMScaled(3);
		h = view->size.height - WMScaled(3);
	}

	/* calc. image alignment */
	if (position != WIPNoImage && image != NULL) {
		switch (position) {
		case WIPOverlaps:
		case WIPImageOnly:
			ix = (view->size.width - image->width) / 2;
			iy = (view->size.height - image->height) / 2;
			/*
			   x = 2;
			   y = 0;
			 */
			break;

		case WIPLeft:
			ix = x;
			iy = y + (h - image->height) / 2;
			x = x + image->width + WMScaled(5);
			y = 0;
			w -= image->width + WMScaled(5);
			break;

		case WIPRight:
			ix = view->size.width - image->width - x;
			iy = y + (h - image->height) / 2;
			w -= image->width + WMScaled(5);
			break;

		case WIPBelow:
			ix = (view->size.width - image->width) / 2;
			iy = h - image->height;
			y = 0;
			h -= image->height;
			break;

		default:
		case WIPAbove:
			ix = (view->size.width - image->width) / 2;
			iy = y;
			y = image->height;
			h -= image->height;
			break;
		}

		ix += ofs;
		iy += ofs;

		XSetClipOrigin(screen->display, screen->clipGC, ix, iy);
		XSetClipMask(screen->display, screen->clipGC, image->mask);

		if (image->depth == 1)
			XCopyPlane(screen->display, image->pixmap, d, screen->clipGC,
				   0, 0, image->width, image->height, ix, iy, 1);
		else
			XCopyArea(screen->display, image->pixmap, d, screen->clipGC,
				  0, 0, image->width, image->height, ix, iy);
	}

	/* draw text */
	if (position != WIPImageOnly && text != NULL) {
		int textHeight;

		textHeight = W_GetTextHeight(font, text, w - WMScaled(8), wrap);
		W_PaintText(view, d, font, x + ofs + WMScaled(4), y + ofs + (h - textHeight) / 2, w - WMScaled(8),
			    alignment, textColor, wrap, text, strlen(text));
	}

	/* draw relief */
	W_DrawRelief(screen, d, 0, 0, view->size.width, view->size.height, relief);

#ifdef DOUBLE_BUFFER
	XCopyArea(screen->display, d, view->window, screen->copyGC, 0, 0,
		  view->size.width, view->size.height, 0, 0);
	XFreePixmap(screen->display, d);
#endif
}

WMPoint wmkpoint(int x, int y)
{
	WMPoint point;

	point.x = x;
	point.y = y;

	return point;
}

WMSize wmksize(unsigned int width, unsigned int height)
{
	WMSize size;

	size.width = width;
	size.height = height;

	return size;
}

WMRect wmkrect(int x, int y, unsigned int width, unsigned int height)
{
	WMRect rect;

	rect.pos.x = x;
	rect.pos.y = y;
	rect.size.width = width;
	rect.size.height = height;

	return rect;
}
