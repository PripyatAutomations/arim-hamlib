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
#include "ini.h"
#include "util.h"
#include "bufq.h"
#include "datathread.h"

WINDOW *ui_ptable_win;
int show_ptable;
int ptable_row, ptable_col;
int cur_ptable_row;
int max_ptable_rows;

typedef struct pt {
    char call[16];
    char in_sn[8];
    char in_qual[8];
    char out_sn[8];
    char out_qual[8];
    time_t in_time;
    time_t out_time;
} PT_ENTRY;
PT_ENTRY ptable_list[MAX_PTABLE_LIST_LEN+1];
int ptable_list_cnt;
int ptable_start_line;
int refresh_ptable;

void ui_ptable_init()
{
    ptable_row = 0, ptable_col = 1;
    cur_ptable_row = ptable_row;
}

void ui_ptable_inc_start_line()
{
    ptable_start_line++;
    if (ptable_start_line >= ptable_list_cnt)
        ptable_start_line = ptable_list_cnt - 1;
}

void ui_ptable_dec_start_line()
{
    ptable_start_line--;
    if (ptable_start_line < 0)
        ptable_start_line = 0;
}

void ui_clear_ptable()
{
    pthread_mutex_lock(&mutex_ptable);
    memset(&ptable_list, 0, sizeof(ptable_list));
    pthread_mutex_unlock(&mutex_ptable);
    if (show_ptable && ui_ptable_win) {
        delwin(ui_ptable_win);
        ui_ptable_win = NULL;
        ui_ptable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ptable_win) {
            ui_print_status("Ping History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ptable_win, COLOR_PAIR(7));
        touchwin(ui_ptable_win);
        wrefresh(ui_ptable_win);
    }
    refresh_ptable = ptable_list_cnt = ptable_start_line = 0;
}

void ui_print_ptable_title()
{
    int center, start;

    center = (tnc_cmd_box_w / 2) - 1;
    start = center - 10;
    if (start < 1)
        start = 1;
    if (last_time_heard == LT_HEARD_CLOCK)
        mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, " PING HISTORY (LT) ");
    else
        mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, " PING HISTORY (ET) ");
    wrefresh(tnc_cmd_box);
}

void ui_print_ptable()
{
    static int once = 0;
    static time_t tprev = 0;
    struct tm *ping_time;
    char *p, ping_data[MAX_PTABLE_ROW_SIZE];
    char in_time[16], out_time[16];
    int max_cols, max_ptable_rows, cur_ptable_row = 0;
    int i, days, hours, minutes;
    time_t tcur, telapsed;

    if (!once) {
        once = 1;
        memset(&ptable_list, 0, sizeof(ptable_list));
    }

    if (show_ptable && !ui_ptable_win) {
        ui_ptable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ptable_win) {
            ui_print_status("Ping History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ptable_win, COLOR_PAIR(7));
        max_ptable_rows = tnc_cmd_box_h - 2;
        if (show_titles)
            ui_print_ptable_title();
        refresh_ptable = 1;
    }

    pthread_mutex_lock(&mutex_ptable);
    p = cmdq_pop(&g_ptable_q);
    pthread_mutex_unlock(&mutex_ptable);

    /*
      layout of record taken from queue:
         byte 0:     in/out indicator
         byte 1-12:  call sign
         byte 13-15: inbound s/n ratio
         byte 16-18: inbound quality
         byte 19-21: outbound s/n ratio
         byte 22-24: outbound quality
    */

    if (p) {
        memmove(&ptable_list[1], &ptable_list[0], MAX_PTABLE_LIST_LEN * sizeof(PT_ENTRY));
        memset(&ptable_list[0], 0, sizeof(PT_ENTRY));
        snprintf(ptable_list[0].call, sizeof(ptable_list[0].call), "%.11s", &p[1]);
        if (p[0] == 'R')
            ptable_list[0].in_time = time(NULL);
        else
            ptable_list[0].out_time = time(NULL);
        snprintf(ptable_list[0].in_sn, sizeof(ptable_list[0].in_sn), "%.3s", &p[13]);
        snprintf(ptable_list[0].in_qual, sizeof(ptable_list[0].in_qual), "%.3s", &p[16]);
        snprintf(ptable_list[0].out_sn, sizeof(ptable_list[0].out_sn), "%.3s", &p[19]);
        snprintf(ptable_list[0].out_qual, sizeof(ptable_list[0].out_qual), "%.3s", &p[22]);
        ++ptable_list_cnt;
        for (i = 1; i < ptable_list_cnt; i++) {
            if (!strncasecmp(ptable_list[0].call, ptable_list[i].call, TNC_MYCALL_SIZE)) {
                /* copy existing complementary data before overwriting the record */
                if (p[0] == 'R') {
                    ptable_list[0].out_time = ptable_list[i].out_time;
                    snprintf(ptable_list[0].out_sn, sizeof(ptable_list[0].out_sn),
                                                            "%s", ptable_list[i].out_sn);
                    snprintf(ptable_list[0].out_qual, sizeof(ptable_list[0].out_qual),
                                                            "%s", ptable_list[i].out_qual);
                } else {
                    ptable_list[0].in_time = ptable_list[i].in_time;
                    snprintf(ptable_list[0].in_sn, sizeof(ptable_list[0].in_sn),
                                                            "%s", ptable_list[i].in_sn);
                    snprintf(ptable_list[0].in_qual, sizeof(ptable_list[0].in_qual),
                                                            "%s", ptable_list[i].in_qual);
                }
                memmove(&ptable_list[i], &ptable_list[i + 1],
                            (MAX_PTABLE_LIST_LEN - i) * sizeof(PT_ENTRY));
                --ptable_list_cnt;
                break;
            }
        }
        if (ptable_list_cnt > MAX_PTABLE_LIST_LEN)
            --ptable_list_cnt;
        refresh_ptable = 1;
    }
    tcur = time(NULL);
    if ((tcur - tprev) > 15) {
        tprev = tcur;
        refresh_ptable = 1;
    }
    if (show_ptable && ui_ptable_win && refresh_ptable) {
        delwin(ui_ptable_win);
        ui_ptable_win = NULL;
        ui_ptable_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                     tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
        if (!ui_ptable_win) {
            ui_print_status("Ping History: failed to create window", 1);
            return;
        }
        if (color_code)
            wbkgd(ui_ptable_win, COLOR_PAIR(7));
        max_ptable_rows = tnc_cmd_box_h - 2;
        if (show_titles)
            ui_print_ptable_title();
        refresh_ptable = 0;
        max_cols = (tnc_cmd_box_w - 4) + 1;
        if (max_cols > sizeof(ping_data))
            max_cols = sizeof(ping_data);
        cur_ptable_row = 0;
        for (i = ptable_start_line; i < ptable_list_cnt &&
             i < (max_ptable_rows + ptable_start_line); i++) {
            if (ptable_list[i].in_time) {
                if (last_time_heard == LT_HEARD_CLOCK) {
                    pthread_mutex_lock(&mutex_time);
                    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
                        ping_time = gmtime(&ptable_list[i].in_time);
                    else
                        ping_time = localtime(&ptable_list[i].in_time);
                    pthread_mutex_unlock(&mutex_time);
                    snprintf(in_time, sizeof(in_time),
                        "%02d:%02d:%02d", ping_time->tm_hour,
                            ping_time->tm_min, ping_time->tm_sec);
                } else {
                    telapsed = tcur - ptable_list[i].in_time;
                    days = telapsed / (24*60*60);
                    if (days > 99)
                        days = 99;
                    telapsed = telapsed % (24*60*60);
                    hours = telapsed / (60*60);
                    telapsed = telapsed % (60*60);
                    minutes = telapsed / 60;
                    snprintf(in_time, sizeof(in_time),
                                "%02d:%02d:%02d", days, hours, minutes);
                }
            } else {
                snprintf(in_time, sizeof(in_time), "--:--:--");
            }
            if (ptable_list[i].out_time) {
                if (last_time_heard == LT_HEARD_CLOCK) {
                    pthread_mutex_lock(&mutex_time);
                    if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
                        ping_time = gmtime(&ptable_list[i].out_time);
                    else
                        ping_time = localtime(&ptable_list[i].out_time);
                    pthread_mutex_unlock(&mutex_time);
                    snprintf(out_time, sizeof(out_time),
                        "%02d:%02d:%02d", ping_time->tm_hour,
                            ping_time->tm_min, ping_time->tm_sec);
                } else {
                    telapsed = tcur - ptable_list[i].out_time;
                    days = telapsed / (24*60*60);
                    if (days > 99)
                        days = 99;
                    telapsed = telapsed % (24*60*60);
                    hours = telapsed / (60*60);
                    telapsed = telapsed % (60*60);
                    minutes = telapsed / 60;
                    snprintf(out_time, sizeof(out_time),
                                "%02d:%02d:%02d", days, hours, minutes);
                }
            } else {
                snprintf(out_time, sizeof(out_time), "--:--:--");
            }
            snprintf(ping_data, max_cols,
                "[%2d]%.11s [%s] >> S/N:%3sdB,Q:%3s  [%s] << S/N:%3sdB,Q:%3s",
                    i + 1, ptable_list[i].call, in_time, ptable_list[i].in_sn,
                        ptable_list[i].in_qual, out_time, ptable_list[i].out_sn,
                                ptable_list[i].out_qual);
            mvwprintw(ui_ptable_win, cur_ptable_row, ptable_col, "%s", ping_data);
            cur_ptable_row++;
        }
        touchwin(ui_ptable_win);
        wrefresh(ui_ptable_win);
    } else if ((!show_ptable && ui_ptable_win)) {
        delwin(ui_ptable_win);
        ui_ptable_win = NULL;
        if (show_titles)
            ui_print_cmd_win_title();
        touchwin(tnc_cmd_box);
        wrefresh(tnc_cmd_box);
        refresh_ptable = ptable_start_line = 0;
    }
}

void ui_refresh_ptable()
{
    if (show_titles)
        ui_print_ptable_title();
    refresh_ptable = 1;
    ui_print_ptable();
}

