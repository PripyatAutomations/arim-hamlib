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

#ifndef _UI_RECENTS_H_INCLUDED_
#define _UI_RECENTS_H_INCLUDED_

extern int show_recents;
extern int recents_list_cnt;

extern void ui_recents_init(void);
extern void ui_print_recents(void);
extern void ui_refresh_recents(void);
extern void ui_clear_recents(void);
extern void ui_recents_inc_start_line(void);
extern void ui_recents_dec_start_line(void);
extern int ui_get_recent(int index, char *header, size_t size);
extern int ui_set_recent_flag(const char *header, char flag);

#endif

