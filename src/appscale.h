/* appscale.h - pass the GUI scale factor on to the other applications
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

#ifndef WMAPPSCALE_H_
#define WMAPPSCALE_H_

#include "screen.h"

/* publish the GUI scale factor for the toolkits of the other applications
 * (XSETTINGS, Xft.dpi resource, environment), if ScaleApplications is set */
void wAppScaleInit(WScreen *scr);

/* undo it, when Window Maker exits */
void wAppScaleRestore(WScreen *scr);

#endif /* WMAPPSCALE_H_ */
