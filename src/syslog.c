/*
**  igmpproxy - IGMP proxy based multicast router
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, either version 2 or
**    any later version.
**
**  mrouted 3.9-beta3 - Copyright (C) 2002 by The Board of Trustees of
**  Leland Stanford Junior University.
**  - Licensed under the 3-clause BSD license, see Stanford.txt file.
**
*/

#include "igmpproxy.h"
#include <time.h>

int LogLevel = LOG_WARNING;
bool Log2Stderr = false;
// Map color codes to ANSI escape sequences
const char* color_codes[] = {
    "\x1b[0m",   // Reset
    "\x1b[31m",  // Red
    "\x1b[32m",  // Green
    "\x1b[33m",  // Yellow
    "\x1b[34m",  // Blue
    "\x1b[35m",  // Magenta
    "\x1b[36m",  // Cyan
    "\x1b[37m",  // White
    "\x1b[91m",  // Bright Red
    "\x1b[92m",  // Bright Green
    "\x1b[93m",  // Bright Yellow
    "\x1b[94m",  // Bright Blue
    "\x1b[95m",  // Bright Magenta
    "\x1b[96m",  // Bright Cyan
    "\x1b[97m"   // Bright White
};

void my_log(int Severity, int color_code, int Errno, const char *FmtSt, ...) {
    char LogMsg[256];
    char TimeBuf[64];
    time_t now = time(NULL);
    struct tm *now_tm = localtime(&now);

    strftime(TimeBuf, sizeof(TimeBuf), "%Y/%m/%d %H:%M:%S", now_tm);

    va_list ArgPt;
    va_start(ArgPt, FmtSt);

    // Use color_codes array for both the selected color and the reset color
    const char *color = (color_code >= 0 && color_code < sizeof(color_codes) / sizeof(char*))
                        ? color_codes[color_code]
                        : color_codes[COLOR_CODE_RESET];  // Use reset color code

    char fullFmt[300]; // Ensure buffer is large enough
    snprintf(fullFmt, sizeof(fullFmt), "%s%s - %s%s", color, TimeBuf, FmtSt, color_codes[COLOR_CODE_RESET]);

    vsnprintf(LogMsg, sizeof(LogMsg), fullFmt, ArgPt);
    va_end(ArgPt);

    if (Errno > 0) {
        char ErrMsg[128];
        snprintf(ErrMsg, sizeof(ErrMsg), "; Errno(%d): %s", Errno, strerror(Errno));
        strncat(LogMsg, ErrMsg, sizeof(LogMsg) - strlen(LogMsg) - 1);
    }

    if (Severity <= LogLevel) {
        if (Log2Stderr)
            fprintf(stderr, "%s\n", LogMsg);
        else
            syslog(Severity, "%s", LogMsg);
    }

    if (Severity <= LOG_ERR)
        exit(-1);
}
