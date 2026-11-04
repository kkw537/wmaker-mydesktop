
#include "WINGsP.h"
#include "wconfig.h"

#include <X11/Xlocale.h>

_WINGsConfiguration WINGsConfiguration;

#define SYSTEM_FONT "sans serif"
#define BOLD_SYSTEM_FONT "sans serif:bold"
#define DEFAULT_FONT_SIZE 12

#define FLOPPY_PATH "/floppy"

#define MIN_SCALE_FACTOR 0.5
#define MAX_SCALE_FACTOR 8.0

/* Scale factor currently in use by WMScale() and the font code */
double WMScaleFactor = 1.0;

static double readScaleFactorFromPL(WMPropList *val)
{
	char *str, *end;
	double value;

	if (!val || !WMIsPLString(val))
		return 0.0;

	str = WMGetFromPLString(val);
	if (!str)
		return 0.0;

	value = strtod(str, &end);
	/* accept a decimal comma too, whatever the current locale is */
	if (*end == ',' || *end == '.') {
		char *frac = end + 1, *fend;
		double f = strtod(frac, &fend);

		if (fend > frac) {
			int digits = fend - frac;

			while (digits-- > 0)
				f /= 10.0;
			value += f;
		}
	}

	return value;
}

/*
 * GNUstep keeps its global settings in the NSGlobalDomain domain, which is
 * not one of the domains WINGs reads, so look at it directly as a fallback.
 */
static double readGNUstepScaleFactor(void)
{
	static const char *const names[] = { "NSGlobalDomain.plist", "NSGlobalDomain" };
	double value = 0.0;
	int i;

	for (i = 0; i < wlengthof(names) && value <= 0.0; i++) {
		char *path = wdefaultspathfordomain(names[i]);
		WMPropList *dict, *key;

		if (!path)
			continue;
		dict = WMReadPropListFromFile(path);
		wfree(path);
		if (!dict)
			continue;
		if (WMIsPLDictionary(dict)) {
			key = WMCreatePLString("GSScaleFactor");
			value = readScaleFactorFromPL(WMGetFromPLDictionary(dict, key));
			WMReleasePropList(key);
		}
		WMReleasePropList(dict);
	}

	return value;
}

/*
 * Returns the GSScaleFactor set by the user (WMGLOBAL first, then GNUstep's
 * NSGlobalDomain), or def_factor when the user did not set one. This can be
 * called before any screen is created.
 */
double WMGetUserScaleFactorWithDefault(double def_factor)
{
	static double factor = 0.0;
	static Bool looked_up = False;
	WMUserDefaults *defaults;
	double value;

	if (!looked_up) {
		looked_up = True;
		defaults = WMGetStandardUserDefaults();
		if (defaults)
			factor = readScaleFactorFromPL(WMGetUDObjectForKey(defaults, "GSScaleFactor"));
		if (factor <= 0.0)
			factor = readGNUstepScaleFactor();
	}

	value = (factor > 0.0) ? factor : def_factor;

	if (value <= 0.0)
		value = 1.0;
	else if (value < MIN_SCALE_FACTOR)
		value = MIN_SCALE_FACTOR;
	else if (value > MAX_SCALE_FACTOR)
		value = MAX_SCALE_FACTOR;

	return value;
}

/*
 * Returns the GSScaleFactor set by the user, or 1.0.
 */
double WMGetUserScaleFactor(void)
{
	return WMGetUserScaleFactorWithDefault(1.0);
}

double WMGetScaleFactor(void)
{
	return WMScaleFactor;
}

void WMSetScaleFactor(double factor)
{
	if (factor < MIN_SCALE_FACTOR)
		factor = MIN_SCALE_FACTOR;
	if (factor > MAX_SCALE_FACTOR)
		factor = MAX_SCALE_FACTOR;
	WMScaleFactor = factor;
}

static unsigned getButtonWithName(const char *name, unsigned defaultButton)
{
	if (strncmp(name, "Button", 6) == 0 && strlen(name) == 7) {
		switch (name[6]) {
		case '1':
			return Button1;
		case '2':
			return Button2;
		case '3':
			return Button3;
		case '4':
			return Button4;
		case '5':
			return Button5;
		default:
			break;
		}
	}

	return defaultButton;
}

void W_ReadConfigurations(void)
{
	WMUserDefaults *defaults;
	Bool aaIsSet = False;

	memset(&WINGsConfiguration, 0, sizeof(_WINGsConfiguration));

	defaults = WMGetStandardUserDefaults();

	if (defaults) {
		char *buttonName;
		WMPropList *val;
		unsigned button;

		WINGsConfiguration.systemFont = WMGetUDStringForKey(defaults, "SystemFont");

		WINGsConfiguration.boldSystemFont = WMGetUDStringForKey(defaults, "BoldSystemFont");

		val = WMGetUDObjectForKey(defaults, "AntialiasedText");
		if (val && WMIsPLString(val) && WMGetFromPLString(val)) {
			aaIsSet = True;
			WINGsConfiguration.antialiasedText =
				WMGetUDBoolForKey(defaults, "AntialiasedText");
		}

		WINGsConfiguration.doubleClickDelay = WMGetUDIntegerForKey(defaults, "DoubleClickTime");

		WINGsConfiguration.floppyPath = WMGetUDStringForKey(defaults, "FloppyPath");

		buttonName = WMGetUDStringForKey(defaults, "MouseWheelUp");
		if (buttonName) {
			button = getButtonWithName(buttonName, Button4);
			wfree(buttonName);
		} else {
			button = Button4;
		}
		WINGsConfiguration.mouseWheelUp = button;

		buttonName = WMGetUDStringForKey(defaults, "MouseWheelDown");
		if (buttonName) {
			button = getButtonWithName(buttonName, Button5);
			wfree(buttonName);
		} else {
			button = Button5;
		}
		WINGsConfiguration.mouseWheelDown = button;

		if (WINGsConfiguration.mouseWheelDown == WINGsConfiguration.mouseWheelUp) {
			WINGsConfiguration.mouseWheelUp = Button4;
			WINGsConfiguration.mouseWheelDown = Button5;
		}

		WINGsConfiguration.defaultFontSize = WMGetUDIntegerForKey(defaults, "DefaultFontSize");
	}

	if (!WINGsConfiguration.systemFont) {
		WINGsConfiguration.systemFont = SYSTEM_FONT;
	}
	if (!WINGsConfiguration.boldSystemFont) {
		WINGsConfiguration.boldSystemFont = BOLD_SYSTEM_FONT;
	}
	if (WINGsConfiguration.defaultFontSize == 0) {
		WINGsConfiguration.defaultFontSize = DEFAULT_FONT_SIZE;
	}
	if (!aaIsSet) {
		WINGsConfiguration.antialiasedText = True;
	}
	if (!WINGsConfiguration.floppyPath) {
		WINGsConfiguration.floppyPath = FLOPPY_PATH;
	}
	if (WINGsConfiguration.doubleClickDelay == 0) {
		WINGsConfiguration.doubleClickDelay = 250;
	}
	if (WINGsConfiguration.mouseWheelUp == 0) {
		WINGsConfiguration.mouseWheelUp = Button4;
	}
	if (WINGsConfiguration.mouseWheelDown == 0) {
		WINGsConfiguration.mouseWheelDown = Button5;
	}

}

unsigned W_getconf_mouseWheelUp(void)
{
	return WINGsConfiguration.mouseWheelUp;
}

unsigned W_getconf_mouseWheelDown(void)
{
	return WINGsConfiguration.mouseWheelDown;
}

void W_setconf_doubleClickDelay(int value)
{
	WINGsConfiguration.doubleClickDelay = value;
}
