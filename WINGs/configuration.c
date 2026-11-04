
#include "WINGsP.h"
#include "wconfig.h"

#include <X11/Xlocale.h>

_WINGsConfiguration WINGsConfiguration;

#define SYSTEM_FONT "sans serif"
#define BOLD_SYSTEM_FONT "sans serif:bold"
#define DEFAULT_FONT_SIZE 12

#define FLOPPY_PATH "/floppy"

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

/* ---[ GUI scale factor (GSScaleFactor) ]--------------------------------- */

#define MIN_SCALE_FACTOR 0.5F
#define MAX_SCALE_FACTOR 8.0F

static float scaleFactor = 1.0F;
static Bool scaleFactorLoaded = False;

static float sanitizeScaleFactor(float factor)
{
	if (!(factor > 0.0F))	/* also catches NaN */
		return 1.0F;
	if (factor < MIN_SCALE_FACTOR)
		return MIN_SCALE_FACTOR;
	if (factor > MAX_SCALE_FACTOR)
		return MAX_SCALE_FACTOR;
	return factor;
}

static void loadScaleFactor(void)
{
	WMUserDefaults *defaults;
	const char *env;
	float factor = 0.0F;

	scaleFactorLoaded = True;

	env = getenv("GS_SCALE_FACTOR");
	if (env && *env) {
		char *end;

		factor = strtof(env, &end);
		if (end == env)
			factor = 0.0F;
	}

	if (!(factor > 0.0F)) {
		defaults = WMGetStandardUserDefaults();
		if (defaults && WMGetUDObjectForKey(defaults, "GSScaleFactor"))
			factor = WMGetUDFloatForKey(defaults, "GSScaleFactor");
	}

	scaleFactor = sanitizeScaleFactor(factor);
}

float WMGetScaleFactor(void)
{
	if (!scaleFactorLoaded)
		loadScaleFactor();

	return scaleFactor;
}

void WMSetScaleFactor(float factor)
{
	scaleFactorLoaded = True;
	scaleFactor = sanitizeScaleFactor(factor);
}

static int roundScaled(double value)
{
	if (value >= 0)
		return (int)(value + 0.5);
	return -(int)(-value + 0.5);
}

int WMScaleValue(int value)
{
	float factor = WMGetScaleFactor();
	int result;

	if (factor == 1.0F || value == 0)
		return value;

	result = roundScaled((double)value * factor);

	/* never make a non-zero value (eg: a 1 pixel line) disappear */
	if (result == 0)
		result = (value > 0) ? 1 : -1;

	return result;
}

int WMUnscaleValue(int value)
{
	float factor = WMGetScaleFactor();

	if (factor == 1.0F || value == 0)
		return value;

	return roundScaled((double)value / factor);
}

RImage *WMScaleImageByFactor(RImage *image)
{
	unsigned int width, height;

	if (!image)
		return NULL;

	width = WMScaleValue(image->width);
	height = WMScaleValue(image->height);

	if (width == image->width && height == image->height)
		return RRetainImage(image);

	/* Integer factors are done by plain pixel replication, so that the
	 * result looks exactly like the image drawn with "bigger pixels".
	 * Fractional factors need some smoothing to look acceptable. */
	if (width % image->width == 0 && height % image->height == 0)
		return RScaleImage(image, width, height);

	return RSmoothScaleImage(image, width, height);
}
