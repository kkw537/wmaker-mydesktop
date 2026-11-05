
#include <WINGs/WINGsP.h>

#include "geomview.h"


struct W_GeometryView {
	W_Class widgetClass;
	WMView *view;

	WMColor *color;
	WMColor *bgColor;

	WMFont *font;

	WMSize textSize;

	union {
		struct {
			int x, y;
		} pos;
		struct {
			unsigned width, height;
		} size;
	} data;

	unsigned showPosition:1;
	unsigned painted:1;	/* the window shows the current data */
};

static void handleEvents(XEvent * event, void *clientData);
static void paint(WGeometryView * gview);

WGeometryView *WCreateGeometryView(WMScreen * scr)
{
	WGeometryView *gview;
	char buffer[64];
	static W_Class widgetClass = 0;

	if (!widgetClass) {
		widgetClass = W_RegisterUserWidget();
	}

	gview = malloc(sizeof(WGeometryView));
	if (!gview) {
		return NULL;
	}
	memset(gview, 0, sizeof(WGeometryView));

	gview->widgetClass = widgetClass;

	gview->view = W_CreateTopView(scr);
	if (!gview->view) {
		wfree(gview);

		return NULL;
	}
	gview->view->self = gview;

	gview->font = WMSystemFontOfSize(scr, 12);
	if (!gview->font) {
		W_DestroyView(gview->view);
		wfree(gview);

		return NULL;
	}

	gview->bgColor = WMCreateRGBColor(scr, 0x3333, 0x6666, 0x9999, True);
	gview->color = WMWhiteColor(scr);

	WMCreateEventHandler(gview->view, ExposureMask | StructureNotifyMask, handleEvents, gview);

	snprintf(buffer, sizeof(buffer), "%+05i,  %+05i", 0, 0);

	gview->textSize.width = WMWidthOfString(gview->font, buffer, strlen(buffer));
	gview->textSize.height = WMFontHeight(gview->font);

	WMSetWidgetBackgroundColor(gview, gview->bgColor);

	W_ResizeView(gview->view, gview->textSize.width + WMScaled(8), gview->textSize.height + WMScaled(6));

	return gview;
}

void WSetGeometryViewShownPosition(WGeometryView * gview, int x, int y)
{
	/* the display is updated on every pointer motion: don't repaint it
	 * when what it shows does not change */
	if (gview->painted && gview->showPosition
	    && gview->data.pos.x == x && gview->data.pos.y == y)
		return;

	gview->showPosition = 1;
	gview->data.pos.x = x;
	gview->data.pos.y = y;

	paint(gview);
}

void WSetGeometryViewShownSize(WGeometryView * gview, unsigned width, unsigned height)
{
	if (gview->painted && !gview->showPosition
	    && gview->data.size.width == width && gview->data.size.height == height)
		return;

	gview->showPosition = 0;
	gview->data.size.width = width;
	gview->data.size.height = height;

	paint(gview);
}

static void paint(WGeometryView * gview)
{
	WMScreen *scr = W_VIEW_SCREEN(gview->view);
	unsigned int w = W_VIEW_WIDTH(gview->view);
	unsigned int h = W_VIEW_HEIGHT(gview->view);
	char buffer[64];
	Pixmap buf;

	if (!W_VIEW_REALIZED(gview->view))
		return;

	if (gview->showPosition) {
		snprintf(buffer, sizeof(buffer), "%+5i , %+5i    ", gview->data.pos.x, gview->data.pos.y);
	} else {
		snprintf(buffer, sizeof(buffer), "%+5i x %+5i    ",
			 gview->data.size.width, gview->data.size.height);
	}

	/* Compose the display off screen and copy it in one go: drawing the
	 * background and then the text directly on the window lets the text
	 * disappear for a moment, which shows as flicker during a resize (on a
	 * tear free X server the screen may be flipped between the two) */
	buf = XCreatePixmap(scr->display, W_VIEW_DRAWABLE(gview->view), w, h, scr->depth);

	XFillRectangle(scr->display, buf, WMColorGC(gview->bgColor), 0, 0, w, h);

	WMDrawString(scr, buf, gview->color, gview->font,
		     (int)(w - gview->textSize.width) / 2,
		     (int)(h - gview->textSize.height) / 2, buffer, strlen(buffer));

	W_DrawRelief(scr, buf, 0, 0, w, h, WRSimple);

	XCopyArea(scr->display, buf, W_VIEW_DRAWABLE(gview->view), scr->copyGC, 0, 0, w, h, 0, 0);
	XFreePixmap(scr->display, buf);

	gview->painted = 1;
}

static void handleEvents(XEvent * event, void *clientData)
{
	WGeometryView *gview = (WGeometryView *) clientData;

	switch (event->type) {
	case Expose:
		paint(gview);
		break;

	case UnmapNotify:
		/* it must be painted again when it is mapped back, even if
		 * what it shows did not change */
		gview->painted = 0;
		break;

	}
}
