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
#include <ctype.h>
#include "main.h"
#include "arim_message.h"
#include "arim_proto.h"
#include "arim_arq.h"
#include "arim_arq_msg.h"
#include "ui.h"
#include "ui_dialog.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "ui_tnc_cmd_win.h"
#include "ui_cmd_prompt_win.h"
#include "bufq.h"
#include "mbox.h"

#define MAX_CMD_HIST            10+1

int msg_view_restart;
static int zoption;

char *ui_set_header_flag(char *head, int flag)
{
    char *p;

    p = head;
    while (*p && *p != '\n')
        ++p;
    if (*p && *(p - 4) == ' ') {
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
    return head;
}

int ui_forward_msg(char *msgbuffer, size_t msgbufsize, const char *fn,
                        const char *hdr, const char *to_call)
{
    int state;

    state = arim_get_state();
    if (state != ST_IDLE && state != ST_ARQ_CONNECTED)
        return 0;
    /* get the message from the mbox file */
    mbox_fwd_msg(msgbuffer, msgbufsize, fn, hdr);
    if (state == ST_ARQ_CONNECTED) {
        arim_arq_msg_on_send_cmd(msgbuffer, zoption);
    } else {
        /* FEC mode; try to send the message */
        if (!arim_send_msg(msgbuffer, to_call))
            return 0;
    }
    return 1;
}

int ui_send_msg(char *msgbuffer, size_t msgbufsize, const char *fn, const char *hdr)
{
    char to_call[MAX_CALLSIGN_SIZE];
    int state;

    state = arim_get_state();
    if (state != ST_IDLE && state != ST_ARQ_CONNECTED)
        return 0;
    /* get the message from the mbox file */
    mbox_send_msg(msgbuffer, msgbufsize, to_call, sizeof(to_call), fn, hdr);
    if (state == ST_ARQ_CONNECTED) {
        arim_arq_msg_on_send_cmd(msgbuffer, zoption);
    } else {
        /* FEC mode */
        if (!arim_send_msg(msgbuffer, to_call))
            return 0;
    }
    return 1;
}

int ui_read_msg(const char *fn, const char *hdr, int msgnbr, int is_recent)
{
    WINDOW *read_pad;
    int cmd, top = 0, quit = 0;
    int max_read_rows, max_read_cols, min_read_rows, min_read_cols, num_read_rows;
    int center, start, max_pad_rows = 0;
    char msgbuffer[MAX_UNCOMP_DATA_SIZE], status[MAX_STATUS_BAR_SIZE];

    /* get the message from the mbox file */
    max_pad_rows = mbox_read_msg(msgbuffer, sizeof(msgbuffer), fn, hdr);

    min_read_rows = tnc_cmd_box_y + 1;
    max_read_rows = min_read_rows + tnc_cmd_box_h - 3;
    min_read_cols = tnc_cmd_box_x + 2;
    max_read_cols = min_read_cols + tnc_cmd_box_w - 4;
    num_read_rows = max_read_rows - min_read_rows;
    if (show_titles) {
        center = (tnc_cmd_box_w / 2) - 1;
        if (is_recent)
            snprintf(status, sizeof(status), " READ RECENT: %d ", msgnbr);
        else
            snprintf(status, sizeof(status), " READ MESSAGE: %d ", msgnbr);
        start = center - (strlen(status) / 2);
        if (start < 0)
            start = 0;
        mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, status);
        wrefresh(tnc_cmd_box);
    }
    read_pad = newpad(max_pad_rows + num_read_rows, max_read_cols);
    if (!read_pad)
        return 0;
    if (color_code)
        wbkgd(read_pad, COLOR_PAIR(7));
    waddstr(read_pad, msgbuffer);
    prefresh(read_pad, top, 0, min_read_rows, min_read_cols,
                 max_read_rows, max_read_cols);
    snprintf(status, sizeof(status),
            "Msg: [%3d] %d lines - use UP, DOWN keys to scroll, 'q' to quit",
                    msgnbr, max_pad_rows);
    status_timer = 1;
    while (!quit) {
        if (status_timer && --status_timer == 0)
            ui_print_status(status, 0);
        cmd = getch();
        switch (cmd) {
        case KEY_HOME:
            top = 0;
            prefresh(read_pad, top, 0, min_read_rows, min_read_cols,
                        max_read_rows, max_read_cols);
            break;
        case KEY_END:
            if (max_pad_rows < num_read_rows)
                break;
            top = max_pad_rows - num_read_rows;
            prefresh(read_pad, top, 0, min_read_rows, min_read_cols,
                        max_read_rows, max_read_cols);
            break;
        case ' ':
        case KEY_NPAGE:
            top += num_read_rows;
            if (top > max_pad_rows - 1)
                top = max_pad_rows - 1;
            prefresh(read_pad, top, 0, min_read_rows, min_read_cols,
                        max_read_rows, max_read_cols);
            break;
        case '-':
        case KEY_PPAGE:
            top -= num_read_rows;
            if (top < 0)
                top = 0;
            prefresh(read_pad, top, 0, min_read_rows, min_read_cols,
                        max_read_rows, max_read_cols);
            break;
        case KEY_UP:
            top -= 1;
            if (top < 0)
                top = 0;
            prefresh(read_pad, top, 0, min_read_rows, min_read_cols,
                        max_read_rows, max_read_cols);
            break;
        case '\n':
        case KEY_DOWN:
            top += 1;
            if (top > max_pad_rows - 1)
                top = max_pad_rows - 1;
            prefresh(read_pad, top, 0, min_read_rows, min_read_cols,
                        max_read_rows, max_read_cols);
            break;
        case 't':
        case 'T':
            if (last_time_heard == LT_HEARD_ELAPSED) {
                last_time_heard = LT_HEARD_CLOCK;
                ui_print_status("Showing clock time in Heard List, press 't' to toggle", 1);
            } else {
                last_time_heard = LT_HEARD_ELAPSED;
                ui_print_status("Showing elapsed time in Heard List, press 't' to toggle", 1);
            }
            ui_refresh_heard_list();
            break;
        case 'n':
        case 'N':
            ui_clear_new_ctrs();
            break;
        case 'q':
        case 'Q':
            delwin(read_pad);
            touchwin(tnc_cmd_box);
            wrefresh(tnc_cmd_box);
            status_timer = 1;
            quit = 1;
            break;
        case 27:
            ui_on_cancel();
            break;
        default:
            ui_print_heard_list();
            ui_check_status_dirty();
            break;
        }
        if (g_win_changed)
            quit = 1;
        usleep(100000);
    }
    if (show_titles)
        ui_print_cmd_win_title();
    return 1;
}

int ui_create_get_line(char *msg_line, size_t max_len)
{
    size_t len = 0, cur = 0;
    int ch, quit = 0;

    ui_cmd_prompt_clear();
    curs_set(1);
    keypad(prompt_win, TRUE);
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
            if (!data_buf_scroll_timer)
                ui_print_data_in();
            ui_print_heard_list();
            ui_check_status_dirty();
            wmove(prompt_win, prompt_row, prompt_col + cur);
            curs_set(1);
            break;
        case '\n':
            /* allow insertion of empty lines into message text */
            if (!len) {
                msg_line[len++] = '\n';
                msg_line[len] = '\0';
            }
            quit = 1;
            break;
        case 27:
            ui_on_cancel();
            break;
        case 127: /* DEL */
        case KEY_BACKSPACE:
            if (len && cur) {
                memmove(msg_line + cur - 1, msg_line + cur, max_len - cur);
                --len;
                --cur;
                mvwdelch(prompt_win, prompt_row, prompt_col + cur);
            }
            break;
        case 4: /* CTRL-D */
        case KEY_DC:
            if (len && cur < len) {
                memmove(msg_line + cur, msg_line + cur + 1, max_len - cur);
                mvwdelch(prompt_win, prompt_row, prompt_col + cur);
                --len;
            }
            break;
        case 11: /* CTRL-K */
            if (len && cur < len) {
                len -= (len - cur);
                msg_line[cur] = '\0';
                wmove(prompt_win, prompt_row, prompt_col);
                wclrtoeol(prompt_win);
                waddstr(prompt_win, msg_line);
            }
            break;
        case 21: /* CTRL-U */
            if (len && cur && cur <= len) {
                len -= cur;
                memmove(msg_line, msg_line + cur, max_len - cur);
                cur = 0;
                wmove(prompt_win, prompt_row, prompt_col);
                wclrtoeol(prompt_win);
                waddstr(prompt_win, msg_line);
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
        default:
            if (isprint(ch) && len < max_len) {
                if (cur == len) {
                    msg_line[len++] = ch;
                    msg_line[len] = '\0';
                    waddch(prompt_win, ch);
                } else {
                    memmove(msg_line + cur + 1, msg_line + cur, max_len - cur);
                    msg_line[cur] = ch;
                    ++len;
                    mvwinsch(prompt_win, prompt_row, prompt_col + cur, ch);
                }
                ++cur;
            }
        }
    }
    keypad(prompt_win, FALSE);
    curs_set(0);
    ui_cmd_prompt_clear();
    return (len != 0);
}

int ui_create_msg(char *buffer, size_t bufsize, const char *to)
{
    WINDOW *msg_win;
    char msg_line[MAX_CMD_SIZE+1];
    size_t len, msglen;
    int center, start;
    int max_msg_rows, max_msg_cols;
    int cur_msg_row = 0, cancel = 0, quit = 0;

    msg_win = newwin(tnc_cmd_box_h - 2, tnc_cmd_box_w - 2,
                                 tnc_cmd_box_y + 1, tnc_cmd_box_x + 1);
    if (!msg_win) {
        ui_print_status("New message: failed to create window", 1);
        return 0;
    }
    if (color_code)
        wbkgd(msg_win, COLOR_PAIR(7));
    show_cmds = 0;
    scrollok(msg_win, TRUE);
    if (show_titles) {
        center = (tnc_cmd_box_w / 2) - 1;
        snprintf(msg_line, sizeof(msg_line), " NEW MESSAGE to: %s ", to);
        start = center - (strlen(msg_line) / 2);
        if (start < 0)
            start = 0;
        mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, msg_line);
        wrefresh(tnc_cmd_box);
    }
    max_msg_rows = tnc_cmd_box_h - 2;
    max_msg_cols = tnc_cmd_box_w - 4;
    if (max_msg_cols > MAX_CMD_SIZE-1)
        max_msg_cols = MAX_CMD_SIZE-1;
    wrefresh(msg_win);
    buffer[0] = '\0';
    msglen = 0;
    ui_print_status("Type message lines at prompt, '/ex' to end, '/can' to cancel", 1);

    while (!quit) {
        memset(msg_line, 0, sizeof(msg_line));
        ui_create_get_line(msg_line, max_msg_cols);
        len = strlen(msg_line);
        if (len) {
            if (!strncasecmp(msg_line, "/ex", 3)) {
                quit = 1;
            }
            if (!strncasecmp(msg_line, "/can", 3)) {
                quit = cancel = 1;
                buffer[0] = '\0';
                ui_print_status("New message: canceled", 1);
            }
            /* print line to msg window */
            if (cur_msg_row == max_msg_rows) {
                wscrl(msg_win, 1);
                cur_msg_row--;
            }
            mvwprintw(msg_win, cur_msg_row, 0, " %s", msg_line);
            if (cur_msg_row < max_msg_rows)
                cur_msg_row++;
            wrefresh(msg_win);
            if (!quit) {
                /* add line to msg buffer */
                if (msglen + len < bufsize) {
                    strncat(buffer, msg_line, bufsize - msglen - 1);
                    msglen += len;
                    if (msg_line[0] != '\n') /* don't double up terminator on empty lines */
                        strncat(buffer, "\n", bufsize - msglen - 1);
                    ++msglen;
                } else {
                    ui_print_status("New message: buffer full! Type '/ex' to end, '/can' to cancel", 1);
                }
            }
        }
    }
    if (show_titles)
        ui_print_cmd_win_title();
    show_cmds = 1;
    return cancel ? 0 : 1;
}

void ui_print_msg_list_title(int mbox_type)
{
    int center, start;

    center = (tnc_data_box_w / 2) - 1;
    start = center - 12;
    if (start < 1)
        start = 1;
    switch (mbox_type) {
    case MBOX_TYPE_IN:
        mvwprintw(tnc_data_box, tnc_data_box_h - 1, start, " MESSAGE INBOX LISTING ");
        break;
    case MBOX_TYPE_OUT:
        mvwprintw(tnc_data_box, tnc_data_box_h - 1, start, " MESSAGE OUTBOX LISTING ");
        break;
    default:
        mvwprintw(tnc_data_box, tnc_data_box_h - 1, start, " SENT MESSAGES LISTING ");
        break;
    }
    wrefresh(tnc_data_box);
}

int ui_list_get_line(char *cmd_line, size_t max_len)
{
    static char cmd_hist[MAX_CMD_HIST][MAX_CMD_SIZE+1];
    static int prev_cmd = 0, next_cmd = 0, cnt_hist = 0;
    size_t len = 0, cur = 0;
    int ch, temp, hist_cmd, quit = 0;

    ui_cmd_prompt_clear();
    curs_set(1);
    keypad(prompt_win, TRUE);
    memset(cmd_line, 0, max_len);
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
            ui_print_heard_list();
            ui_check_status_dirty();
            wmove(prompt_win, prompt_row, prompt_col + cur);
            curs_set(1);
            break;
        case '\n':
            if (strlen(cmd_line) && strcmp(cmd_hist[prev_cmd], cmd_line)) {
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
                memmove(cmd_line + cur - 1, cmd_line + cur, max_len - cur);
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
                memmove(cmd_line + cur, cmd_line + cur + 1, max_len - cur);
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
                memmove(cmd_line, cmd_line + cur, max_len - cur);
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
            if (hist_cmd != next_cmd) {
                temp = hist_cmd;
                ++hist_cmd;
                if (hist_cmd >= MAX_CMD_HIST)
                    hist_cmd = 0;
                if (hist_cmd != next_cmd) {
                    snprintf(cmd_line, max_len, "%s", cmd_hist[hist_cmd]);
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
            if (hist_cmd != next_cmd) {
                temp = hist_cmd;
                snprintf(cmd_line, max_len, "%s", cmd_hist[hist_cmd]);
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
        default:
            if (isprint(ch) && len < max_len) {
                if (cur == len) {
                    cmd_line[len++] = ch;
                    cmd_line[len] = '\0';
                    waddch(prompt_win, ch);
                } else {
                    memmove(cmd_line + cur + 1, cmd_line + cur, max_len - cur);
                    cmd_line[cur] = ch;
                    ++len;
                    mvwinsch(prompt_win, prompt_row, prompt_col + cur, ch);
                }
                ++cur;
            }
        }
    }
    keypad(prompt_win, FALSE);
    curs_set(0);
    ui_cmd_prompt_clear();
    return (len != 0);
}

void ui_list_msg(const char *fn, int mbox_type)
{
    WINDOW *mbox_win;
    FILE *mboxfp;
    char *p, linebuf[MAX_MBOX_HDR_SIZE+1], msgbuffer[MAX_UNCOMP_DATA_SIZE];
    static char list[MAX_MBOX_LIST_LEN+1][MAX_MBOX_HDR_SIZE];
    char to_call[MAX_CALLSIGN_SIZE];
    char fpath[MAX_PATH_SIZE];
    static int once = 0;
    int i, temp, max_cols, max_mbox_rows, cmd, cur, top, start, quit = 0;

    if (!once) {
        once = 1;
        memset(&list, 0, sizeof(list));
    }
    mbox_win = newwin(tnc_data_box_h - 2, tnc_data_box_w - 2,
                                 tnc_data_box_y + 1, tnc_data_box_x + 1);
    max_mbox_rows = tnc_data_box_h - 2;
    if (!mbox_win) {
        ui_print_status("List: failed to create list window", 1);
        return;
    }
    if (color_code)
        wbkgd(mbox_win, COLOR_PAIR(7));
    ui_set_active_win(mbox_win);
    max_mbox_rows = tnc_data_box_h - 2;
    mbox_purge(fn, atoi(g_arim_settings.max_msg_days));

restart:
    msg_view_restart = 0;
    wclear(mbox_win);
    start = 0;
    snprintf(fpath, sizeof(fpath), "%s/%s", mbox_dir_path, fn);
    mboxfp = fopen(fpath, "r");
    if (!mboxfp) {
        ui_print_status("List: failed to open mailbox file", 1);
        ui_set_active_win(tnc_data_box);
        return;
    }
    flockfile(mboxfp);
    p = fgets(linebuf, sizeof(linebuf), mboxfp);
    while (p) {
        while (p && strncmp(p, "From ", 5))
            p = fgets(linebuf, sizeof(linebuf), mboxfp);
        if (p) {
            snprintf(list[start], sizeof(list[0]), "%s", p);
            p = list[start];
            while (*p && *p != '\n')
                ++p;
            *p = '\0';
            ++start;
            if (start == MAX_MBOX_LIST_LEN) {
                cmd = ui_show_dialog("\tToo many messages to list;\n"
                                     "\tnewer messages can't be shown.\n"
                                     "\tKill older messages to\n"
                                     "\tmake room for new.\n \n\t[O]k", "oO \n");
                break;
            }
            p = fgets(linebuf, sizeof(linebuf), mboxfp);
        }
    }
    funlockfile(mboxfp);
    fclose(mboxfp);
    --start;
    max_cols = (tnc_data_box_w - 4) + 1;
    if (max_cols > sizeof(linebuf))
        max_cols = sizeof(linebuf);
    cur = top = start;
    for (i = 0; i < max_mbox_rows && cur >= 0; i++) {
        snprintf(linebuf, max_cols, "[%3d] %s", cur + 1, list[cur]);
        mvwprintw(mbox_win, i, 1, linebuf);
        --cur;
    }
    if (show_titles)
        ui_print_msg_list_title(mbox_type);
    wrefresh(mbox_win);
    status_timer = 1;
    while (!quit) {
        if (status_timer && --status_timer == 0) {
            if (mbox_type == MBOX_TYPE_OUT) {
                if (arim_is_arq_state())
                    ui_print_status("<SP> for prompt: 'rm n' read, 'km n' kill, 'sm [-z] n' send, "
                                    "'cf n fl' clr flag, 'pm d' purge old, 'q' quit", 0);
                else
                    ui_print_status("<SP> for prompt: 'rm n' read, 'km n' kill, 'sm n' send, "
                                    "'cf n fl' clr flag, 'pm d' purge old, 'q' quit", 0);
            } else {
                if (arim_is_arq_state())
                    ui_print_status("<SP> for prompt: 'rm n' read, 'sv n fn' save, 'km n' kill, "
                                    "'fm [-z] n' fwd, 'cf n fl' clr flag, 'pm d' purge old, 'q' quit", 0);
                else
                    ui_print_status("<SP> for prompt: 'rm n' read, 'sv n fn' save, 'km n' kill, "
                                    "'fm n call' fwd, 'cf n fl' clr flag, 'pm d' purge old, 'q' quit", 0);
            }
        }
        cmd = getch();
        switch (cmd) {
        case ' ':
            memset(linebuf, 0, sizeof(linebuf));
            ui_list_get_line(linebuf, max_cols - 1);
            /* process the command */
            if (linebuf[0] == ':') {
                if (g_tnc_attached)
                    bufq_queue_data_out(&linebuf[1]);
                break;
            } else if (linebuf[0] == '!') {
                if (g_tnc_attached)
                    bufq_queue_cmd_out(&linebuf[1]);
                break;
            } else if (linebuf[0] == 'q') {
                quit = 1;
            } else {
                p = strtok(linebuf, " \t");
                if (!p)
                    break;
                if (!strncasecmp(p, "rm", 2)) {
                    p = strtok(NULL, " \t");
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Read message: invalid message number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= start) {
                        if (ui_read_msg(fn, list[i], i + 1, 0)) {
                            ui_set_recent_flag(list[i], 'R');
                        } else {
                            ui_print_status("Read message: cannot read message", 1);
                        }
                    } else {
                        ui_print_status("Read message: invalid message number", 1);
                    }
                    if (show_recents)
                        ui_refresh_recents();
                    goto restart;
                } else if (!strncasecmp(p, "rr", 2) && show_recents) {
                    p = strtok(NULL, " \t");
                    if (!p || (i = atoi(p)) < 1) {
                        ui_print_status("Read recent: invalid message number", 1);
                        break;
                    }
                    if (ui_get_recent(i - 1, linebuf, sizeof(linebuf))) {
                        ui_read_msg(MBOX_INBOX_FNAME, linebuf, i, 1);
                        ui_set_recent_flag(linebuf, 'R');
                        ui_refresh_recents();
                    } else {
                        ui_print_status("Read recent: cannot read message", 1);
                    }
                    goto restart;
                } else if (!strncasecmp(p, "pm", 2)) {
                    p = strtok(NULL, " \t");
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Purge messages: invalid older-than age", 1);
                        break;
                    }
                    if (i >= 1) {
                        snprintf(msgbuffer, sizeof(msgbuffer),
                            "\tAre you sure\n\tyou want to purge all messages\n\tolder than %d days?\n \n\t[Y]es   [N]o", i);
                        temp = ui_show_dialog(msgbuffer, "yYnN");
                        if (temp == 'y' || temp == 'Y') {
                            if (mbox_purge(fn, i))
                                goto restart;
                            else
                                ui_print_status("Purge messages: cannot kill messages", 1);
                        }
                    }
                } else if (!strncasecmp(p, "km", 2)) {
                    p = strtok(NULL, " \t");
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Kill message: invalid message number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= start) {
                        if (mbox_delete_msg(fn, list[i]))
                            goto restart;
                        else
                            ui_print_status("Kill message: cannot kill message", 1);
                    } else {
                        ui_print_status("Kill message: invalid message number", 1);
                    }
                } else if (!strncasecmp(p, "sv", 2)) {
                    p = strtok(NULL, " \t");
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Save message: invalid message number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= start) {
                        p = strtok(NULL, " \t");
                        if (!p)
                            break;
                        if (mbox_save_msg(fn, list[i], p)) {
                            ui_set_recent_flag(list[i], 'S');
                            ui_print_status("Save message: message saved to file", 1);
                            goto restart;
                        } else {
                            ui_print_status("Save message: cannot save message", 1);
                        }
                    } else {
                        ui_print_status("Save message: invalid message number", 1);
                    }
                } else if (mbox_type == MBOX_TYPE_OUT && !strncasecmp(p, "sm", 2)) {
                    if (!g_tnc_attached) {
                        ui_print_status("Send message: cannot send, no TNC attached", 1);
                        break;
                    }
                    zoption = 0;
                    p = strtok(NULL, " \t");
                    if (p && !strcmp(p, "-z")) {
                        if (!arim_is_arq_state()) {
                            ui_print_status("Send message: -z option not supported in FEC mode", 1);
                            break;
                        }
                        p = strtok(NULL, " \t");
                        zoption = 1;
                    }
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Send message: invalid message number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= start) {
                        if (ui_send_msg(msgbuffer, sizeof(msgbuffer), fn, list[i])) {
                            wclear(mbox_win);
                            ui_print_status("ARIM Busy: sending message", 1);
                            goto restart;
                        } else {
                            ui_print_status("Send message: cannot send, TNC busy", 1);
                        }
                    } else {
                        ui_print_status("Send message: invalid message number", 1);
                    }
                } else if (mbox_type != MBOX_TYPE_OUT && !strncasecmp(p, "fm", 2)) {
                    if (!g_tnc_attached) {
                        ui_print_status("Fwd message: cannot forward, no TNC attached", 1);
                        break;
                    }
                    zoption = 0;
                    p = strtok(NULL, " \t");
                    if (p && !strcmp(p, "-z")) {
                        if (!arim_is_arq_state()) {
                            ui_print_status("Fwd message: -z option not supported in FEC mode", 1);
                            break;
                        }
                        p = strtok(NULL, " \t");
                        zoption = 1;
                    }
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Fwd message: invalid message number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= start) {
                        p = strtok(NULL, " \t");
                        if (arim_is_arq_state()) {
                            arim_copy_remote_call(to_call, sizeof(to_call));
                        } else {
                            if (!p || (!ini_validate_mycall(p) && !ini_validate_netcall(p))) {
                                ui_print_status("Fwd message: invalid call sign", 1);
                                break;
                            }
                            snprintf(to_call, sizeof(to_call), "%s", p);
                        }
                        if (ui_forward_msg(msgbuffer, sizeof(msgbuffer), fn, list[i], to_call)) {
                            wclear(mbox_win);
                            ui_set_recent_flag(list[i], 'F');
                            ui_print_status("ARIM Busy: forwarding message", 1);
                            goto restart;
                        } else {
                            ui_print_status("Fwd message: cannot forward, TNC busy", 1);
                        }
                    } else {
                        ui_print_status("Fwd message: invalid message number", 1);
                    }
                } else if (!strncasecmp(p, "cf", 2)) {
                    p = strtok(NULL, " \t");
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Clear flag: invalid msg number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= start) {
                        p = strtok(NULL, " \t");
                        if (!p) {
                            ui_print_status("Clear flag: cannot clear, no flag given", 1);
                            break;
                        }
                        if (mbox_clear_flag(fn, list[i], *p)) {
                            wclear(mbox_win);
                            goto restart;
                        }
                    } else {
                        ui_print_status("Clear flag: invalid msg number", 1);
                    }
                } else {
                    ui_print_status("Message listing: command not available in this view", 1);
                }
            }
            break;
        case 't':
        case 'T':
            if (last_time_heard == LT_HEARD_ELAPSED) {
                last_time_heard = LT_HEARD_CLOCK;
                ui_print_status("Showing clock time in Heard List, press 't' to toggle", 1);
            } else {
                last_time_heard = LT_HEARD_ELAPSED;
                ui_print_status("Showing elapsed time in Heard List, press 't' to toggle", 1);
            }
            ui_refresh_heard_list();
            if (show_ptable)
                ui_refresh_ptable();
            break;
        case 'r':
        case 'R':
            if (show_ptable || show_ctable || show_ftable)
                break;
            if (!show_recents) {
                show_recents = 1;
                ui_print_status("Showing Recents, press 'r' to toggle", 1);
            } else {
                show_recents = 0;
                ui_print_status("Showing TNC cmds, press 'r' to toggle", 1);
            }
            break;
        case 'p':
        case 'P':
            if (show_recents || show_ctable || show_ftable)
                break;
            if (!show_ptable) {
                show_ptable = 1;
                ui_print_status("Showing Pings, <SP> 'u' or 'd' to scroll, 'p' to toggle", 1);
            } else {
                show_ptable = 0;
                ui_print_status("Showing TNC cmds, press 'p' to toggle", 1);
            }
            break;
        case 'c':
        case 'C':
            if (show_recents || show_ptable || show_ftable)
                break;
            if (!show_ctable) {
                show_ctable = 1;
                ui_print_status("Showing Connections, <SP> 'u' or 'd' to scroll, 'c' to toggle", 1);
            } else {
                show_ctable = 0;
                ui_print_status("Showing TNC cmds, press 'c' to toggle", 1);
            }
            break;
        case 'l':
        case 'L':
            if (show_recents || show_ptable || show_ctable)
                break;
            if (!show_ftable) {
                show_ftable = 1;
                ui_print_status("Showing ARQ File History, <SP> 'u' or 'd' to scroll, 'l' to toggle", 1);
            } else {
                show_ftable = 0;
                ui_print_status("Showing TNC cmds, press 'l' to toggle", 1);
            }
            break;
        case 'd':
            if (show_ptable && ptable_list_cnt) {
                ui_ptable_inc_start_line();
                ui_refresh_ptable();
            }
            else if (show_ctable && ctable_list_cnt) {
                ui_ctable_inc_start_line();
                ui_refresh_ctable();
            }
            else if (show_ftable && ftable_list_cnt) {
                ui_ftable_inc_start_line();
                ui_refresh_ftable();
            }
            else if (show_recents && recents_list_cnt) {
                ui_recents_inc_start_line();
                ui_refresh_recents();
            }
            break;
        case 'u':
            if (show_ptable && ptable_list_cnt) {
                ui_ptable_dec_start_line();
                ui_refresh_ptable();
            }
            else if (show_ctable && ctable_list_cnt) {
                ui_ctable_dec_start_line();
                ui_refresh_ctable();
            }
            else if (show_ftable && ftable_list_cnt) {
                ui_ftable_dec_start_line();
                ui_refresh_ftable();
            }
            else if (show_recents && recents_list_cnt) {
                ui_recents_dec_start_line();
                ui_refresh_recents();
            }
            break;
        case KEY_HOME:
            top = start;
            cur = top;
            wclear(mbox_win);
            for (i = 0; i < max_mbox_rows && cur >= 0; i++) {
                snprintf(linebuf, max_cols, "[%3d] %s", cur + 1, list[cur]);
                mvwprintw(mbox_win, i, 1, linebuf);
                --cur;
            }
            wrefresh(mbox_win);
            break;
        case KEY_END:
            if (start < max_mbox_rows - 1)
                break;
            top = max_mbox_rows - 1;
            cur = top;
            wclear(mbox_win);
            for (i = 0; i < max_mbox_rows && cur >= 0; i++) {
                snprintf(linebuf, max_cols, "[%3d] %s", cur + 1, list[cur]);
                mvwprintw(mbox_win, i, 1, linebuf);
                --cur;
            }
            wrefresh(mbox_win);
            break;
        case KEY_NPAGE:
            top -= max_mbox_rows;
            if (top < 0)
                top = 0;
            cur = top;
            wclear(mbox_win);
            for (i = 0; i < max_mbox_rows && cur >= 0; i++) {
                snprintf(linebuf, max_cols, "[%3d] %s", cur + 1, list[cur]);
                mvwprintw(mbox_win, i, 1, linebuf);
                --cur;
            }
            wrefresh(mbox_win);
            break;
        case '-':
        case KEY_PPAGE:
            top += max_mbox_rows;
            if (top > start)
                top = start;
            cur = top;
            wclear(mbox_win);
            for (i = 0; i < max_mbox_rows && cur >= 0; i++) {
                snprintf(linebuf, max_cols, "[%3d] %s", cur + 1, list[cur]);
                mvwprintw(mbox_win, i, 1, linebuf);
                --cur;
            }
            wrefresh(mbox_win);
            break;
        case KEY_UP:
            top += 1;
            if (top > start)
                top = start;
            cur = top;
            wclear(mbox_win);
            for (i = 0; i < max_mbox_rows && cur >= 0; i++) {
                snprintf(linebuf, max_cols, "[%3d] %s", cur + 1, list[cur]);
                mvwprintw(mbox_win, i, 1, linebuf);
                --cur;
            }
            wrefresh(mbox_win);
            break;
        case '\n':
        case KEY_DOWN:
            top -= 1;
            if (top < 0)
                top = 0;
            cur = top;
            wclear(mbox_win);
            for (i = 0; i < max_mbox_rows && cur >= 0; i++) {
                snprintf(linebuf, max_cols, "[%3d] %s", cur + 1, list[cur]);
                mvwprintw(mbox_win, i, 1, linebuf);
                --cur;
            }
            wrefresh(mbox_win);
            break;
        case 'n':
        case 'N':
            ui_clear_new_ctrs();
            break;
        case 'q':
        case 'Q':
            quit = 1;
            break;
        case 24: /* CTRL-X */
            if (arim_is_arq_state()) {
                cmd = ui_show_dialog("\tAre you sure\n\tyou want to disconnect?\n \n\t[Y]es   [N]o", "yYnN");
                if (cmd == 'y' || cmd == 'Y')
                    arim_arq_send_disconn_req();
            }
            break;
        case 27:
            ui_on_cancel();
            break;
        default:
            ui_print_cmd_in();
            ui_print_recents();
            ui_print_ptable();
            ui_print_ctable();
            ui_print_ftable();
            ui_print_heard_list();
            ui_check_status_dirty();
            break;
        }
        if (msg_view_restart)
            goto restart;
        if (g_win_changed)
            quit = 1;
        usleep(100000);
    }
    delwin(mbox_win);
    ui_set_active_win(tnc_data_box);
    touchwin(tnc_data_box);
    wrefresh(tnc_data_box);
    if (show_titles)
        ui_print_data_win_title();
    status_timer = 1;
}

