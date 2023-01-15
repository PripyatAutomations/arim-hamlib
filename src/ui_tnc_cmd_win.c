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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "bufq.h"
#include "ui.h"
#include "ui_themes.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"

WINDOW *tnc_cmd_win;
WINDOW *tnc_cmd_box;
int tnc_cmd_box_y, tnc_cmd_box_x, tnc_cmd_box_w, tnc_cmd_box_h;
int cmd_row, cur_cmd_row, cmd_col;
int max_cmd_rows;

void ui_print_cmd_win_title()
{
    int center, start = 1;

    box(tnc_cmd_box, 0, 0);
    center = (tnc_cmd_box_w / 2) - 1;
    start = center - 7;
    if (start < 1)
        start = 1;
    mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, " TNC COMMANDS ");
}

int ui_cmd_win_init(int y, int x, int width, int height)
{
    cmd_row = 0, cmd_col = 0, cur_cmd_row = 0;
    tnc_cmd_box_y = y;
    tnc_cmd_box_x = x;
    tnc_cmd_box_w = width;
    tnc_cmd_box_h = height;
    tnc_cmd_box = newwin(tnc_cmd_box_h, tnc_cmd_box_w, tnc_cmd_box_y, tnc_cmd_box_x);
    if (!tnc_cmd_box)
        return 0;
    if (color_code)
        wbkgd(tnc_cmd_box, COLOR_PAIR(7));
    box(tnc_cmd_box, 0, 0);
    tnc_cmd_win = derwin(tnc_cmd_box, tnc_cmd_box_h - 2, tnc_cmd_box_w - 2, 1, 1);
    if (!tnc_cmd_win)
        return 0;
    if (color_code)
        wbkgd(tnc_cmd_win, COLOR_PAIR(7));
    max_cmd_rows = tnc_cmd_box_h - 2;
    scrollok(tnc_cmd_win, TRUE);
    if (show_titles)
        ui_print_cmd_win_title();
    return 1;
}

void ui_print_cmd_in()
{
    char *p;

    if (!show_cmds)
        return;
    pthread_mutex_lock(&mutex_cmd_in);
    p = cmdq_pop(&g_cmd_in_q);
    while (p) {
        if (cur_cmd_row == max_cmd_rows) {
            wscrl(tnc_cmd_win, 1);
            cur_cmd_row--;
        }
        if (strlen(p) > (tnc_cmd_box_w - 4))
            p[tnc_cmd_box_w - 4] = '\0';
        if (color_code) {
            if (!strncasecmp(p, "<<", 2)) {
                wattrset(tnc_cmd_win, COLOR_PAIR(13)|themes[theme].tc_cmd_attr);
            } else if (!strncasecmp(p, ">> PTT TRUE", 11)) {
                wattrset(tnc_cmd_win, COLOR_PAIR(14)|themes[theme].tc_ptt_t_attr);
            } else if (!strncasecmp(p, ">> PTT FALSE", 12)) {
                wattrset(tnc_cmd_win, COLOR_PAIR(15)|themes[theme].tc_ptt_f_attr);
            } else if (!strncasecmp(p, ">> BUFFER", 9)) {
                wattrset(tnc_cmd_win, COLOR_PAIR(16)|themes[theme].tc_buf_attr);
            } else if (!strncasecmp(p, ">> PING", 7)) {
                wattrset(tnc_cmd_win, COLOR_PAIR(17)|themes[theme].tc_ping_attr);
            } else if (!strncasecmp(p, ">> BUSY", 7)) {
                wattrset(tnc_cmd_win, COLOR_PAIR(18)|themes[theme].tc_busy_attr);
            } else if (!strncasecmp(p, ">> NEWSTATE", 11)) {
                wattrset(tnc_cmd_win, COLOR_PAIR(19)|themes[theme].tc_newst_attr);
            }
        } else if (!strncasecmp(p, "<<", 2)) {
            wattrset(tnc_cmd_win, A_BOLD);
        } else {
            wattrset(tnc_cmd_win, A_NORMAL);
        }
        mvwprintw(tnc_cmd_win, cur_cmd_row, cmd_col, " %s", p);
        if (color_code)
            wattrset(tnc_cmd_win, COLOR_PAIR(7)|A_NORMAL);
        else
            wattrset(tnc_cmd_win, A_NORMAL);
        if (cur_cmd_row < max_cmd_rows)
            cur_cmd_row++;
        p = cmdq_pop(&g_cmd_in_q);
    }
    pthread_mutex_unlock(&mutex_cmd_in);
    if (!show_recents && !show_ptable && !show_ctable && !show_ftable) {
        touchwin(tnc_cmd_box);
        wrefresh(tnc_cmd_box);
    }
}

