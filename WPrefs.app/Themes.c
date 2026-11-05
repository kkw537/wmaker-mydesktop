/* Themes.c- Theme stuff
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

#include <unistd.h>

typedef struct _Panel {
	WMBox *box;

	char *sectionName;

	CallbackRec callbacks;

	WMWidget *parent;

	WMButton *saveB;
	WMList *list;
	WMButton *loadB;
	WMButton *instB;

	WMFrame *totF;
	WMButton *totB;
	WMLabel *totL;

	WMFrame *botF;
	WMButton *botB;
	WMLabel *botL;

	pid_t tilePID;
	pid_t barPID;
} _Panel;

#define ICON_FILE	"theme"

static void showData(_Panel * panel)
{

}

static void finishedTileDownload(void *data)
{
	_Panel *panel = (_Panel *) data;

	WMSetButtonText(panel->totB, _("Set"));
	panel->tilePID = 0;
}

static void finishedBarDownload(void *data)
{
	_Panel *panel = (_Panel *) data;

	WMSetButtonText(panel->botB, _("Set"));
	panel->barPID = 0;
}

static pid_t downloadFile(WMScreen * scr, _Panel * panel, const char *file)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		werror("could not fork() process");

		WMRunAlertPanel(scr, GetWindow(), _("Error"),
				"Could not start download. fork() failed", _("OK"), NULL, NULL);
		return -1;
	}
	if (pid != 0) {
		return pid;
	}

	close(ConnectionNumber(WMScreenDisplay(scr)));

	exit(1);
}

static void downloadCallback(WMWidget * w, void *data)
{
	_Panel *panel = (_Panel *) data;
	pid_t newPid;
	WMButton *button = (WMButton *) w;
	pid_t *pid;

	if (button == panel->totB) {
		pid = &panel->tilePID;
	} else {
		pid = &panel->barPID;
	}

	if (*pid == 0) {
		newPid = downloadFile(WMWidgetScreen(w), panel, NULL);
		if (newPid < 0) {
			return;
		}
		WMSetButtonText(button, _("Stop"));

		if (button == panel->totB) {
			AddDeadChildHandler(newPid, finishedTileDownload, data);
		} else {
			AddDeadChildHandler(newPid, finishedBarDownload, data);
		}
		*pid = newPid;
	} else {
		*pid = 0;

		WMSetButtonText(button, _("Download"));
	}
}

static void createPanel(Panel * p)
{
	_Panel *panel = (_Panel *) p;

	panel->box = WMCreateBox(panel->parent);
	WMSetViewExpandsToParent(WMWidgetView(panel->box), WMScaled(2), WMScaled(2), WMScaled(2), WMScaled(2));

	panel->saveB = WMCreateCommandButton(panel->box);
	WMResizeWidget(panel->saveB, WMScaled(154), WMScaled(24));
	WMMoveWidget(panel->saveB, WMScaled(15), WMScaled(10));
	WMSetButtonText(panel->saveB, _("Save Current Theme"));

	panel->list = WMCreateList(panel->box);
	WMResizeWidget(panel->list, WMScaled(154), WMScaled(150));
	WMMoveWidget(panel->list, WMScaled(15), WMScaled(40));

	panel->loadB = WMCreateCommandButton(panel->box);
	WMResizeWidget(panel->loadB, WMScaled(74), WMScaled(24));
	WMMoveWidget(panel->loadB, WMScaled(15), WMScaled(200));
	WMSetButtonText(panel->loadB, _("Load"));

	panel->instB = WMCreateCommandButton(panel->box);
	WMResizeWidget(panel->instB, WMScaled(74), WMScaled(24));
	WMMoveWidget(panel->instB, WMScaled(95), WMScaled(200));
	WMSetButtonText(panel->instB, _("Install"));

    /**************** Tile of the day ****************/

	panel->totF = WMCreateFrame(panel->box);
	WMResizeWidget(panel->totF, WMScaled(210), WMScaled(105));
	WMMoveWidget(panel->totF, WMScaled(240), WMScaled(10));
	WMSetFrameTitle(panel->totF, _("Tile of The Day"));

	panel->totL = WMCreateLabel(panel->totF);
	WMResizeWidget(panel->totL, WMScaled(67), WMScaled(67));
	WMMoveWidget(panel->totL, WMScaled(25), WMScaled(25));
	WMSetLabelRelief(panel->totL, WRSunken);

	panel->totB = WMCreateCommandButton(panel->totF);
	WMResizeWidget(panel->totB, WMScaled(86), WMScaled(24));
	WMMoveWidget(panel->totB, WMScaled(105), WMScaled(45));
	WMSetButtonText(panel->totB, _("Download"));
	WMSetButtonAction(panel->totB, downloadCallback, panel);

	WMMapSubwidgets(panel->totF);

    /**************** Bar of the day ****************/

	panel->botF = WMCreateFrame(panel->box);
	WMResizeWidget(panel->botF, WMScaled(315), WMScaled(95));
	WMMoveWidget(panel->botF, WMScaled(190), WMScaled(125));
	WMSetFrameTitle(panel->botF, _("Bar of The Day"));

	panel->botL = WMCreateLabel(panel->botF);
	WMResizeWidget(panel->botL, WMScaled(285), WMScaled(32));
	WMMoveWidget(panel->botL, WMScaled(15), WMScaled(20));
	WMSetLabelRelief(panel->botL, WRSunken);

	panel->botB = WMCreateCommandButton(panel->botF);
	WMResizeWidget(panel->botB, WMScaled(86), WMScaled(24));
	WMMoveWidget(panel->botB, WMScaled(110), WMScaled(60));
	WMSetButtonText(panel->botB, _("Download"));
	WMSetButtonAction(panel->botB, downloadCallback, panel);

	WMMapSubwidgets(panel->botF);

	WMRealizeWidget(panel->box);
	WMMapSubwidgets(panel->box);

	showData(panel);
}

static void storeData(_Panel * panel)
{
}

Panel *InitThemes(WMWidget *parent)
{
	_Panel *panel;

	panel = wmalloc(sizeof(_Panel));

	panel->sectionName = _("Themes");

	panel->parent = parent;

	panel->callbacks.createWidgets = createPanel;
	panel->callbacks.updateDomain = storeData;

	AddSection(panel, ICON_FILE);

	return panel;
}
