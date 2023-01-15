/***********************************************************************

    ARIM Amateur Radio Instant Messaging program for the ARDOP TNC.

    Copyright (C) 2016-2021 Robert Cunnings NW8L

    This file is part of the ARIM messaging program.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*************************************************************************/

#ifndef _TNC_ATTACH_H_INCLUDED_
#define _TNC_ATTACH_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ver {
    int major;
    int minor;
    int revision;
    int build;
    int vendor;
} TNC_VERSION;

extern TNC_VERSION g_tnc_version;
extern int tnc_attach(int which);
extern void tnc_detach(void);
extern void tnc_get_version(const char *str);

#ifdef __cplusplus
}
#endif

#endif

