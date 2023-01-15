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
#include "util.h"
#include "ardop_data.h"

WINDOW *ui_ftable_win;
int show_ftable;
int ftable_row, cur_ftable_row, ftable_col;
int max_ftable_rows;

typedef struct ft {
    char call[16];
    char fname[70];
    char size[8];
    char check[8];
    int compressed;
    int inbound;
    int done;
    time_t start_time;
    time_t stop_time;
} FT_ENTRY;
FT_ENTRY ftable_list[MAX_FTABLE_LIST_LEN+1];
int ftable_list_cnt;
int ftable_start_line;
int refresh_ftable;

void ui_ftable_init()
{
    ftable_row = 0, ftable_col = 1;
    cur_ftable_row = ftable_row;
}

void ui_ftable_inc_start_line()
{
    ftable_start_line++;
    if (ftable_start_line >= ftable_list_cnt)
        ftable_start_line = ftable_list_cnt - 1;
}

void ui_ftable_dec_start_line()
{
    ftable_start_line--;
    if (ftable_start_line < 0)
        ftable_start_line = 0;
}

void ui_clear_ftable()
{
    pthread_mutex_lock(&mutex_ftable);
    memset(&ftable_list, 0, sizeof(ftable_list));
    pthread_mutex_unlock(&mutex_ftable);
    if (show_ftable && ui_ftable_win) {
        delwin(ui_ftable_win);
        ui_ftable_win = NULL;
        ui_ftable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ftable_win) {
            ui_print_status("ARQ File History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ftable_win, COLOR_PAIR(7));
        touchwin(ui_ftable_win);
        wrefresh(ui_ftable_win);
    }
    refresh_ftable = ftable_list_cnt = ftable_start_line = 0;
}

void ui_print_ftable_title()
{
    int center, start;

    center = (tnc_cmd_box_w / 2) - 1;
    start = center - 10;
    if (start < 1)
        start = 1;
    mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, " ARQ FILE HISTORY ");
    wrefresh(tnc_cmd_box);
}

void ui_print_ftable()
{
    static int once = 0;
    char *p, file_data[MAX_FTABLE_ROW_SIZE];
    char start_time[24], elapsed_time[16];
    int max_cols, max_ftable_rows, cur_ftable_row = 0;
    int i, hours, minutes, secs, numch;
    time_t telapsed;

    if (!once) {
        once = 1;
        memset(&ftable_list, 0, sizeof(ftable_list));
    }

    if (show_ftable && !ui_ftable_win) {
        ui_ftable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ftable_win) {
            ui_print_status("ARQ File History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ftable_win, COLOR_PAIR(7));
        max_ftable_rows = tnc_cmd_box_h - 2;
        if (show_titles)
            ui_print_ftable_title();
        refresh_ftable = 1;
    }

    pthread_mutex_lock(&mutex_ftable);
    p = cmdq_pop(&g_ftable_q);
    pthread_mutex_unlock(&mutex_ftable);

    /*
      layout of record taken from queue:
         byte 0:     type 'O' outbound, 'I' inbound, 'D' outbound done, 'S' inbound starting
         byte 1:     compression flag 'Z' compressed or ' ' not compressed
         byte 2-13:  remote station call sign
         byte 14-19: file size
         byte 20-23: checksum
         byte 24-87: file name
    */

    if (p) {
        if (p[0] == 'O') {
            /* outbound transfer is starting, intialize entry */
            memmove(&ftable_list[1], &ftable_list[0], MAX_FTABLE_LIST_LEN * sizeof(FT_ENTRY));
            memset(&ftable_list[0], 0, sizeof(FT_ENTRY));
            ftable_list[0].inbound = 0;
            ftable_list[0].compressed = (p[1] == 'Z' ? 1 : 0);
            snprintf(ftable_list[0].call, sizeof(ftable_list[0].call), "%.11s", &p[2]);
            snprintf(ftable_list[0].size, sizeof(ftable_list[0].size), "%.6s", &p[14]);
            snprintf(ftable_list[0].check, sizeof(ftable_list[0].check), "%.4s", &p[20]);
            snprintf(ftable_list[0].fname, sizeof(ftable_list[0].fname), "%.64s", &p[24]);
            ftable_list[0].start_time = time(NULL);
            ++ftable_list_cnt;
            if (ftable_list_cnt > MAX_FTABLE_LIST_LEN)
                --ftable_list_cnt;
        } else if (p[0] == 'D' && !ftable_list[0].done) {
            /* outbound transfer is done */
            ftable_list[0].stop_time = time(NULL);
            ftable_list[0].done = 1;
        } else if (p[0] == 'I') {
            /* inbound transfer is done but did start signal arrive first? */
            if (!ftable_list_cnt || ftable_list[0].done) {
                /* didn't receive 'S' (start) signal yet, initialize entry here */
                memmove(&ftable_list[1], &ftable_list[0], MAX_FTABLE_LIST_LEN * sizeof(FT_ENTRY));
                memset(&ftable_list[0], 0, sizeof(FT_ENTRY));
                ++ftable_list_cnt;
                if (ftable_list_cnt > MAX_FTABLE_LIST_LEN)
                    --ftable_list_cnt;
                ftable_list[0].done = 2; /* signal 'I' out-of-order w.r.t signal 'S' */
                ftable_list[0].start_time = time(NULL);
            }
            ftable_list[0].inbound = 1;
            ftable_list[0].compressed = (p[1] == 'Z' ? 1 : 0);
            snprintf(ftable_list[0].call, sizeof(ftable_list[0].call), "%.11s", &p[2]);
            snprintf(ftable_list[0].size, sizeof(ftable_list[0].size), "%.6s", &p[14]);
            snprintf(ftable_list[0].check, sizeof(ftable_list[0].check), "%.4s", &p[20]);
            snprintf(ftable_list[0].fname, sizeof(ftable_list[0].fname), "%.64s", &p[24]);
            ftable_list[0].stop_time = time(NULL);
            if (!ftable_list[0].done)
                ftable_list[0].done = 1;
        } else if (p[0] == 'S' && (!ftable_list_cnt || ftable_list[0].done == 1)) {
            /* inbound transfer is starting, initialize entry. Ignored if
             * arriving out-of-order, in which case ftable_list[0].done == 2 */
            memmove(&ftable_list[1], &ftable_list[0], MAX_FTABLE_LIST_LEN * sizeof(FT_ENTRY));
            memset(&ftable_list[0], 0, sizeof(FT_ENTRY));
            ftable_list[0].start_time = time(NULL);
            ++ftable_list_cnt;
            if (ftable_list_cnt > MAX_FTABLE_LIST_LEN)
                --ftable_list_cnt;
        } else {
            if (!ftable_list[0].done && !ftable_list[0].inbound) {
                /* outbound file transfer failed, restore previous state */
                memmove(&ftable_list[0], &ftable_list[1], MAX_FTABLE_LIST_LEN * sizeof(FT_ENTRY));
                --ftable_list_cnt;
                if (ftable_list_cnt < 0)
                    ftable_list_cnt = 0;
            }
        }
        refresh_ftable = 1;
    }
    if (show_ftable && ui_ftable_win && refresh_ftable) {
        delwin(ui_ftable_win);
        ui_ftable_win = NULL;
        ui_ftable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ftable_win) {
            ui_print_status("ARQ File History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ftable_win, COLOR_PAIR(7));
        max_ftable_rows = tnc_cmd_box_h - 2;
        if (show_titles)
            ui_print_ftable_title();
        refresh_ftable = 0;
        max_cols = (tnc_cmd_box_w - 4) + 1;
        if (max_cols > sizeof(file_data))
            max_cols = sizeof(file_data);
        cur_ftable_row = 0;
        for (i = ftable_start_line; i < ftable_list_cnt &&
             i < (max_ftable_rows + ftable_start_line); i++) {
            if (ftable_list[i].done && ftable_list[i].start_time && ftable_list[i].stop_time) {
                util_clock_tm(ftable_list[i].start_time, start_time, sizeof(start_time));
                telapsed = ftable_list[i].stop_time - ftable_list[i].start_time;
                hours = telapsed / (60*60);
                if (hours > 99)
                    hours = 99;
                telapsed = telapsed % (60*60);
                minutes = telapsed / 60;
                telapsed = telapsed % 60;
                secs = telapsed;
                /* if file very small, 'I' signal may arrive before 'S' signal, so elapsed
                 * time is zero. If so make it 1 second. */
                if (secs == 0 && minutes == 0 && hours == 0)
                    secs = 1;
                snprintf(elapsed_time, sizeof(elapsed_time),
                            "%02d:%02d:%02d", hours, minutes, secs);
                numch = snprintf(file_data, sizeof(file_data), "[%2d] %s %.11s %s [%s] %s %.6s bytes %.4s %s",
                                 i + 1, ftable_list[i].inbound ? ">>" : "<<", ftable_list[i].call,
                                     start_time, elapsed_time, ftable_list[i].compressed ? "-z" : "  ",
                                         ftable_list[i].size, ftable_list[i].check, ftable_list[i].fname);
                if (numch >= sizeof(file_data))
                    ui_truncate_line(file_data, sizeof(file_data));
                mvwprintw(ui_ftable_win, cur_ftable_row, ftable_col, "%s", file_data);
                cur_ftable_row++;
            }
        }
        touchwin(ui_ftable_win);
        wrefresh(ui_ftable_win);
    } else if ((!show_ftable && ui_ftable_win)) {
        delwin(ui_ftable_win);
        ui_ftable_win = NULL;
        if (show_titles)
            ui_print_cmd_win_title();
        touchwin(tnc_cmd_box);
        wrefresh(tnc_cmd_box);
        refresh_ftable = ftable_start_line = 0;
    }
}

void ui_refresh_ftable()
{
    if (show_titles)
        ui_print_ftable_title();
    refresh_ftable = 1;
    ui_print_ftable();
}

