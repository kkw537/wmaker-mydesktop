/*
 * Original idea and implementation by Frederik Schueler <fr.schueler@netsurf.de>
 * Rewritten by Pascal Hofstee <daeron@windowmaker.info>
 * - Added options to set min/max values
 * - centralized drawing into one pain function
 */

#include "WINGsP.h"

typedef struct W_ProgressIndicator {
	W_Class widgetClass;
	W_View *view;

	int value;
	int minValue;
	int maxValue;

	void *clientData;
} ProgressIndicator;

#define DEFAULT_PROGRESS_INDICATOR_WIDTH	WMScaled(276)
#define DEFAULT_PROGRESS_INDICATOR_HEIGHT	WMScaled(16)

/* define if only the ticks within the progress region should be displayed */
#undef SHOW_PROGRESS_TICKS_ONLY

static void didResizeProgressIndicator(W_ViewDelegate * self, WMView * view);

W_ViewDelegate _ProgressIndicatorDelegate = {
	NULL,
	NULL,
	didResizeProgressIndicator,
	NULL,
	NULL
};

static void destroyProgressIndicator(ProgressIndicator * pPtr);
static void paintProgressIndicator(ProgressIndicator * pPtr);
static void handleEvents(XEvent * event, void *data);

WMProgressIndicator *WMCreateProgressIndicator(WMWidget * parent)
{
	ProgressIndicator *pPtr;

	pPtr = wmalloc(sizeof(ProgressIndicator));

	pPtr->widgetClass = WC_ProgressIndicator;

	pPtr->view = W_CreateView(W_VIEW(parent));
	if (!pPtr->view) {
		wfree(pPtr);
		return NULL;
	}

	pPtr->view->self = pPtr;

	pPtr->view->delegate = &_ProgressIndicatorDelegate;

	WMCreateEventHandler(pPtr->view, ExposureMask | StructureNotifyMask, handleEvents, pPtr);

	W_ResizeView(pPtr->view, DEFAULT_PROGRESS_INDICATOR_WIDTH, DEFAULT_PROGRESS_INDICATOR_HEIGHT);

	/* Initialize ProgressIndicator Values */
	pPtr->value = 0;
	pPtr->minValue = 0;
	pPtr->maxValue = 100;

	return pPtr;
}

void WMSetProgressIndicatorMinValue(WMProgressIndicator * progressindicator, int value)
{
	CHECK_CLASS(progressindicator, WC_ProgressIndicator);

	progressindicator->minValue = value;
	if (progressindicator->value < value) {
		progressindicator->value = value;
		if (progressindicator->view->flags.mapped) {
			paintProgressIndicator(progressindicator);
		}
	}
}

void WMSetProgressIndicatorMaxValue(WMProgressIndicator * progressindicator, int value)
{
	CHECK_CLASS(progressindicator, WC_ProgressIndicator);

	progressindicator->maxValue = value;
	if (progressindicator->value > value) {
		progressindicator->value = value;
		if (progressindicator->view->flags.mapped) {
			paintProgressIndicator(progressindicator);
		}
	}
}

void WMSetProgressIndicatorValue(WMProgressIndicator * progressindicator, int value)
{
	CHECK_CLASS(progressindicator, WC_ProgressIndicator);

	progressindicator->value = value;

	/* Check if value is within min/max-range */
	if (progressindicator->minValue > value)
		progressindicator->value = progressindicator->minValue;

	if (progressindicator->maxValue < value)
		progressindicator->value = progressindicator->maxValue;

	if (progressindicator->view->flags.mapped) {
		paintProgressIndicator(progressindicator);
	}
}

int WMGetProgressIndicatorMinValue(WMProgressIndicator * progressindicator)
{
	CHECK_CLASS(progressindicator, WC_ProgressIndicator);

	return progressindicator->minValue;
}

int WMGetProgressIndicatorMaxValue(WMProgressIndicator * progressindicator)
{
	CHECK_CLASS(progressindicator, WC_ProgressIndicator);

	return progressindicator->maxValue;
}

int WMGetProgressIndicatorValue(WMProgressIndicator * progressindicator)
{
	CHECK_CLASS(progressindicator, WC_ProgressIndicator);

	return progressindicator->value;
}

static void didResizeProgressIndicator(W_ViewDelegate * self, WMView * view)
{
	WMProgressIndicator *pPtr = (WMProgressIndicator *) view->self;
	int width = pPtr->view->size.width;
	int height = pPtr->view->size.height;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) self;
	(void) width;
	(void) height;

	assert(width > 0);
	assert(height > 0);
}

static void paintProgressIndicator(ProgressIndicator * pPtr)
{
	W_Screen *scr = pPtr->view->screen;
	GC bgc;
	GC wgc;
	GC lgc;
	GC dgc;
	WMSize size = pPtr->view->size;
	int perc, w, h;
	double unit, i;
	Pixmap buffer;
	int t = WMScaled(1);	/* line thickness */

	bgc = WMColorGC(scr->black);
	wgc = WMColorGC(scr->white);
	lgc = WMColorGC(scr->gray);
	dgc = WMColorGC(scr->darkGray);

	unit = (double)(size.width - 3 * t) / 100;

	buffer = XCreatePixmap(scr->display, pPtr->view->window, size.width, size.height, scr->depth);

	XFillRectangle(scr->display, buffer, lgc, 0, 0, size.width, size.height);

	/* Calculate size of Progress to draw and paint ticks */
	perc = (pPtr->value - pPtr->minValue) * 100 / (pPtr->maxValue - pPtr->minValue);

	w = (int)((double)(perc * unit));
	h = size.height - 2 * t;

	if (w > (size.width - 3 * t))
		w = size.width - 3 * t;

	if (w > 0) {
		XFillRectangle(scr->display, buffer, lgc, 2 * t, t, w, h);
		XFillRectangle(scr->display, buffer, scr->stippleGC, 2 * t, t, w, h);
		W_DrawRelief(scr, buffer, 2 * t, t, w, h, WRFlat);

		/* Draw Progress Marks */
		i = (5.0 * unit);

#ifdef SHOW_PROGRESS_TICKS_ONLY
		while ((int)i < w + 5 * t) {
#else
		while ((int)i < (size.width - 3 * t)) {
#endif
			XFillRectangle(scr->display, buffer, dgc, (int)i + 2 * t, h - 3 * t, t, 3 * t);

			i += (5.0 * unit);

#ifdef SHOW_PROGRESS_TICKS_ONLY
			if ((int)i >= w)
				break;
#endif

			XFillRectangle(scr->display, buffer, dgc, (int)i + 2 * t, h - 6 * t, t, 6 * t);

			i += (5.0 * unit);
		}
	}

	XFillRectangle(scr->display, buffer, bgc, w + 2 * t, t, t, h + t);
	XFillRectangle(scr->display, buffer, lgc, 2 * t, h, w + t, t);

	XFillRectangle(scr->display, buffer, dgc, 0, 0, t, size.height);
	XFillRectangle(scr->display, buffer, dgc, 0, 0, size.width, t);
	XFillRectangle(scr->display, buffer, bgc, t, t, t, size.height - t);
	XFillRectangle(scr->display, buffer, bgc, t, t, size.width - t, t);

	XFillRectangle(scr->display, buffer, wgc, size.width - t, 0, t, size.height);
	XFillRectangle(scr->display, buffer, wgc, 0, size.height - t, size.width, t);

	XCopyArea(scr->display, buffer, pPtr->view->window, scr->copyGC, 0, 0, size.width, size.height, 0, 0);

	XFreePixmap(scr->display, buffer);
}

static void handleEvents(XEvent * event, void *data)
{
	ProgressIndicator *pPtr = (ProgressIndicator *) data;

	CHECK_CLASS(data, WC_ProgressIndicator);

	switch (event->type) {
	case Expose:
		if (event->xexpose.count != 0)
			break;
		paintProgressIndicator(pPtr);
		break;
	case DestroyNotify:
		destroyProgressIndicator(pPtr);
		break;
	}
}

static void destroyProgressIndicator(ProgressIndicator * pPtr)
{
	WMRemoveNotificationObserver(pPtr);

	wfree(pPtr);
}
