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

WINDOW *ui_ctable_win;
int show_ctable;
int ctable_row, cur_ctable_row, ctable_col;
int max_ctable_rows;

typedef struct ct {
    char call[16];
    char gridsq[12];
    char arqbw[12];
    int inbound;
    int disconnected;
    time_t start_time;
    time_t stop_time;
    size_t num_bytes_in;
    size_t num_bytes_out;
} CT_ENTRY;
CT_ENTRY ctable_list[MAX_CTABLE_LIST_LEN+1];
int ctable_list_cnt;
int ctable_start_line;
int refresh_ctable;

void ui_ctable_init()
{
    ctable_row = 0, ctable_col = 1;
    cur_ctable_row = ctable_row;
}

void ui_ctable_inc_start_line()
{
    ctable_start_line++;
    if (ctable_start_line >= ctable_list_cnt)
        ctable_start_line = ctable_list_cnt - 1;
}

void ui_ctable_dec_start_line()
{
    ctable_start_line--;
    if (ctable_start_line < 0)
        ctable_start_line = 0;
}

void ui_clear_ctable()
{
    pthread_mutex_lock(&mutex_ctable);
    memset(&ctable_list, 0, sizeof(ctable_list));
    pthread_mutex_unlock(&mutex_ctable);
    if (show_ctable && ui_ctable_win) {
        delwin(ui_ctable_win);
        ui_ctable_win = NULL;
        ui_ctable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ctable_win) {
            ui_print_status("Connection History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ctable_win, COLOR_PAIR(7));
        touchwin(ui_ctable_win);
        wrefresh(ui_ctable_win);
    }
    refresh_ctable = ctable_list_cnt = ctable_start_line = 0;
}

void ui_print_ctable_title()
{
    int center, start;

    center = (tnc_cmd_box_w / 2) - 1;
    start = center - 10;
    if (start < 1)
        start = 1;
    mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, " CONNECTION HISTORY ");
    wrefresh(tnc_cmd_box);
}

void ui_print_ctable()
{
    static int once = 0;
    char *p, conn_data[MAX_CTABLE_ROW_SIZE];
    char start_time[24], elapsed_time[16];
    int max_cols, max_ctable_rows, cur_ctable_row = 0;
    int i, hours, minutes, secs;
    time_t telapsed;

    if (!once) {
        once = 1;
        memset(&ctable_list, 0, sizeof(ctable_list));
    }

    if (show_ctable && !ui_ctable_win) {
        ui_ctable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ctable_win) {
            ui_print_status("Connection History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ctable_win, COLOR_PAIR(7));
        max_ctable_rows = tnc_cmd_box_h - 2;
        if (show_titles)
            ui_print_ctable_title();
        refresh_ctable = 1;
    }

    pthread_mutex_lock(&mutex_ctable);
    p = cmdq_pop(&g_ctable_q);
    pthread_mutex_unlock(&mutex_ctable);

    /*
      layout of record taken from queue:
         byte 1:     inbound/outbound flag
         byte 2-13:  remote station call sign
         byte 14-21: remote/local station grid square
         byte 22-31: arq bandwidth
    */

    if (p) {
        if (p[0] == 'C') {
            memmove(&ctable_list[1], &ctable_list[0], MAX_CTABLE_LIST_LEN * sizeof(CT_ENTRY));
            memset(&ctable_list[0], 0, sizeof(CT_ENTRY));
            ctable_list[0].inbound = (p[1] == 'I' ? 1 : 0);
            snprintf(ctable_list[0].call, sizeof(ctable_list[0].call), "%.11s", &p[2]);
            snprintf(ctable_list[0].gridsq, sizeof(ctable_list[0].gridsq), "%.8s", &p[14]);
            snprintf(ctable_list[0].arqbw, sizeof(ctable_list[0].arqbw), "%.4s", &p[22]);
            ctable_list[0].start_time = time(NULL);
            ++ctable_list_cnt;
            if (ctable_list_cnt > MAX_CTABLE_LIST_LEN)
                --ctable_list_cnt;
        } else {
            ctable_list[0].stop_time = time(NULL);
            ctable_list[0].num_bytes_in = ardop_data_get_num_bytes_in();
            ctable_list[0].num_bytes_out = ardop_data_get_num_bytes_out();
            ctable_list[0].disconnected = 1;
        }
        refresh_ctable = 1;
    }
    if (show_ctable && ui_ctable_win && refresh_ctable) {
        delwin(ui_ctable_win);
        ui_ctable_win = NULL;
        ui_ctable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ctable_win) {
            ui_print_status("Connection History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ctable_win, COLOR_PAIR(7));
        max_ctable_rows = tnc_cmd_box_h - 2;
        if (show_titles)
            ui_print_ctable_title();
        refresh_ctable = 0;
        max_cols = (tnc_cmd_box_w - 4) + 1;
        if (max_cols > sizeof(conn_data))
            max_cols = sizeof(conn_data);
        cur_ctable_row = 0;
        for (i = ctable_start_line; i < ctable_list_cnt &&
             i < (max_ctable_rows + ctable_start_line); i++) {
            if (ctable_list[i].disconnected &&
                ctable_list[i].start_time &&
                ctable_list[i].stop_time) {
                /* print only when ARQ session has ended */
                util_clock_tm(ctable_list[i].start_time, start_time, sizeof(start_time));
                telapsed = ctable_list[i].stop_time - ctable_list[i].start_time;
                hours = telapsed / (60*60);
                if (hours > 99)
                    hours = 99;
                telapsed = telapsed % (60*60);
                minutes = telapsed / 60;
                telapsed = telapsed % 60;
                secs = telapsed;
                snprintf(elapsed_time, sizeof(elapsed_time),
                            "%02d:%02d:%02d", hours, minutes, secs);
                snprintf(conn_data, max_cols, "[%2d] %s %.11s [%.8s] %s [%s] In: %7zu Out: %7zu BW=%s",
                         i + 1, ctable_list[i].inbound ? ">>" : "<<", ctable_list[i].call,
                             ctable_list[i].gridsq, start_time, elapsed_time,
                                 ctable_list[i].num_bytes_in, ctable_list[i].num_bytes_out,
                                     ctable_list[i].arqbw);
                mvwprintw(ui_ctable_win, cur_ctable_row, ctable_col, "%s", conn_data);
                cur_ctable_row++;
            }
        }
        touchwin(ui_ctable_win);
        wrefresh(ui_ctable_win);
    } else if ((!show_ctable && ui_ctable_win)) {
        delwin(ui_ctable_win);
        ui_ctable_win = NULL;
        if (show_titles)
            ui_print_cmd_win_title();
        touchwin(tnc_cmd_box);
        wrefresh(tnc_cmd_box);
        refresh_ctable = ctable_start_line = 0;
    }
}

void ui_refresh_ctable()
{
    if (show_titles)
        ui_print_ctable_title();
    refresh_ctable = 1;
    ui_print_ctable();
}

