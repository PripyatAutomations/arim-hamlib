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
#include <ctype.h>
#include "main.h"
#include "arim_proto.h"
#include "bufq.h"
#include "ini.h"
#include "ui.h"
#include "ui_themes.h"

#define MAX_DATA_BUF_LEN        512
#define MAX_DATA_ROW_SIZE       512
#define DATA_BUF_SCROLLING_TIME 300
#define DATA_WIN_SCROLL_LEGEND  "Scrolling: UP DOWN PAGEUP PAGEDOWN HOME END, 'e' to end. "

WINDOW *tnc_data_win;
WINDOW *tnc_data_box;
int tnc_data_box_y, tnc_data_box_x, tnc_data_box_w, tnc_data_box_h;
int data_row, cur_data_row, data_col;
int max_data_rows;

char data_buf[MAX_DATA_BUF_LEN+1][MAX_DATA_ROW_SIZE];
int data_buf_start, data_buf_end, data_buf_top, data_buf_cnt;
int data_buf_scroll_timer;

void ui_data_win_refresh()
{
    wrefresh(tnc_data_box);
}

void ui_print_data_win_title()
{
    char buffer[64];
    int center, start = 1;

    box(tnc_data_box, 0, 0);
    snprintf(buffer, sizeof(buffer), " TRAFFIC MONITOR ");
    center = (tnc_data_box_w / 2) - 1;
    start = center - 9;
    if (start < 1)
        start = 1;
    mvwprintw(tnc_data_box, tnc_data_box_h - 1, start, buffer);
}

int ui_data_win_init(int y, int x, int width, int height)
{
    data_row = 0, cur_data_row = 0, data_col = 0;
    tnc_data_box_y = y;
    tnc_data_box_x = x;
    tnc_data_box_w = width;
    tnc_data_box_h = height;
    tnc_data_box = newwin(tnc_data_box_h, tnc_data_box_w, tnc_data_box_y, tnc_data_box_x);
    if (!tnc_data_box)
        return 0;
    if (color_code)
        wbkgd(tnc_data_box, COLOR_PAIR(7));
    box(tnc_data_box, 0, 0);
    tnc_data_win = derwin(tnc_data_box, tnc_data_box_h - 2, tnc_data_box_w - 2, 1, 1);
    if (!tnc_data_win)
        return 0;
    if (color_code)
        wbkgd(tnc_data_win, COLOR_PAIR(7));
    max_data_rows = tnc_data_box_h - 2;
    scrollok(tnc_data_win, TRUE);
    if (show_titles)
        ui_print_data_win_title();
    return 1;
}

attr_t ui_calc_data_in_attr(char *linebuf)
{
    char *p, call[MAX_CALLSIGN_SIZE+4], mycall[TNC_MYCALL_SIZE];
    size_t i, cnt, len;
    int found_call, frame_type;
    attr_t attr = A_NORMAL;

    if (color_code) {
        if ((linebuf[0] == '<') || (mon_timestamp && linebuf[11] == '<'))
            attr = themes[theme].tm_tx_frame_attr;
        /* extract frame type */
        if (mon_timestamp)
            frame_type = linebuf[15];
        else
            frame_type = linebuf[4];
        /* check for net traffic first */
        cnt = arim_get_netcall_cnt();
        for (i = 0; i < cnt; i++) {
            if (!arim_copy_netcall(mycall, sizeof(mycall), i))
                continue;
            snprintf(call, sizeof(call), "|%s|", mycall);
            len = strlen(call);
            found_call = 0;
            p = linebuf;
            while (*p) {
                if (*p == '|') {
                    if (!strncasecmp(p, call, len)) {
                        found_call = 1;
                        break;
                    }
                }
                if ((p - linebuf) > MAX_ARIM_HDR_SIZE)
                    break;
                ++p;
            }
            if (found_call)
                break;
        }
        if (found_call) {
            switch (frame_type) {
            case 'E':
            case '!':
            case 'X':
                attr |= (COLOR_PAIR(1)|themes[theme].tm_err_attr);
                break;
            default:
                attr |= (COLOR_PAIR(6)|themes[theme].tm_net_attr);
                break;
            }
        } else {
            /* check for TNC call */
            arim_copy_mycall(mycall, sizeof(mycall));
            snprintf(call, sizeof(call), "|%s|", mycall);
            len = strlen(call);
            found_call = 0;
            p = linebuf;
            while (*p) {
                if (*p == '|') {
                    if (!strncasecmp(p, call, len)) {
                        found_call = 1;
                        break;
                    }
                }
                /* special check for ARDOP pings */
                if ((*p == '>') && (p - linebuf) < (MAX_CALLSIGN_SIZE*2) + 4) {
                    if (!strncasecmp(p + 1, mycall, strlen(mycall))) {
                        found_call = 1;
                        break;
                    }
                    if (!strncasecmp(p - strlen(mycall), mycall, strlen(mycall))) {
                        found_call = 1;
                        break;
                    }
                }
                if ((p - linebuf) > MAX_ARIM_HDR_SIZE)
                    break;
                ++p;
            }
            if (found_call) {
                switch (frame_type) {
                case 'M':
                case 'A':
                    attr |= (COLOR_PAIR(2)|themes[theme].tm_msg_attr);
                    break;
                case 'Q':
                case 'R':
                    attr |= (COLOR_PAIR(3)|themes[theme].tm_qry_attr);
                    break;
                case 'N':
                case 'E':
                case '!':
                case 'X':
                    attr |= (COLOR_PAIR(1)|themes[theme].tm_err_attr);
                    break;
                case 'B':
                    attr |= (COLOR_PAIR(5)|themes[theme].tm_bcn_attr);
                    break;
                case '@':
                    attr |= (COLOR_PAIR(9)|themes[theme].tm_arq_attr);
                    break;
                case 'P':
                case 'p':
                    attr |= (COLOR_PAIR(4)|themes[theme].tm_ping_attr);
                    break;
                default:
                    attr |= (COLOR_PAIR(7)|themes[theme].ui_def_attr);
                    break;
                }
            } else {
                switch (frame_type) {
                case 'I':
                    attr |= (COLOR_PAIR(8)|themes[theme].tm_id_attr);
                    break;
                case 'X':
                case 'E':
                    attr |= (COLOR_PAIR(1)|themes[theme].tm_err_attr);
                    break;
                case 'B':
                    attr |= (COLOR_PAIR(5)|themes[theme].tm_bcn_attr);
                    break;
                case '@':
                    attr |= (COLOR_PAIR(9)|themes[theme].tm_arq_attr);
                    break;
                default:
                    attr |= (COLOR_PAIR(7)|themes[theme].ui_def_attr);
                    break;
                }
            }
        }
    } else {
        if ((linebuf[0] == '<') || (mon_timestamp && linebuf[11] == '<'))
            attr = A_BOLD;
    }
    return attr;
}

void ui_refresh_data_win()
{
    int i, max_cols, cur;
    char linebuf[MAX_DATA_ROW_SIZE];

    cur = data_buf_top;
    max_cols = (tnc_data_box_w - 4) + 1;
    if (max_cols > sizeof(linebuf))
        max_cols = sizeof(linebuf);
    wclear(tnc_data_win);
    for (i = 0; i < max_data_rows; i++) {
        if (cur == data_buf_end)
            break;
        snprintf(linebuf, max_cols, "%s", data_buf[cur]);
        wattrset(tnc_data_win, ui_calc_data_in_attr(linebuf));
        mvwprintw(tnc_data_win, i, data_col, " %s", linebuf);
        if (color_code)
            wattrset(tnc_data_win, COLOR_PAIR(7)|A_NORMAL);
        else
            wattrset(tnc_data_win, A_NORMAL);
        if (++cur == MAX_DATA_BUF_LEN)
            cur = 0;
    }
    if (show_titles) {
        ui_print_data_win_title();
    }
    touchwin(tnc_data_box);
    wrefresh(tnc_data_box);
}

void ui_clear_data_in()
{
    pthread_mutex_lock(&mutex_data_in);
    memset(data_buf, 0, sizeof(data_buf));
    data_buf_start = data_buf_end = data_buf_top = data_buf_cnt = 0;
    data_buf_scroll_timer = 0;
    pthread_mutex_unlock(&mutex_data_in);
    wclear(tnc_data_win);
    touchwin(tnc_data_box);
    wrefresh(tnc_data_box);
}

void ui_print_data_in()
{
    char *p;

    pthread_mutex_lock(&mutex_data_in);
    p = dataq_pop(&g_data_in_q);
    while (p) {
        if (data_buf_cnt < MAX_DATA_BUF_LEN)
            ++data_buf_cnt;
        snprintf(data_buf[data_buf_end], sizeof(data_buf[0]), "%s", p);
        p = data_buf[data_buf_end];
        while (*p) {
            if (!isprint((int)*p)) /* replace unprintable chars with spaces */
                *p = ' ';
            ++p;
        }
        ++data_buf_end;
        if (data_buf_end == MAX_DATA_BUF_LEN)
            data_buf_end = 0;
        if (data_buf_cnt == MAX_DATA_BUF_LEN) {
            ++data_buf_start;
            if (data_buf_start == MAX_DATA_BUF_LEN)
                data_buf_start = 0;
        }
        p = dataq_pop(&g_data_in_q);
    }
    pthread_mutex_unlock(&mutex_data_in);
    if (data_buf_cnt > max_data_rows) {
        data_buf_top = data_buf_end - max_data_rows;
        if (data_buf_top < 0)
            data_buf_top += MAX_DATA_BUF_LEN;
    } else {
        data_buf_top = data_buf_end - data_buf_cnt;
    }
    ui_refresh_data_win();
}

void ui_data_win_on_key_home(void)
{
    if (!data_buf_scroll_timer)
        ui_print_status(DATA_WIN_SCROLL_LEGEND, 0);
    data_buf_scroll_timer = DATA_BUF_SCROLLING_TIME;
    data_buf_top = data_buf_start;
    ui_refresh_data_win();
}

void ui_data_win_on_key_end(void)
{
    if (!data_buf_scroll_timer)
        ui_print_status(DATA_WIN_SCROLL_LEGEND, 0);
    data_buf_scroll_timer = DATA_BUF_SCROLLING_TIME;
    if (data_buf_cnt < max_data_rows)
        return;
    data_buf_top = data_buf_end - max_data_rows;
    if (data_buf_top < 0)
        data_buf_top += MAX_DATA_BUF_LEN;
    ui_refresh_data_win();
}

void ui_data_win_on_key_pg_up(void)
{
    int temp;

    if (!data_buf_scroll_timer)
        ui_print_status(DATA_WIN_SCROLL_LEGEND, 0);
    data_buf_scroll_timer = DATA_BUF_SCROLLING_TIME;
    if (data_buf_top == data_buf_start)
        return;
    /* calculate distance from start */
    temp = data_buf_top - data_buf_start;
    if (temp < 0)
        temp += MAX_DATA_BUF_LEN;
    if (temp <= max_data_rows) {
        data_buf_top = data_buf_start;
    } else {
        data_buf_top -= max_data_rows;
        if (data_buf_top < 0)
            data_buf_top += MAX_DATA_BUF_LEN;
    }
    ui_refresh_data_win();
}

void ui_data_win_on_key_pg_dwn(void)
{
    int temp;

    if (!data_buf_scroll_timer)
        ui_print_status(DATA_WIN_SCROLL_LEGEND, 0);
    data_buf_scroll_timer = DATA_BUF_SCROLLING_TIME;
    if (data_buf_top == data_buf_end || data_buf_cnt < max_data_rows)
        return;
    /* calculate distance from end */
    temp = data_buf_end - data_buf_top;
    if (temp < 0)
        temp += MAX_DATA_BUF_LEN;
    if (temp <= max_data_rows)
        return;
    data_buf_top += max_data_rows;
    if (data_buf_top >= MAX_DATA_BUF_LEN)
        data_buf_top -= MAX_DATA_BUF_LEN;
    if (data_buf_top == data_buf_end)
        return;
    ui_refresh_data_win();
}

void ui_data_win_on_key_up(void)
{
    if (!data_buf_scroll_timer)
        ui_print_status(DATA_WIN_SCROLL_LEGEND, 0);
    data_buf_scroll_timer = DATA_BUF_SCROLLING_TIME;
    if (data_buf_top == data_buf_start)
        return;
    data_buf_top -= 1;
    if (data_buf_top < 0)
        data_buf_top += MAX_DATA_BUF_LEN;
    ui_refresh_data_win();
}

void ui_data_win_on_key_dwn(void)
{
    if (!data_buf_scroll_timer)
        ui_print_status(DATA_WIN_SCROLL_LEGEND, 0);
    data_buf_scroll_timer = DATA_BUF_SCROLLING_TIME;
    if (data_buf_top == data_buf_end)
        return;
    data_buf_top += 1;
    if (data_buf_top >= MAX_DATA_BUF_LEN)
        data_buf_top -= MAX_DATA_BUF_LEN;
    if (data_buf_top == data_buf_end)
        return;
    ui_refresh_data_win();
}

void ui_data_win_on_end_scroll()
{
    data_buf_scroll_timer = 1;
}

