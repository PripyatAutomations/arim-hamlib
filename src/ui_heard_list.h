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

#ifndef _UI_HEARD_LIST_H_INCLUDED_
#define _UI_HEARD_LIST_H_INCLUDED_

#include <curses.h>

extern WINDOW *ui_list_box;

extern int ui_heard_list_init(int y, int x, int width, int height);
extern int ui_heard_list_get_width(void);
extern void ui_print_heard_list(void);
extern void ui_refresh_heard_list(void);
extern void ui_get_heard_list(char *listbuf, size_t listbufsize);

#endif

