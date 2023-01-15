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
#include <ctype.h>
#include <string.h>
#include <curses.h>
#include "main.h"
#include "ui.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"
#include "ui_themes.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "ui_tnc_cmd_win.h"
#include "arim_proto.h"
#include "auth.h"

#define MAX_DIALOG_PROMPT_SIZE 512

WINDOW *dialog_win;

int ui_show_dialog(const char *prompt, char *wanted_keys)
{
    WINDOW *prev_win;
    char *p, linebuf[MAX_DIALOG_PROMPT_SIZE];
    size_t i, len;
    int cmd, quit = 0;
    int startx, starty, center, left, right, top, bot;
    int max_dialog_rows, max_w = 0, num_lines = 0;

    if (dialog_win)
        return 0;
    snprintf(linebuf, sizeof(linebuf), "%s", prompt);
    p = strtok(linebuf, "\n");
    while (p) {
        len = strlen(p);
        if (len > max_w)
            max_w = len;
        ++num_lines;
        p = strtok(NULL, "\n");
    }
    left = tnc_data_box_x + ((tnc_data_box_w - max_w) / 2) - 3;
    if (left <= tnc_data_box_x)
        left = tnc_data_box_x + 1;
    right = left + max_w + 5;
    if (right >= tnc_data_box_x + tnc_data_box_w)
        right = tnc_data_box_x + tnc_data_box_w - 1;
    top = tnc_data_box_y + ((tnc_data_box_h - num_lines) / 2) - 1;
    if (top <= tnc_data_box_y)
        top = tnc_data_box_y + 1;
    bot = top + num_lines + 2;
    if (bot > tnc_data_box_y + tnc_data_box_h)
        bot = tnc_data_box_y + tnc_data_box_h - 1;
    center = (right - left) / 2;
    max_dialog_rows = bot - top - 1 ;
    starty = (max_dialog_rows - num_lines) / 2;
    if (starty < 1)
        starty = 1;
    dialog_win = newwin(bot - top, right - left, top, left);
    if (!dialog_win) {
        ui_print_status("Dialog: failed to create dialog window", 1);
        return 0;
    }
    if (color_code) {
        wbkgd(dialog_win, COLOR_PAIR(11));
        wattron(dialog_win, themes[theme].ui_dlg_attr);
    } else {
        wattron(dialog_win, A_BOLD);
    }
    prev_win = ui_set_active_win(dialog_win);
    snprintf(linebuf, sizeof(linebuf), "%s", prompt);
    p = strtok(linebuf, "\n");
    for (i = 0; p && i < max_dialog_rows; i++) {
        /* center lines whose first character is a tab */
        if (*p == '\t') {
            ++p;
            len = strlen(p);
            startx = center - (len / 2);
            if (startx < 2)
                startx = 2;
        } else
            startx = 2;
        mvwprintw(dialog_win, i + starty, 1, " ");
        mvwprintw(dialog_win, i + starty, startx, p);
        p = strtok(NULL, "\n");
    }
    box(dialog_win, 0, 0);
    wrefresh(dialog_win);
    while (!quit) {
        cmd = getch();
        switch (cmd) {
        case 27:
            ui_on_cancel();
            break;
        default:
            p = wanted_keys;
            while (*p) {
                if (cmd == *p++) {
                    /* found in the list of wanted keys, return it */
                    delwin(dialog_win);
                    /* refresh underlying window */
                    if (prev_win) {
                        touchwin(prev_win);
                        wrefresh(prev_win);
                    }
                    ui_set_active_win(prev_win);
                    quit = 1;
                    break;
                }
            }
            if (!quit) {
                ui_print_cmd_in();
                ui_print_recents();
                ui_print_ptable();
                ui_print_ctable();
                ui_print_ftable();
                ui_print_heard_list();
                ui_check_status_dirty();
            }
            break;
        }
        if (g_win_changed)
            quit = 1;
        usleep(100000);
    }
    dialog_win = NULL;
    return cmd;
}

