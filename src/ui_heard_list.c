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
#include <curses.h>
#include <time.h>
#include <ctype.h>
#include "main.h"
#include "bufq.h"
#include "ini.h"
#include "ui.h"
#include "ui_themes.h"
#include "util.h"

WINDOW *ui_list_box;
WINDOW *ui_list_win;
int ui_list_box_y, ui_list_box_x, ui_list_box_w, ui_list_box_h;
int list_row, cur_list_row, list_col, max_list_rows;
int last_time_heard, prev_last_time_heard = -1;

typedef struct hl {
    char htext[MAX_HEARD_SIZE];
    time_t htime;
} HL_ENTRY;
HL_ENTRY heard_list[MAX_HEARD_LIST_LEN+1];
int heard_list_cnt;

void ui_print_heard_list_title()
{
    box(ui_list_box, 0, 0);
    if (last_time_heard == LT_HEARD_ELAPSED)
        mvwprintw(ui_list_box, 0, 4, " CALLS HEARD (ET) ");
    else
        mvwprintw(ui_list_box, 0, 4, " CALLS HEARD (LT) ");
}

int ui_heard_list_init(int y, int x, int width, int height)
{
    ui_list_box_y = y;
    ui_list_box_x = x;
    ui_list_box_w = width;
    ui_list_box_h = height;
    ui_list_box = newwin(ui_list_box_h, ui_list_box_w, ui_list_box_y, ui_list_box_x);
    if (!ui_list_box)
        return 0;
    if (color_code)
        wbkgd(ui_list_box, COLOR_PAIR(7));
    box(ui_list_box, 0, 0);
    ui_list_win = derwin(ui_list_box, ui_list_box_h - 2, ui_list_box_w - 2, 1, 1);
    if (!ui_list_win)
        return 0;
    if (color_code)
        wbkgd(ui_list_win, COLOR_PAIR(7));
    max_list_rows = ui_list_box_h - 2;
    if (show_titles)
        ui_print_heard_list_title();
    return 1;
}

int ui_heard_list_get_width()
{
    return ui_list_box_w;
}

void ui_clear_calls_heard()
{
    memset(heard_list, 0, sizeof(heard_list));
    heard_list_cnt = 0;
    wclear(ui_list_win);
    touchwin(ui_list_box);
    wrefresh(ui_list_box);
}

void ui_get_heard_list(char *listbuf, size_t listbufsize)
{
    char linebuf[MAX_HEARD_SIZE];
    size_t i, len, cnt = 0;
    int numch;

    snprintf(listbuf, listbufsize, "Calls heard (%s):\n",
                last_time_heard == LT_HEARD_ELAPSED ? "ET" : "LT");
    cnt += strlen(listbuf);
    for (i = 0; i < heard_list_cnt; i++) {
        numch = snprintf(linebuf, sizeof(linebuf), "  %s\n", &(heard_list[i].htext[1]));
        len = strlen(linebuf);
        if ((cnt + len) < listbufsize) {
            strncat(listbuf, linebuf, listbufsize - cnt - 1);
            cnt += len;
        } else {
            break;
        }
    }
    snprintf(linebuf, sizeof(linebuf), "End\n");
    len = strlen(linebuf);
    if ((cnt + len) < listbufsize) {
        strncat(listbuf, linebuf, listbufsize - cnt - 1);
        cnt += len;
    }
    listbuf[cnt] = '\0';
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

void ui_update_heard_list()
{
    static time_t tprev;
    time_t tcur, telapsed;
    struct tm *heard_time;
    int i, days, hours, minutes, numch, reformat = 0;
    char heard[MAX_HEARD_SIZE];

    if (last_time_heard != prev_last_time_heard)
        reformat = 1;
    if (last_time_heard == LT_HEARD_ELAPSED) {
        tcur = time(NULL);
        if (reformat || (tcur - tprev) > 15) {
            tprev = tcur;
            for (i = 0; i < heard_list_cnt; i++) {
                telapsed = tcur - heard_list[i].htime;
                days = telapsed / (24*60*60);
                if (days > 99)
                    continue;
                telapsed = telapsed % (24*60*60);
                hours = telapsed / (60*60);
                telapsed = telapsed % (60*60);
                minutes = telapsed / 60;
                snprintf(heard, sizeof(heard), "%.16s", heard_list[i].htext);
                numch = snprintf(heard_list[i].htext, sizeof(heard_list[0].htext),
                                 "%s %02d:%02d:%02d", heard, days, hours, minutes);
            }
            if (reformat && show_titles)
                ui_print_heard_list_title();
        }
    } else if (reformat) {
        for (i = 0; i < heard_list_cnt; i++) {
            pthread_mutex_lock(&mutex_time);
            if (!strncasecmp(g_ui_settings.utc_time, "TRUE", 4))
                heard_time = gmtime(&heard_list[i].htime);
            else
                heard_time = localtime(&heard_list[i].htime);
            pthread_mutex_unlock(&mutex_time);
            snprintf(heard, sizeof(heard), "%.16s", heard_list[i].htext);
            numch = snprintf(heard_list[i].htext, sizeof(heard_list[0].htext),
                             "%s %02d:%02d:%02d", heard, heard_time->tm_hour,
                                 heard_time->tm_min, heard_time->tm_sec);
        }
        if (show_titles)
            ui_print_heard_list_title();
    }
    prev_last_time_heard = last_time_heard;
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

void ui_refresh_heard_list()
{
    int i;

    ui_update_heard_list();
    cur_list_row = 0;
    wclear(ui_list_win);
    for (i = 0; i < heard_list_cnt && i < max_list_rows; i++) {
        if (color_code) {
            switch (heard_list[i].htext[0]) {
            case '1':
                wattrset(ui_list_win, COLOR_PAIR(1)|themes[theme].tm_err_attr);
                break;
            case '2':
                wattrset(ui_list_win, COLOR_PAIR(2)|themes[theme].tm_msg_attr);
                break;
            case '3':
                wattrset(ui_list_win, COLOR_PAIR(3)|themes[theme].tm_qry_attr);
                break;
            case '4':
                wattrset(ui_list_win, COLOR_PAIR(4)|themes[theme].tm_ping_attr);
                break;
            case '5':
                wattrset(ui_list_win, COLOR_PAIR(5)|themes[theme].tm_bcn_attr);
                break;
            case '6':
                wattrset(ui_list_win, COLOR_PAIR(6)|themes[theme].tm_net_attr);
                break;
            case '7':
                wattrset(ui_list_win, COLOR_PAIR(7)|themes[theme].ui_def_attr);
                break;
            case '8':
                wattrset(ui_list_win, COLOR_PAIR(8)|themes[theme].tm_id_attr);
                break;
            case '9':
                wattrset(ui_list_win, COLOR_PAIR(9)|themes[theme].tm_arq_attr);
                break;
            }
        }
        mvwprintw(ui_list_win, cur_list_row, list_col, "%s", &(heard_list[i].htext[1]));
        if (color_code)
            wattrset(ui_list_win, COLOR_PAIR(7)|A_NORMAL);
        else
            wattrset(ui_list_win, A_NORMAL);
        cur_list_row++;
    }
    touchwin(ui_list_box);
    wrefresh(ui_list_box);
}

void ui_print_heard_list()
{
    static int once = 0;
    char *p;
    int i;

    if (!once) {
        once = 1;
        memset(&heard_list, 0, sizeof(heard_list));
    }

    pthread_mutex_lock(&mutex_heard);
    p = cmdq_pop(&g_heard_q);
    pthread_mutex_unlock(&mutex_heard);

    if (p) {
        memmove(&heard_list[1], &heard_list[0], MAX_HEARD_LIST_LEN * sizeof(HL_ENTRY));
        heard_list[0].htime = time(NULL);
        snprintf(heard_list[0].htext, sizeof(heard_list[0].htext), "%s", p);
        ++heard_list_cnt;
        for (i = 1; i < heard_list_cnt; i++) {
            if (!strncasecmp(&(heard_list[0].htext[5]), &(heard_list[i].htext[5]), 10)) {
                memmove(&heard_list[i], &heard_list[i + 1], (MAX_HEARD_LIST_LEN - i) * sizeof(HL_ENTRY));
                --heard_list_cnt;
                break;
            }
        }
        if (heard_list_cnt > MAX_HEARD_LIST_LEN)
            --heard_list_cnt;
        /* force reformatting of heard list */
        prev_last_time_heard = -1;
        ui_refresh_heard_list();
    } else if (last_time_heard == LT_HEARD_ELAPSED) {
        /* periodic check, results in update every 15 seconds */
        ui_refresh_heard_list();
    } else {
        touchwin(ui_list_box);
        wrefresh(ui_list_box);
    }
}

