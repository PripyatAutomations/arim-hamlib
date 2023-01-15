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

#ifndef _UI_TNC_DATA_WIN_H_INCLUDED_
#define _UI_TNC_DATA_WIN_H_INCLUDED_

#include <curses.h>

extern WINDOW *tnc_data_box;
extern int tnc_data_box_y, tnc_data_box_x, tnc_data_box_w, tnc_data_box_h;

extern int ui_data_win_init(int y, int x, int width, int height);
extern void ui_refresh_data_win(void);
extern void ui_print_data_in(void);
extern void ui_print_data_win_title(void);
extern void ui_data_win_on_key_home(void);
extern void ui_data_win_on_key_end(void);
extern void ui_data_win_on_key_pg_up(void);
extern void ui_data_win_on_key_pg_dwn(void);
extern void ui_data_win_on_key_up(void);
extern void ui_data_win_on_key_dwn(void);
extern void ui_data_win_on_end_scroll(void);
extern void ui_clear_data_in(void);

#endif

