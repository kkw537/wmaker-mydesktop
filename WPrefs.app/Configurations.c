/* Configurations.c- misc. configurations
 *
 *  WPrefs - Window Maker Preferences Program
 *
 *  Copyright (c) 1998-2003 Alfredo K. Kojima
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

#include "WPrefs.h"

typedef struct _Panel {
	WMBox *box;
	char *sectionName;

	char *description;

	CallbackRec callbacks;

	WMWidget *parent;

	WMFrame *icoF;
	WMButton *icoB[5];

	WMFrame *shaF;
	WMButton *shaB[5];

	WMFrame *titlF;
	WMButton *oldsB;
	WMButton *newsB;
	WMButton *nextB;

	WMFrame *animF;
	WMButton *animB;
	WMButton *supB;
	WMLabel *noteL;

	WMFrame *smoF;
	WMButton *smoB;

	WMFrame *dithF;
	WMButton *dithB;
	WMSlider *dithS;
	WMLabel *dithL;
	WMLabel *dith1L;
	WMLabel *dith2L;

	int cmapSize;
} _Panel;

#define ICON_FILE	"configs"
#define OLDS_IMAGE	"oldstyle"
#define NEWS_IMAGE	"newstyle"
#define NEXT_IMAGE	"nextstyle"
#define ANIM_IMAGE	"animations"
#define SUPERF_IMAGE	"moreanim"
#define SMOOTH_IMAGE	"smooth"
#define SPEED_IMAGE 	"speed%i"
#define SPEED_IMAGE_S 	"speed%is"
#define ARQUIVO_XIS	"xis"

static void updateLabel(WMWidget *self, void *data);

static void showData(_Panel *panel)
{
	char *str;

	WMPerformButtonClick(panel->icoB[GetSpeedForKey("IconSlideSpeed")]);
	WMPerformButtonClick(panel->shaB[GetSpeedForKey("ShadeSpeed")]);

	str = GetStringForKey("NewStyle");
	if (str && strcasecmp(str, "next") == 0) {
		WMPerformButtonClick(panel->nextB);
	} else if (str && strcasecmp(str, "old") == 0) {
		WMPerformButtonClick(panel->oldsB);
	} else {
		WMPerformButtonClick(panel->newsB);
	}

	WMSetButtonSelected(panel->animB, !GetBoolForKey("DisableAnimations"));
	WMSetButtonSelected(panel->supB, GetBoolForKey("Superfluous"));
	WMSetButtonSelected(panel->smoB, GetBoolForKey("SmoothWorkspaceBack"));
	WMSetButtonSelected(panel->dithB, GetBoolForKey("DisableDithering"));
	WMSetSliderValue(panel->dithS, GetIntegerForKey("ColormapSize"));
	updateLabel(panel->dithS, panel);
}

static void updateLabel(WMWidget *self, void *data)
{
	WMSlider *sPtr = (WMSlider *) self;
	_Panel *panel = (_Panel *) data;
	char buffer[64];
	float fl;

	fl = WMGetSliderValue(sPtr);
	panel->cmapSize = (int)fl;

	sprintf(buffer, "%i", panel->cmapSize * panel->cmapSize * panel->cmapSize);
	WMSetLabelText(panel->dithL, buffer);
}

static void createPanel(Panel *p)
{
	_Panel *panel = (_Panel *) p;
	WMScreen *scr = WMWidgetScreen(panel->parent);
	char *buf1, *buf2;
	WMPixmap *icon, *altIcon;
	RImage *xis = NULL;
	int i;
	RContext *rc = WMScreenRContext(scr);
	WMFont *font = WMSystemFontOfSize(scr, 10);
	char *path;

	path = LocateImage(ARQUIVO_XIS);
	if (path) {
		xis = RLoadImage(rc, path, 0);
		if (!xis)
			wwarning(_("could not load image file %s"), path);
		wfree(path);
	}

	panel->box = WMCreateBox(panel->parent);
	WMSetViewExpandsToParent(WMWidgetView(panel->box), WMScaled(2), WMScaled(2), WMScaled(2), WMScaled(2));

    /*********** Icon Slide Speed **********/
	panel->icoF = WMCreateFrame(panel->box);
	WMResizeWidget(panel->icoF, WMScaled(212), WMScaled(45));
	WMMoveWidget(panel->icoF, WMScaled(15), WMScaled(10));
	WMSetFrameTitle(panel->icoF, _("Icon Slide Speed"));

    /*********** Shade Animation Speed **********/
	panel->shaF = WMCreateFrame(panel->box);
	WMResizeWidget(panel->shaF, WMScaled(212), WMScaled(45));
	WMMoveWidget(panel->shaF, WMScaled(15), WMScaled(65));
	WMSetFrameTitle(panel->shaF, _("Shade Animation Speed"));

	buf1 = wmalloc(strlen(SPEED_IMAGE) + 1);
	buf2 = wmalloc(strlen(SPEED_IMAGE_S) + 1);

	for (i = 0; i < 5; i++) {
		panel->icoB[i] = WMCreateCustomButton(panel->icoF, WBBStateChangeMask);
		panel->shaB[i] = WMCreateCustomButton(panel->shaF, WBBStateChangeMask);
		WMResizeWidget(panel->icoB[i], WMScaled(40), WMScaled(24));
		WMMoveWidget(panel->icoB[i], WMScaled(2) + (WMScaled(40) * i), WMScaled(15));
		WMResizeWidget(panel->shaB[i], WMScaled(40), WMScaled(24));
		WMMoveWidget(panel->shaB[i], WMScaled(2) + (WMScaled(40) * i), WMScaled(15));
		WMSetButtonBordered(panel->icoB[i], False);
		WMSetButtonImagePosition(panel->icoB[i], WIPImageOnly);
		if (i > 0) {
			WMGroupButtons(panel->icoB[0], panel->icoB[i]);
		}
		WMSetButtonBordered(panel->shaB[i], False);
		WMSetButtonImagePosition(panel->shaB[i], WIPImageOnly);
		if (i > 0) {
			WMGroupButtons(panel->shaB[0], panel->shaB[i]);
		}
		sprintf(buf1, SPEED_IMAGE, i);
		sprintf(buf2, SPEED_IMAGE_S, i);
		path = LocateImage(buf1);
		if (path) {
			icon = WMCreatePixmapFromFile(scr, path);
			if (icon) {
				WMSetButtonImage(panel->icoB[i], icon);
				WMSetButtonImage(panel->shaB[i], icon);
				WMReleasePixmap(icon);
			} else {
				wwarning(_("could not load icon file %s"), path);
			}
			wfree(path);
		}
		path = LocateImage(buf2);
		if (path) {
			icon = WMCreatePixmapFromFile(scr, path);
			if (icon) {
				WMSetButtonAltImage(panel->icoB[i], icon);
				WMSetButtonAltImage(panel->shaB[i], icon);
				WMReleasePixmap(icon);
			} else {
				wwarning(_("could not load icon file %s"), path);
			}
			wfree(path);
		}
	}
	wfree(buf1);
	wfree(buf2);

	WMMapSubwidgets(panel->icoF);
	WMMapSubwidgets(panel->shaF);

    /***************** Smoothed Scaling *****************/
	panel->smoF = WMCreateFrame(panel->box);
	WMResizeWidget(panel->smoF, WMScaled(94), WMScaled(100));
	WMMoveWidget(panel->smoF, WMScaled(420), WMScaled(10));
	WMSetFrameTitle(panel->smoF, _("Smooth Scaling"));

	panel->smoB = WMCreateButton(panel->smoF, WBTToggle);
	WMResizeWidget(panel->smoB, WMScaled(64), WMScaled(64));
	WMMoveWidget(panel->smoB, WMScaled(15), WMScaled(23));
	WMSetButtonImagePosition(panel->smoB, WIPImageOnly);
	path = LocateImage(SMOOTH_IMAGE);
	if (path) {
		RImage *image, *scaled;

		image = RLoadImage(WMScreenRContext(scr), path, 0);
		wfree(path);

		scaled = RScaleImage(image, WMScaled(61), WMScaled(61));
		icon = WMCreatePixmapFromRImage(scr, scaled, 128);
		RReleaseImage(scaled);
		if (icon) {
			WMSetButtonImage(panel->smoB, icon);
			WMReleasePixmap(icon);
		}

		scaled = RSmoothScaleImage(image, WMScaled(61), WMScaled(61));
		icon = WMCreatePixmapFromRImage(scr, scaled, 128);
		RReleaseImage(scaled);
		if (icon) {
			WMSetButtonAltImage(panel->smoB, icon);
			WMReleasePixmap(icon);
		}

		RReleaseImage(image);
	}
	WMSetBalloonTextForView(_("Smooth scaled background images, neutralizing\n"
				  "the `pixelization' effect. This will slow\n"
				  "down loading of background images considerably."), WMWidgetView(panel->smoB));

	WMMapSubwidgets(panel->smoF);

    /***************** Titlebar Style Size ****************/
	panel->titlF = WMCreateFrame(panel->box);
	WMResizeWidget(panel->titlF, WMScaled(212), WMScaled(97));
	WMMoveWidget(panel->titlF, WMScaled(15), WMScaled(120));
	WMSetFrameTitle(panel->titlF, _("Titlebar Style"));

	panel->oldsB = WMCreateButton(panel->titlF, WBTOnOff);
	WMResizeWidget(panel->oldsB, WMScaled(60), WMScaled(40));
	WMMoveWidget(panel->oldsB, WMScaled(16), WMScaled(32));
	WMSetButtonImagePosition(panel->oldsB, WIPImageOnly);
	path = LocateImage(OLDS_IMAGE);
	if (path) {
		icon = WMCreatePixmapFromFile(scr, path);
		if (icon) {
			WMSetButtonImage(panel->oldsB, icon);
			WMReleasePixmap(icon);
		}
		wfree(path);
	}

	panel->newsB = WMCreateButton(panel->titlF, WBTOnOff);
	WMResizeWidget(panel->newsB, WMScaled(60), WMScaled(40));
	WMMoveWidget(panel->newsB, WMScaled(76), WMScaled(32));
	WMSetButtonImagePosition(panel->newsB, WIPImageOnly);
	path = LocateImage(NEWS_IMAGE);
	if (path) {
		icon = WMCreatePixmapFromFile(scr, path);
		if (icon) {
			WMSetButtonImage(panel->newsB, icon);
			WMReleasePixmap(icon);
		}
		wfree(path);
	}

	panel->nextB = WMCreateButton(panel->titlF, WBTOnOff);
	WMResizeWidget(panel->nextB, WMScaled(60), WMScaled(40));
	WMMoveWidget(panel->nextB, WMScaled(136), WMScaled(32));
	WMSetButtonImagePosition(panel->nextB, WIPImageOnly);
	path = LocateImage(NEXT_IMAGE);
	if (path) {
		icon = WMCreatePixmapFromFile(scr, path);
		if (icon) {
			WMSetButtonImage(panel->nextB, icon);
			WMReleasePixmap(icon);
		}
		wfree(path);
	}

	WMGroupButtons(panel->newsB, panel->oldsB);
	WMGroupButtons(panel->newsB, panel->nextB);

	WMMapSubwidgets(panel->titlF);

    /**************** Features ******************/
	panel->animF = WMCreateFrame(panel->box);
	WMResizeWidget(panel->animF, WMScaled(173), WMScaled(100));
	WMMoveWidget(panel->animF, WMScaled(237), WMScaled(10));
	WMSetFrameTitle(panel->animF, _("Animations"));

	panel->animB = WMCreateButton(panel->animF, WBTToggle);
	WMResizeWidget(panel->animB, WMScaled(64), WMScaled(64));
	WMMoveWidget(panel->animB, WMScaled(15), WMScaled(23));
	WMSetButtonFont(panel->animB, font);
	WMSetButtonText(panel->animB, _("Animations"));
	WMSetButtonImagePosition(panel->animB, WIPAbove);
	CreateImages(scr, rc, xis, ANIM_IMAGE, &altIcon, &icon);
	if (icon) {
		WMSetButtonImage(panel->animB, icon);
		WMReleasePixmap(icon);
	}
	if (altIcon) {
		WMSetButtonAltImage(panel->animB, altIcon);
		WMReleasePixmap(altIcon);
	}
	WMSetBalloonTextForView(_("Disable/enable animations such as those shown\n"
				  "for window miniaturization, shading etc."), WMWidgetView(panel->animB));

	panel->supB = WMCreateButton(panel->animF, WBTToggle);
	WMResizeWidget(panel->supB, WMScaled(64), WMScaled(64));
	WMMoveWidget(panel->supB, WMScaled(94), WMScaled(23));
	WMSetButtonFont(panel->supB, font);
	WMSetButtonText(panel->supB, _("Superfluous"));
	WMSetButtonImagePosition(panel->supB, WIPAbove);
	CreateImages(scr, rc, xis, SUPERF_IMAGE, &altIcon, &icon);
	if (icon) {
		WMSetButtonImage(panel->supB, icon);
		WMReleasePixmap(icon);
	}
	if (altIcon) {
		WMSetButtonAltImage(panel->supB, altIcon);
		WMReleasePixmap(altIcon);
	}
	WMSetBalloonTextForView(_("Disable/enable `superfluous' features and\n"
				  "animations. These include the `ghosting' of the\n"
				  "dock when it's being moved to another side and\n"
				  "the explosion animation when undocking icons."), WMWidgetView(panel->supB));

	WMMapSubwidgets(panel->animF);

    /*********** Dithering **********/
	panel->cmapSize = 4;

	panel->dithF = WMCreateFrame(panel->box);
	WMResizeWidget(panel->dithF, WMScaled(277), WMScaled(97));
	WMMoveWidget(panel->dithF, WMScaled(237), WMScaled(120));
	WMSetFrameTitle(panel->dithF, _("Dithering colormap for 8bpp"));

	WMSetBalloonTextForView(_("Number of colors to reserve for Window Maker\n"
				  "on displays that support only 8bpp (PseudoColor)."),
				WMWidgetView(panel->dithF));

	panel->dithB = WMCreateSwitchButton(panel->dithF);
	WMResizeWidget(panel->dithB, WMScaled(235), WMScaled(32));
	WMMoveWidget(panel->dithB, WMScaled(15), WMScaled(15));
	WMSetButtonText(panel->dithB, _("Disable dithering in any visual/depth"));

	panel->dithL = WMCreateLabel(panel->dithF);
	WMResizeWidget(panel->dithL, WMScaled(75), WMScaled(16));
	WMMoveWidget(panel->dithL, WMScaled(98), WMScaled(50));
	WMSetLabelTextAlignment(panel->dithL, WACenter);
	WMSetLabelText(panel->dithL, "64");

	panel->dithS = WMCreateSlider(panel->dithF);
	WMResizeWidget(panel->dithS, WMScaled(95), WMScaled(16));
	WMMoveWidget(panel->dithS, WMScaled(90), WMScaled(65));
	WMSetSliderMinValue(panel->dithS, 2);
	WMSetSliderMaxValue(panel->dithS, 6);
	WMSetSliderContinuous(panel->dithS, True);
	WMSetSliderAction(panel->dithS, updateLabel, panel);

	panel->dith1L = WMCreateLabel(panel->dithF);
	WMResizeWidget(panel->dith1L, WMScaled(80), WMScaled(35));
	WMMoveWidget(panel->dith1L, WMScaled(5), WMScaled(50));
	WMSetLabelTextAlignment(panel->dith1L, WACenter);
	WMSetLabelFont(panel->dith1L, font);
	WMSetLabelText(panel->dith1L, _("More colors for\napplications"));

	panel->dith2L = WMCreateLabel(panel->dithF);
	WMResizeWidget(panel->dith2L, WMScaled(80), WMScaled(35));
	WMMoveWidget(panel->dith2L, WMScaled(190), WMScaled(50));
	WMSetLabelTextAlignment(panel->dith2L, WACenter);
	WMSetLabelFont(panel->dith2L, font);
	WMSetLabelText(panel->dith2L, _("More colors for\nWindow Maker"));

	WMMapSubwidgets(panel->dithF);

	WMRealizeWidget(panel->box);
	WMMapSubwidgets(panel->box);

	if (xis)
		RReleaseImage(xis);
	WMReleaseFont(font);
	showData(panel);
}

static void storeData(_Panel *panel)
{
	int i;

	for (i = 0; i < 5; i++) {
		if (WMGetButtonSelected(panel->icoB[i]))
			break;
	}
	SetSpeedForKey(i, "IconSlideSpeed");

	for (i = 0; i < 5; i++) {
		if (WMGetButtonSelected(panel->shaB[i]))
			break;
	}
	SetSpeedForKey(i, "ShadeSpeed");

	if (WMGetButtonSelected(panel->newsB)) {
		SetStringForKey("new", "NewStyle");
	} else if (WMGetButtonSelected(panel->oldsB)) {
		SetStringForKey("old", "NewStyle");
	} else {
		SetStringForKey("next", "NewStyle");
	}
	SetBoolForKey(!WMGetButtonSelected(panel->animB), "DisableAnimations");
	SetBoolForKey(WMGetButtonSelected(panel->supB), "Superfluous");
	SetBoolForKey(WMGetButtonSelected(panel->smoB), "SmoothWorkspaceBack");
	SetBoolForKey(WMGetButtonSelected(panel->dithB), "DisableDithering");
	SetIntegerForKey(WMGetSliderValue(panel->dithS), "ColormapSize");
}

Panel *InitConfigurations(WMWidget *parent)
{
	_Panel *panel;

	panel = wmalloc(sizeof(_Panel));

	panel->sectionName = _("Other Configurations");
	panel->description = _("Animation speeds, titlebar styles, various option\n"
			       "toggling and number of colors to reserve for\n" "Window Maker on 8bit displays.");

	panel->parent = parent;
	panel->callbacks.createWidgets = createPanel;
	panel->callbacks.updateDomain = storeData;

	AddSection(panel, ICON_FILE);

	return panel;
}
