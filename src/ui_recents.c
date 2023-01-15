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
#include "ui_tnc_cmd_win.h"

WINDOW *ui_recents_win;
int show_recents;
int recents_row, recents_col;
int cur_recents_row;
int max_recents_rows;

char recents_list[MAX_RECENTS_LIST_LEN+1][MAX_MBOX_HDR_SIZE];
int recents_list_cnt;
int recents_start_line;
int refresh_recents;

void ui_recents_init()
{
    recents_row = 0, recents_col = 1;
    cur_recents_row = recents_row;
}

void ui_recents_inc_start_line()
{
    recents_start_line++;
    if (recents_start_line >= recents_list_cnt)
        recents_start_line = recents_list_cnt - 1;
}

void ui_recents_dec_start_line()
{
    recents_start_line--;
    if (recents_start_line < 0)
        recents_start_line = 0;
}

void ui_clear_recents()
{
    pthread_mutex_lock(&mutex_recents);
    memset(&recents_list, 0, sizeof(recents_list));
    recents_list_cnt = 0;
    pthread_mutex_unlock(&mutex_recents);
    if (show_recents && ui_recents_win) {
        delwin(ui_recents_win);
        ui_recents_win = NULL;
        ui_recents_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_recents_win) {
            ui_print_status("Recents: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_recents_win, COLOR_PAIR(7));
        touchwin(ui_recents_win);
        wrefresh(ui_recents_win);
    }
    refresh_recents = recents_start_line = 0;
}

void ui_print_recents_title()
{
    int center, start;

    center = (tnc_cmd_box_w / 2) - 1;
    start = center - 9;
    if (start < 1)
        start = 1;
    mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, " RECENT MESSAGES ");
    wrefresh(tnc_cmd_box);
}

void ui_print_recents()
{
    static int once = 0;
    char *p, recent[MAX_MBOX_HDR_SIZE+8];
    int i, numch, max_cols, max_recents_rows, cur_recents_row = 0;

    if (!once) {
        once = 1;
        memset(&recents_list, 0, sizeof(recents_list));
    }

    if (show_recents && !ui_recents_win) {
        ui_recents_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_recents_win) {
            ui_print_status("Recents: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_recents_win, COLOR_PAIR(7));
        max_recents_rows = tnc_cmd_box_h - 2;
        if (show_titles)
            ui_print_recents_title();
        refresh_recents = 1;
    }

    pthread_mutex_lock(&mutex_recents);
    p = cmdq_pop(&g_recents_q);
    pthread_mutex_unlock(&mutex_recents);

    if (p) {
        snprintf(recent, sizeof(recent), "%s", p);
        memmove(&recents_list[1], &recents_list[0], MAX_RECENTS_LIST_LEN*MAX_MBOX_HDR_SIZE);
        numch = snprintf(recents_list[0], sizeof(recents_list[0]), "%s", recent);
        ++recents_list_cnt;
        if (recents_list_cnt > MAX_RECENTS_LIST_LEN)
            --recents_list_cnt;
        refresh_recents = 1;
    }
    if (show_recents && ui_recents_win && refresh_recents) {
        delwin(ui_recents_win);
        ui_recents_win = NULL;
        ui_recents_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_recents_win) {
            ui_print_status("Recents: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_recents_win, COLOR_PAIR(7));
        max_recents_rows = tnc_cmd_box_h - 2;
        if (show_titles)
            ui_print_cmd_win_title();
        refresh_recents = 0;
        max_cols = (tnc_cmd_box_w - 4) + 1;
        if (max_cols > sizeof(recent))
            max_cols = sizeof(recent);
        cur_recents_row = 0;
        for (i = recents_start_line; i < recents_list_cnt &&
             i < (max_recents_rows + recents_start_line); i++) {
            snprintf(recent, max_cols, "[%3d] %s", i + 1, recents_list[i]);
            mvwprintw(ui_recents_win, cur_recents_row, recents_col, "%s", recent);
            cur_recents_row++;
        }
        touchwin(ui_recents_win);
        wrefresh(ui_recents_win);
    } else if ((!show_recents && ui_recents_win)) {
        delwin(ui_recents_win);
        ui_recents_win = NULL;
        if (show_titles)
            ui_print_cmd_win_title();
        touchwin(tnc_cmd_box);
        wrefresh(tnc_cmd_box);
        refresh_recents = recents_start_line = 0;
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

void ui_refresh_recents()
{
    if (show_titles)
        ui_print_recents_title();
    refresh_recents = 1;
    ui_print_recents();
}

int ui_get_recent(int index, char *header, size_t size)
{
    if (index < MAX_RECENTS_LIST_LEN && recents_list[index][0]) {
        snprintf(header, size, "%s\n", recents_list[index]);
        return 1;
    }
    return 0;
}

int ui_set_recent_flag(const char *header, char flag)
{
    char *p;
    size_t i, len;

    for (i = 0; i < recents_list_cnt; i++) {
        len = strlen(recents_list[i]);
        if (!strncmp(header, recents_list[i], len)) {
            p = recents_list[i] + len;
            if (*(p - 4) == ' ') {
                switch(flag) {
                case 'R':
                    *(p - 3) = flag;
                    break;
                case 'F':
                    *(p - 2) = flag;
                    break;
                case 'S':
                    *(p - 1) = flag;
                    break;
                }
            }
            return 1;
        }
    }
    return 0;
}

