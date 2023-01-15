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
#include "cmdproc.h"
#include "arim_arq.h"
#include "ui.h"
#include "ui_dialog.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"
#include "ui_tnc_cmd_win.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"

#define MAX_CMD_HIST            15+1

WINDOW *prompt_box;
WINDOW *prompt_win;
int prompt_box_y, prompt_box_x, prompt_box_w, prompt_box_h;
int prompt_row, prompt_col;

int ui_cmd_prompt_init(int y, int x, int width, int height)
{
    prompt_row = 0, prompt_col = 1;
    prompt_box_h = height;
    prompt_box_w = width;
    prompt_box_y = y;
    prompt_box_x = x;
    prompt_box = subwin(main_win, prompt_box_h, prompt_box_w, prompt_box_y, prompt_box_x);
    if (!prompt_box)
        return 0;
    if (color_code)
        wbkgd(prompt_box, COLOR_PAIR(7));
    box(prompt_box, 0, 0);
    prompt_win = derwin(prompt_box, 1, prompt_box_w - 2, 1, 1);
    if (!prompt_win)
        return 0;
    if (color_code)
        wbkgd(prompt_win, COLOR_PAIR(7));
    wtimeout(prompt_win, 100);
    return 1;
}

void ui_cmd_prompt_clear()
{
    wmove(prompt_win, prompt_row, prompt_col);
    wclrtoeol(prompt_win);
    wrefresh(prompt_win);
}

int ui_cmd_prompt()
{
    char cmd_line[MAX_CMD_SIZE+1];
    static char cmd_hist[MAX_CMD_HIST][MAX_CMD_SIZE+1];
    static int prev_cmd = 0, next_cmd = 0, cnt_hist = 0;
    size_t len = 0, cur = 0;
    int ch, temp, hist_cmd, max_len, quit = 0;

    max_len = tnc_cmd_box_w - 4;
    if (max_len > MAX_CMD_SIZE-1)
        max_len = MAX_CMD_SIZE-1;

    wmove(prompt_win, prompt_row, prompt_col);
    wclrtoeol(prompt_win);
    wrefresh(prompt_win);

    curs_set(1);
    keypad(prompt_win, TRUE);
    memset(cmd_line, 0, sizeof(cmd_line));
    hist_cmd = prev_cmd;
    while (!quit) {
        if ((status_timer && --status_timer == 0) ||
            (data_buf_scroll_timer && --data_buf_scroll_timer == 0)) {
            if (arim_is_arq_state())
                ui_print_status(ARQ_PROMPT_STR, 0);
            else
                ui_print_status(MENU_PROMPT_STR, 0);
        }
        ch = wgetch(prompt_win);
        switch (ch) {
        case ERR:
            curs_set(0);
            ui_print_cmd_in();
            ui_print_recents();
            ui_print_ptable();
            ui_print_ctable();
            ui_print_ftable();
            if (!data_buf_scroll_timer)
                ui_print_data_in();
            ui_print_heard_list();
            ui_check_status_dirty();
            wmove(prompt_win, prompt_row, prompt_col + cur);
            curs_set(1);
            break;
        case '\n':
            if (strlen(cmd_line) && strcmp(cmd_line, "!!") &&
                                    strcmp(cmd_hist[prev_cmd], cmd_line)) {
                snprintf(cmd_hist[next_cmd], sizeof(cmd_hist[next_cmd]), "%s", cmd_line);
                if (cnt_hist < MAX_CMD_HIST)
                    ++cnt_hist;
                prev_cmd = hist_cmd = next_cmd;
                ++next_cmd;
                if (next_cmd == MAX_CMD_HIST)
                    next_cmd = 0;
            }
            quit = 1;
            break;
        case 27:
            ui_on_cancel();
            break;
        case 127: /* DEL */
        case KEY_BACKSPACE:
            if (len && cur) {
                memmove(cmd_line + cur - 1, cmd_line + cur, MAX_CMD_SIZE - cur);
                --len;
                --cur;
                mvwdelch(prompt_win, prompt_row, prompt_col + cur);
            } else if (len == 0) {
                quit = 1;
            }
            break;
        case 4: /* CTRL-D */
        case KEY_DC:
            if (len && cur < len) {
                memmove(cmd_line + cur, cmd_line + cur + 1, MAX_CMD_SIZE - cur);
                mvwdelch(prompt_win, prompt_row, prompt_col + cur);
                --len;
            }
            break;
        case 11: /* CTRL-K */
            if (len && cur < len) {
                len -= (len - cur);
                cmd_line[cur] = '\0';
                wmove(prompt_win, prompt_row, prompt_col);
                wclrtoeol(prompt_win);
                waddstr(prompt_win, cmd_line);
            }
            break;
        case 21: /* CTRL-U */
            if (len && cur && cur <= len) {
                len -= cur;
                memmove(cmd_line, cmd_line + cur, MAX_CMD_SIZE - cur);
                cur = 0;
                wmove(prompt_win, prompt_row, prompt_col);
                wclrtoeol(prompt_win);
                waddstr(prompt_win, cmd_line);
            }
            break;
        case 1: /* CTRL-A */
        case KEY_HOME:
            if (cur) {
                cur = 0;
                wmove(prompt_win, prompt_row, prompt_col + cur);
            }
            break;
        case 5: /* CTRL-E */
        case KEY_END:
            if (cur < len) {
                cur = len;
                wmove(prompt_win, prompt_row, prompt_col + cur);
            }
            break;
        case 2: /* CTRL-B */
        case KEY_LEFT:
            if (cur) {
                --cur;
                wmove(prompt_win, prompt_row, prompt_col + cur);
            }
            break;
        case 6: /* CTRL-F */
        case KEY_RIGHT:
            if (cur < len) {
                ++cur;
                wmove(prompt_win, prompt_row, prompt_col + cur);
            }
            break;
        case 14: /* CTRL-N */
        case KEY_DOWN:
            if (hist_cmd != next_cmd) {
                temp = hist_cmd;
                ++hist_cmd;
                if (hist_cmd >= MAX_CMD_HIST)
                    hist_cmd = 0;
                if (hist_cmd != next_cmd) {
                    snprintf(cmd_line, sizeof(cmd_line), "%s", cmd_hist[hist_cmd]);
                } else {
                    cmd_line[0] = '\0';
                }
                if (hist_cmd == next_cmd)
                    hist_cmd = temp;
                cur = len = strlen(cmd_line);
                wmove(prompt_win, prompt_row, prompt_col);
                wclrtoeol(prompt_win);
                waddstr(prompt_win, cmd_line);
                wrefresh(prompt_win);
            }
            break;
        case 16: /* CTRL-P */
        case KEY_UP:
            if (hist_cmd != next_cmd) {
                temp = hist_cmd;
                snprintf(cmd_line, sizeof(cmd_line), "%s", cmd_hist[hist_cmd]);
                --hist_cmd;
                if (hist_cmd < 0) {
                    if (cnt_hist == MAX_CMD_HIST)
                        hist_cmd = MAX_CMD_HIST-1;
                    else
                        hist_cmd = 0;
                }
                if (hist_cmd == next_cmd)
                    hist_cmd = temp;
                cur = len = strlen(cmd_line);
                wmove(prompt_win, prompt_row, prompt_col);
                wclrtoeol(prompt_win);
                waddstr(prompt_win, cmd_line);
                wrefresh(prompt_win);
            }
            break;
        case 24: /* CTRL-X */
            if (arim_is_arq_state()) {
                temp = ui_show_dialog("\tAre you sure\n\tyou want to disconnect?\n \n\t[Y]es   [N]o", "yYnN");
                if (temp == 'y' || temp == 'Y')
                    arim_arq_send_disconn_req();
                quit = 1;
            }
            break;
        default:
            if (isprint(ch) && len < max_len) {
                if (cur == len) {
                    cmd_line[len++] = ch;
                    cmd_line[len] = '\0';
                    waddch(prompt_win, ch);
                } else {
                    memmove(cmd_line + cur + 1, cmd_line + cur, MAX_CMD_SIZE - cur);
                    cmd_line[cur] = ch;
                    ++len;
                    mvwinsch(prompt_win, prompt_row, prompt_col + cur, ch);
                }
                ++cur;
            }
        }
        if (g_win_changed)
            quit = 1;
    }
    keypad(prompt_win, FALSE);
    curs_set(0);
    wmove(prompt_win, prompt_row, prompt_col);
    wclrtoeol(prompt_win);
    wrefresh(prompt_win);
    if (ch == '\n')
        cmdproc_cmd(cmd_line); /* process the command */
    return 1;
}

