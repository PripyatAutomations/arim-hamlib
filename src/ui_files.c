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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curses.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <stdint.h>
#include "main.h"
#include "arim_message.h"
#include "arim_proto.h"
#include "arim_arq.h"
#include "arim_arq_files.h"
#include "ini.h"
#include "ui.h"
#include "ui_dialog.h"
#include "ui_themes.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "ui_tnc_cmd_win.h"
#include "ui_cmd_prompt_win.h"
#include "log.h"
#include "util.h"
#include "auth.h"
#include "bufq.h"
#include "cmdproc.h"

#define MAX_CMD_HIST            10+1

int ui_send_file(char *msgbuffer, size_t msgbufsize,
                    const char *fn, const char *to_call)
{
    FILE *fp;
    size_t len, max;
    struct stat stats;

    if (stat(fn, &stats) == 0) {
        if (!S_ISDIR(stats.st_mode)) {
            max = atoi(g_arim_settings.max_file_size);
            if (stats.st_size > max) {
                return -2;
            }
        } else {
            return -3;
        }
    } else {
        return -1;
    }
    fp = fopen(fn, "r");
    if (fp == NULL)
        return -1;
    len = fread(msgbuffer, 1, msgbufsize - 1, fp);
    fclose(fp);
    msgbuffer[len] = '\0';
    /* send the file */
    return arim_send_msg(msgbuffer, to_call);
}

int ui_get_dyn_file(const char *fn, const char *cmd,
                        char *filebuf, size_t filebufsize)
{
    FILE *fp;
    size_t len, max, cnt = 0;
    char cmd_line[MAX_CMD_SIZE];

    if (atoi(g_arim_settings.max_file_size) <= 0) {
        snprintf(filebuf, filebufsize, "File: file sharing disabled.\n");
        return 0;
    }
    max = atoi(g_arim_settings.max_file_size);
    pthread_mutex_lock(&mutex_df_error_log);
    snprintf(cmd_line, sizeof(cmd_line), "%s 2>> %s", cmd, g_df_error_fn);
    pthread_mutex_unlock(&mutex_df_error_log);
    fp = popen(cmd_line, "r");
    if (!fp) {
        snprintf(filebuf, filebufsize, "File: %s read failed.\n", fn);
        return 0;
    }
    snprintf(filebuf, filebufsize, "File: %s\n\n", fn);
    cnt = strlen(filebuf);
    len = fread(filebuf + cnt, 1, max, fp);
    pclose(fp);
    if (len == 0) {
        snprintf(filebuf, filebufsize, "File: %s read failed.\n", fn);
        return 0;
    } else if (len > max) {
        snprintf(filebuf, filebufsize, "File: %s size exceeds limit.\n", fn);
        return 0;
    }
    cnt += len;
    filebuf[cnt] = '\0';
    return 1;
}

int ui_get_file(const char *fn, char *filebuf, size_t filebufsize)
{
    FILE *fp;
    struct stat stats;
    char fpath[MAX_PATH_SIZE];
    size_t len, cnt = 0, max;

    if (strstr(fn, "..")) {
        /* prevent directory traversal */
        snprintf(filebuf, filebufsize, "File: illegal file name.\n");
        return 0;
    }
    snprintf(fpath, sizeof(fpath), "%s", fn);
    if (strstr(basename(fpath), DEFAULT_DIGEST_FNAME)) {
        /* deny access to password digest file */
        snprintf(filebuf, filebufsize, "File: %s not found.\n", fn);
        return 0;
    }
    max = atoi(g_arim_settings.max_file_size);
    if (max <= 0) {
        snprintf(filebuf, filebufsize, "File: file sharing disabled.\n");
        return 0;
    }
    snprintf(fpath, sizeof(fpath), "%s/%s", g_arim_settings.files_dir, fn);
    if (stat(fpath, &stats) == 0) {
        if (!S_ISDIR(stats.st_mode)) {
            if (max > stats.st_size) {
                fp = fopen(fpath, "r");
                if (fp == NULL) {
                    snprintf(filebuf, filebufsize, "File: %s not found.\n", fn);
                    return 0;
                }
                snprintf(filebuf, filebufsize, "File: %s\n\n", fn);
                cnt = strlen(filebuf);
                len = fread(filebuf + cnt, 1, filebufsize - cnt - 1, fp);
                cnt += len;
                fclose(fp);
                filebuf[cnt] = '\0';
            } else {
                snprintf(filebuf, filebufsize, "File: %s size exceeds limit.\n", fn);
                return 0;
            }
        } else {
            snprintf(filebuf, filebufsize, "File: %s is a directory.\n", fn);
            return 0;
        }
    } else {
        snprintf(filebuf, filebufsize, "File: %s not found.\n", fn);
        return 0;
    }
    return 1;
}

void ui_print_file_reader_title(const char *path)
{
    char *p, title[MAX_DIR_PATH_SIZE], temp[MAX_DIR_PATH_SIZE];
    int center, start;
    size_t len, max_len;

    snprintf(temp, sizeof(temp), "%s", path);
    snprintf(title, sizeof(title), " READ FILE: %s ", basename(temp));
    len = strlen(title);
    /* abbreviate path if necessary to fit in line */
    max_len = tnc_cmd_box_w - 2;
    if (sizeof(title) < max_len)
        max_len = sizeof(title);
    p = temp;
    while (len > max_len && strlen(p) > 3) {
        p += 3;
        snprintf(title, sizeof(title), " READ FILE: ...%s ", p);
        len = strlen(title);
    }
    center = (tnc_cmd_box_w / 2) - 1;
    start = center - (len / 2);
    if (start < 1)
        start = 1;
    mvwprintw(tnc_cmd_box, tnc_cmd_box_h - 1, start, title);
    wrefresh(tnc_cmd_box);
}

int ui_read_file(const char *fn, int index)
{
    WINDOW *read_pad;
    FILE *fp;
    size_t i, len;
    int cmd, max_pad_rows = 0, top = 0, quit = 0;
    int max_read_rows, max_read_cols, min_read_rows, min_read_cols, num_read_rows;
    char filebuf[MAX_UNCOMP_DATA_SIZE+1], status[MAX_STATUS_BAR_SIZE];

    fp = fopen(fn, "r");
    if (fp == NULL)
        return 0;
    len = fread(filebuf, 1, sizeof(filebuf), fp);
    fclose(fp);
    filebuf[len] = '\0';
    for (i = 0; i < len; i++) {
        if (filebuf[i] == '\n') {
            ++max_pad_rows;
            /* convert CRLF line endings */
            if (i && filebuf[i - 1] == '\r') {
                /* shift remaining text including terminating null */
                memmove(&filebuf[i - 1], &filebuf[i], len - i + 1);
                --i;
                --len;
            }
        }
    }
    min_read_rows = tnc_cmd_box_y + 1;
    max_read_rows = min_read_rows + tnc_cmd_box_h - 3;
    min_read_cols = tnc_cmd_box_x + 2;
    max_read_cols = min_read_cols + tnc_cmd_box_w - 4;
    num_read_rows = max_read_rows - min_read_rows;
    if (show_titles)
        ui_print_file_reader_title(fn);
    read_pad = newpad(max_pad_rows + num_read_rows, max_read_cols);
    if (!read_pad)
        return 0;
    if (color_code)
        wbkgd(read_pad, COLOR_PAIR(7));
    waddstr(read_pad, filebuf);
    prefresh(read_pad, top, 0, min_read_rows, min_read_cols,
                 max_read_rows, max_read_cols);
    if (index == -1) /* special case, configuration file */
        snprintf(status, sizeof(status),
                "Config file: %d lines - use UP, DOWN keys to scroll, 'q' to quit",
                    max_pad_rows);
    else if (index == -2) /* special case, password digest file */
        snprintf(status, sizeof(status),
                "Password digest file: %d lines - use UP, DOWN keys to scroll, 'q' to quit",
                    max_pad_rows);
    else if (index == -3) /* special case, themes file */
        snprintf(status, sizeof(status),
                "UI themes file: %d lines - use UP, DOWN keys to scroll, 'q' to quit",
                    max_pad_rows);
    else
        snprintf(status, sizeof(status),
                "File [%d]: %d lines - use UP, DOWN keys to scroll, 'q' to quit",
                    index, max_pad_rows);
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

int ui_get_file_list(const char *basedir, const char *dir,
                     char *listbuf, size_t listbufsize)
{
    DIR *dirp;
    struct dirent *dent;
    struct stat stats;
    char *p, linebuf[MAX_DIR_LINE_SIZE];
    char fn[MAX_PATH_SIZE*2], path[MAX_PATH_SIZE*2];
    size_t i, len, max_file_size, cnt = 0;
    int numch;

    if (atoi(g_arim_settings.max_file_size) <= 0) {
        snprintf(listbuf, listbufsize, "File list: file sharing disabled.\n");
        return 0;
    }
    /* dir may be null if flist query has no argument */
    if (dir) {
        /* check if access to this dir is allowed */
        if (strstr(dir, "..")) {
            /* prevent directory traversal */
            snprintf(listbuf, listbufsize, "File list: illegal directory name.\n");
            return 0;
        }
        snprintf(path, sizeof(path), "%s/%s", basedir, dir);
        if (!ini_check_add_files_dir(path) && !ini_check_ac_files_dir(path)) {
            snprintf(listbuf, listbufsize, "File list: directory %s not found.\n", dir);
            return 0;
        }
    } else {
        snprintf(path, sizeof(path), "%s", basedir);
    }
    dirp = opendir(path);
    if (!dirp) {
        snprintf(listbuf, listbufsize, "File list: cannot open directory %s.\n", dir);
        return 0;
    }
    if (dir)
        snprintf(listbuf, listbufsize, "File list: %s\n", dir);
    else
        snprintf(listbuf, listbufsize, "File list:\n");
    cnt += strlen(listbuf);
    i = 0;
    max_file_size = atoi(g_arim_settings.max_file_size);
    dent = readdir(dirp);
    while (dent) {
        numch = snprintf(fn, sizeof(fn), "%s/%s", path, dent->d_name);
        if (stat(fn, &stats) == 0) {
            if (!S_ISDIR(stats.st_mode)) {
                /* don't list password digest file */
                if (!strstr(dent->d_name, DEFAULT_DIGEST_FNAME)) {
                    /* if not in ARQ mode (where compression is available), don't list
                       files whose size is greater than the max set in config file */
                    if (arim_is_arq_state() || stats.st_size <= max_file_size) {
                        numch = snprintf(linebuf, sizeof(linebuf),
                                         "%24s%8jd\n", dent->d_name, (intmax_t)stats.st_size);
                        if (numch >= sizeof(linebuf))
                            ui_truncate_line(linebuf, sizeof(linebuf));
                        len = strlen(linebuf);
                        if ((cnt + len) < listbufsize) {
                            strncat(listbuf, linebuf, listbufsize - cnt - 1);
                            cnt += len;
                        }
                    }
                }
            } else if (strcmp(dent->d_name, "..") && strcmp(dent->d_name, ".")) {
                if (ini_check_add_files_dir(fn)) {
                    numch = snprintf(linebuf, sizeof(linebuf), "%24s%8s\n", dent->d_name, "DIR");
                    if (numch >= sizeof(linebuf))
                        ui_truncate_line(linebuf, sizeof(linebuf));
                    len = strlen(linebuf);
                    if ((cnt + len) < listbufsize) {
                        strncat(listbuf, linebuf, listbufsize - cnt - 1);
                        cnt += len;
                    }
                } else if (ini_check_ac_files_dir(fn)) {
                    numch = snprintf(linebuf, sizeof(linebuf), "%24s%8s\n", dent->d_name, "!DIR");
                    if (numch >= sizeof(linebuf))
                        ui_truncate_line(linebuf, sizeof(linebuf));
                    len = strlen(linebuf);
                    if ((cnt + len) < listbufsize) {
                        strncat(listbuf, linebuf, listbufsize - cnt - 1);
                        cnt += len;
                    }
                }
            }
        }
        dent = readdir(dirp);
    }
    closedir(dirp);
    /* list dynamic files only for shared files root dir */
    if (!dir) {
        for (i = 0; i < g_arim_settings.dyn_files_cnt; i++) {
            snprintf(fn, sizeof(fn), "%s", g_arim_settings.dyn_files[i]);
            p = strstr(fn, ":");
            if (p) {
                *p = '\0';
                numch = snprintf(linebuf, sizeof(linebuf), "%24s%8s\n", fn, "DYN");
                if (numch >= sizeof(linebuf))
                    ui_truncate_line(linebuf, sizeof(linebuf));
                len = strlen(linebuf);
                if ((cnt + len) < listbufsize) {
                    strncat(listbuf, linebuf, listbufsize - cnt - 1);
                    cnt += len;
                }
            }
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
    return 1;
}

void ui_print_file_list_title(const char *path, const char *label)
{
    char *p, title[MAX_DIR_PATH_SIZE], temp[MAX_DIR_PATH_SIZE];
    int center, start, numch;
    size_t len, max_len;

    numch = snprintf(temp, sizeof(temp), "%s", path);
    numch = snprintf(title, sizeof(title), " %s: %s ", label, temp);
    len = strlen(title);
    max_len = tnc_data_box_w - 2;
    if (sizeof(title) < max_len)
        max_len = sizeof(title);
    p = temp;
    while (len > max_len && *p) {
        /* abbreviate directory path if it won't fit */
        while (*p && *p != '/')
            ++p;
        if (*p)
            ++p;
        snprintf(title, sizeof(title), " %s: .../%s ", label, p);
        len = strlen(title);
    }
    center = (tnc_data_box_w / 2) - 1;
    start = center - (len / 2);
    if (start < 1)
        start = 1;
    mvwhline(tnc_data_box, tnc_data_box_h - 1, 1, 0, tnc_data_box_w - 2);
    mvwprintw(tnc_data_box, tnc_data_box_h - 1, start, title);
    wrefresh(tnc_data_box);
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

int ui_files_get_line(char *cmd_line, size_t max_len)
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

void ui_list_files(const char *dir)
{
    WINDOW *dir_win;
    DIR *dirp;
    struct dirent *dent;
    struct stat stats;
    char linebuf[MAX_DIR_LINE_SIZE+1], msgbuffer[MAX_UNCOMP_DATA_SIZE];
    char path[MAX_DIR_LIST_LEN+1][MAX_DIR_PATH_SIZE+MAX_FILE_NAME_SIZE+1];
    char list[MAX_DIR_LIST_LEN+1][MAX_DIR_LINE_SIZE];
    char fn[MAX_FILE_NAME_SIZE], dpath[MAX_DIR_PATH_SIZE];
    char temp[MAX_PATH_SIZE], to_call[MAX_CALLSIGN_SIZE];
    char *p, *destdir, timestamp[MAX_TIMESTAMP_SIZE];
    int i, max_cols, max_dir_rows, max_dir_lines, max_len;
    int cmd, cur, top, numch, quit = 0, level = 0, zoption = 0;
    size_t len;

    dir_win = newwin(tnc_data_box_h - 2, tnc_data_box_w - 2,
                                 tnc_data_box_y + 1, tnc_data_box_x + 1);
    if (!dir_win) {
        ui_print_status("List files: failed to create list window", 1);
        return;
    }
    if (color_code)
        wbkgd(dir_win, COLOR_PAIR(7));
    max_dir_rows = tnc_data_box_h - 2;
    max_cols = (tnc_data_box_w - 4) + 1;
    if (max_cols > MAX_DIR_LINE_SIZE)
        max_cols = MAX_DIR_LINE_SIZE;
    ui_set_active_win(dir_win);
    snprintf(dpath, sizeof(dpath), "%s", dir);

restart:
    wclear(dir_win);
    memset(&list, 0, sizeof(list));
    memset(&path, 0, sizeof(path));
    dirp = opendir(dpath);
    if (!dirp) {
        ui_print_status("List files: failed to open shared files directory", 1);
        return;
    }
    if (level)
        i = 1;
    else
        i = 0;
    dent = readdir(dirp);
    while (dent) {
        /* stat the file */
        snprintf(temp, sizeof(temp), "%s/%s", dpath, dent->d_name);
        if (stat(temp, &stats) == 0) {
            /* calculate max file name length, display line format is:
               ----- -----------------------   -    --------- -----------------
               nbr=5    name (variable)      flag=1   size=9       time=17
               ----- -----------------------   -    --------- -----------------
               [  1] test.txt                             242 Aug 28 02:35 2017
               ----- -----------------------   -    --------- -----------------
               [  2] admin                     !    DIRECTORY Aug 28 02:35 2017
               ----- -----------------------   -    --------- -----------------
               a flag value of '!' indicates an access controlled directory
            */
            max_len = max_cols - (5 + 1 + 0 + 1 + 1 + 9 + 1 + 17) - 1;
            snprintf(fn, sizeof(fn), "%s", dent->d_name);
            len = strlen(fn);
            p = temp;
            /* abbreviate file name if it won't fit in line */
            while (len > max_len && strlen(p) > 3) {
                p += 3;
                snprintf(fn, sizeof(fn), "...%s", p);
                len = strlen(fn);
            }
            /* store entry into list */
            if (S_ISDIR(stats.st_mode)) {
                if (strcmp(dent->d_name, ".")) {
                    if (strcmp(dent->d_name, "..")) {
                        snprintf(list[i], max_cols + 1, "D[%3d] %-*s %c%9s %17s",
                                 i + 1, max_len, fn, ini_check_ac_files_dir(temp) ? '!' : ' ',
                                     "DIRECTORY", util_file_timestamp(stats.st_mtime,
                                        timestamp, sizeof(timestamp)));
                        /* store in path list */
                        snprintf(path[i], sizeof(path[0]), "%s/%s", dpath, dent->d_name);
                        ++i;
                    } else if (level) {
                        /* put parent directory at top of listing */
                        snprintf(list[0], max_cols + 1, "D[%3d] %-*s %c%9s %17s",
                                 1, max_len, fn, ini_check_ac_files_dir(temp) ? '!' : ' ',
                                     "DIRECTORY", util_file_timestamp(stats.st_mtime,
                                        timestamp, sizeof(timestamp)));
                        /* store in path list */
                        snprintf(path[0], sizeof(path[0]), "%s/%s", dpath, dent->d_name);
                    }
                }
            } else {
                snprintf(list[i], max_cols + 1, "F[%3d] %-*s  %9jd %17s",
                         i + 1, max_len, fn, (intmax_t)stats.st_size,
                            util_file_timestamp(stats.st_mtime,
                                timestamp, sizeof(timestamp)));
                /* store in path list */
                snprintf(path[i], sizeof(path[0]), "%s/%s", dpath, dent->d_name);
                ++i;
            }
            if (i == MAX_DIR_LIST_LEN) {
                snprintf(temp, sizeof(temp),
                    "\tToo many files to list;\n"
                        "\tonly the first %d are shown.\n \n\t[O]k", i);
                cmd = ui_show_dialog(temp, "oO \n");
                break;
            }
        }
        dent = readdir(dirp);
    }
    closedir(dirp);
    max_dir_lines = i;
    cur = top = 0;
    for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
        mvwprintw(dir_win, i, 1, &(list[cur][1]));
        ++cur;
    }
    wrefresh(dir_win);
    if (show_titles)
        ui_print_file_list_title(dpath, "LIST FILES");
    status_timer = 1;
    while (!quit) {
        if (status_timer && --status_timer == 0) {
            if (arim_is_arq_state())
                ui_print_status("<SP> for prompt: 'cd n' ch dir, 'rf n' read, "
                    "'sf [-z] n [dir]' send, 'ri' " DEFAULT_INI_FNAME ", 'q' quit", 0);
            else
                ui_print_status("<SP> for prompt: 'cd n' ch dir, 'rf n' read, "
                    "'sf n call' send, 'ri' " DEFAULT_INI_FNAME ", 'q' quit", 0);
        }
        cmd = getch();
        switch (cmd) {
        case ' ':
            memset(linebuf, 0, sizeof(linebuf));
            ui_files_get_line(linebuf, max_cols - 1);
            /* process the command */
            if (linebuf[0] == ':') {
                if (g_tnc_attached)
                    bufq_queue_data_out(&linebuf[1]);
                break;
            } else if (linebuf[0] == '!') {
                if (g_tnc_attached)
                    bufq_queue_cmd_out(&linebuf[1]);
                break;
            } else if (!strncasecmp(linebuf, "passwd", 4)) {
                cmdproc_cmd(linebuf);
            } else if (!strncasecmp(linebuf, "delpass", 4)) {
                cmdproc_cmd(linebuf);
            } else if (linebuf[0] == 'q') {
                quit = 1;
            } else {
                p = strtok(linebuf, " \t");
                if (!p)
                    break;
                if (!strncasecmp(p, "rf", 2)) {
                    p = strtok(NULL, " \t");
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Read file: invalid file number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= max_dir_lines) {
                        if (list[i][0] == 'F') {
                            /* ordinary data file, try to read it */
                            if (!ui_read_file(path[i], (int)(i + 1)))
                                ui_print_status("Read file: cannot read file", 1);
                        } else {
                            ui_print_status("Read file: cannot read directory", 1);
                        }
                    } else {
                        ui_print_status("Read file: invalid file number", 1);
                    }
                    if (show_recents)
                        ui_refresh_recents();
                } else if (!strncasecmp(p, "cd", 2)) {
                    p = strtok(NULL, " \t");
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Change directory: invalid directory number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= max_dir_lines) {
                        if (list[i][0] == 'D') {
                            /* directory, try to open and list it */
                            numch = snprintf(fn, sizeof(fn), "%s", path[i]);
                            p = strstr(fn, "/..");
                            if (p) {
                                /* go up one level */
                                --p;
                                while (p > fn && *p != '/')
                                    --p;
                                *p = '\0';
                                snprintf(dpath, sizeof(dpath), "%s", fn);
                                --level;
                                /* redraw the file listing view */
                                goto restart;
                            }
                            snprintf(dpath, sizeof(dpath), "%s", fn);
                            ++level;
                            /* redraw the file listing view */
                            goto restart;
                        } else {
                            ui_print_status("Change directory: not a directory", 1);
                        }
                    } else {
                        ui_print_status("Change directory: invalid directory number", 1);
                    }
                    if (show_recents)
                        ui_refresh_recents();
                } else if (!strncasecmp(p, "ri", 2)) {
                    if (!ui_read_file(g_config_fname, -1))
                        ui_print_status("Read file: cannot open configuration file", 1);
                    if (show_recents)
                        ui_refresh_recents();
                } else if (!strncasecmp(p, "rp", 2)) {
                    if (!ui_read_file(g_arim_digest_fname, -2))
                        ui_print_status("Read file: cannot open password file", 1);
                    if (show_recents)
                        ui_refresh_recents();
                } else if (!strncasecmp(p, "rt", 2)) {
                    if (!ui_read_file(g_arim_themes_fname, -3))
                        ui_print_status("Read file: cannot open themes file", 1);
                    if (show_recents)
                        ui_refresh_recents();
                } else if (!strncasecmp(p, "sf", 2)) {
                    if (!g_tnc_attached) {
                        ui_print_status("Send file: cannot send, no TNC attached", 1);
                        break;
                    }
                    zoption = 0;
                    p = strtok(NULL, " >\t");
                    if (p && !strcmp(p, "-z")) {
                        if (!arim_is_arq_state()) {
                            ui_print_status("Send file: -z option not supported in FEC mode", 1);
                            break;
                        }
                        p = strtok(NULL, " >\t");
                        zoption = 1;
                    }
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Send file: invalid file number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= max_dir_lines) {
                        if (list[i][0] == 'F') {
                            if (arim_is_arq_state()) {
                                if (arim_get_state() == ST_ARQ_CONNECTED) {
                                    /* get destination directory path */
                                    destdir = strtok(NULL, "\0");
                                    if (destdir) {
                                        snprintf(temp, sizeof(temp), "%s", destdir);
                                        destdir = temp;
                                    }
                                    /* initiate ARQ file upload */
                                    p = path[i] + strlen(g_arim_settings.files_dir) + 1;
                                    snprintf(fn, sizeof(fn), "%s", p);
                                    if (destdir)
                                        snprintf(msgbuffer, sizeof(msgbuffer), "%s %s > %s",
                                                 zoption ? "/FPUT -z" : "/FPUT", fn, destdir);
                                    else
                                        snprintf(msgbuffer, sizeof(msgbuffer), "%s %s",
                                                 zoption ? "/FPUT -z" : "/FPUT", fn);
                                    cmdproc_cmd(msgbuffer);
                                } else {
                                    ui_print_status("Send file: cannot send, TNC busy", 1);
                                }
                            } else {
                                p = strtok(NULL, " \t");
                                if (!p || !ini_validate_mycall(p)) {
                                    ui_print_status("Send file: invalid call sign", 1);
                                    break;
                                }
                                snprintf(to_call, sizeof(to_call), "%s", p);
                                numch = snprintf(fn, sizeof(fn), "%s", path[i]);
                                switch (ui_send_file(msgbuffer, sizeof(msgbuffer), fn, to_call)) {
                                case 0:
                                    ui_print_status("Send file: cannot send, TNC busy", 1);
                                    break;
                                case 1:
                                    ui_print_status("ARIM Busy: sending file", 1);
                                    break;
                                case -2:
                                    ui_print_status("Send file: cannot send, file size exceeds maximum", 1);
                                    break;
                                default:
                                    ui_print_status("Send file: cannot send, failed to open file", 1);
                                    break;
                                }
                            }
                        } else {
                            ui_print_status("Send file: cannot send directory", 1);
                        }
                    } else {
                        ui_print_status("Send file: invalid file number", 1);
                    }
                } else {
                    ui_print_status("List files: command not available in this view", 1);
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
            top = 0;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case KEY_END:
            if (max_dir_lines < max_dir_rows)
                break;
            top = max_dir_lines - max_dir_rows;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case KEY_NPAGE:
            top += max_dir_rows;
            if (top > max_dir_lines - 1)
                top = max_dir_lines - 1;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case '-':
        case KEY_PPAGE:
            top -= max_dir_rows;
            if (top < 0)
                top = 0;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case KEY_UP:
            top -= 1;
            if (top < 0)
                top = 0;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case '\n':
        case KEY_DOWN:
            top += 1;
            if (top > max_dir_lines - 1)
                top = max_dir_lines - 1;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
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
        if (g_win_changed)
            quit = 1;
        usleep(100000);
    }
    delwin(dir_win);
    ui_set_active_win(tnc_data_box);
    touchwin(tnc_data_box);
    wrefresh(tnc_data_box);
    if (show_titles)
        ui_print_data_win_title();
    status_timer = 1;
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

void ui_list_shared_files() {
    ui_list_files(g_arim_settings.files_dir);
}

void ui_list_remote_files(const char *flist, const char *dir)
{
    WINDOW *dir_win;
    static char cache[MAX_UNCOMP_DATA_SIZE];
    static char dpath[MAX_DIR_PATH_SIZE];
    static int is_root = 0;
    static int initialized = 0;
    char buffer[MAX_UNCOMP_DATA_SIZE], linebuf[MAX_DIR_LINE_SIZE+1];
    char list[MAX_DIR_LIST_LEN+1][MAX_DIR_LINE_SIZE];
    char path[MAX_DIR_LIST_LEN+1][MAX_DIR_PATH_SIZE+MAX_FILE_NAME_SIZE+1];
    char fn[MAX_FILE_NAME_SIZE], temp[MAX_PATH_SIZE], cmdbuffer[MAX_CMD_SIZE];
    char *p, *s, *e, *eob, *destdir;
    int i, max_cols, max_dir_rows, max_dir_lines;
    int cmd, cur, top, numch, quit = 0, zoption = 0;

    if (!flist && !initialized) {
        ui_print_status("List remote files: No data, you must run '/flget' first", 1);
        return;
    }
    dir_win = newwin(tnc_data_box_h - 2, tnc_data_box_w - 2,
                                 tnc_data_box_y + 1, tnc_data_box_x + 1);
    if (!dir_win) {
        ui_print_status("List remote files: failed to create list window", 1);
        return;
    }
    if (color_code)
        wbkgd(dir_win, COLOR_PAIR(7));
    max_dir_rows = tnc_data_box_h - 2;
    max_cols = (tnc_data_box_w - 4) + 1;
    if (max_cols > MAX_DIR_LINE_SIZE)
        max_cols = MAX_DIR_LINE_SIZE;
    ui_set_active_win(dir_win);
    wclear(dir_win);

    if (flist) {
        /* /flget data passed in, copy it into buffer */
        if (dir) {
            snprintf(dpath, sizeof(dpath), "%s", dir);
            is_root = 0;
        } else {
            snprintf(dpath, sizeof(dpath), "%s", "/");
            is_root = 1;
        }
        snprintf(cache, sizeof(cache), "%s", flist);
        initialized = 1;
    }
    snprintf(buffer, sizeof(buffer), "%s", cache);
    eob = buffer + strlen(buffer) - 1;
    memset(&list, 0, sizeof(list));
    memset(&path, 0, sizeof(path));
    i = 0;
    if (!is_root) {
        /* put parent directory at top of listing */
        snprintf(list[0], sizeof(list[0]), "D[01]%24s%8s\n", ".." , "DIR");
        /* store in path list */
        snprintf(temp, sizeof(temp), "%s", dpath);
        p = temp + strlen(temp) - 1;
        while (p > temp && *p != '/')
            --p;
        *p = '\0';
        numch = snprintf(path[i], sizeof(path[0]), "%s", temp);
        ++i;
    }
    s = buffer;
    e = buffer;
    /* skip first line */
    while (*e && e < eob && *e != '\n')
        ++e;
    s = e + 1;
    e = s;
    do {
        while (*e && e < eob && *e != '\n')
            ++e;
        /* is this the end? */
        if (s == strstr(s, "End\n"))
            break;
        /* no, add to list */
        *e = '\0';
        p = e - 3;
        if (!strncmp(p, "DIR", 3))
            snprintf(list[i], sizeof(list[0]), "D[%02d]%s", i + 1, s);
        else
            snprintf(list[i], sizeof(list[0]), "F[%02d]%s", i + 1, s);
        /* extract file name */
        p = e;
        while (p > s && *p != ' ')
            --p;
        while (p > s && *p == ' ')
            --p;
        ++p;
        *p = '\0';
        while (*s && *s == ' ')
            ++s;
        snprintf(fn, sizeof(fn), "%s", s);
        /* store in path list */
        if (!is_root)
            snprintf(path[i], sizeof(path[0]), "%s/%s", dpath, fn);
        else
            snprintf(path[i], sizeof(path[0]), "%s", fn);
        ++i;
        if (i == MAX_DIR_LIST_LEN)
            break;
        s = e + 1;
        e = s;
    } while (s < eob);
    max_dir_lines = i;
    cur = top = 0;
    for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
        mvwprintw(dir_win, i, 1, &(list[cur][1]));
        ++cur;
    }
    wrefresh(dir_win);
    if (show_titles)
        ui_print_file_list_title(dpath, "LIST REMOTE FILES");
    status_timer = 1;
    while (!quit) {
        if (status_timer && --status_timer == 0) {
            ui_print_status("<SP> for prompt: 'cd [-z] n' ch dir, 'rf n' read, "
                "'gf [-z] n [dir]' get, 'q' quit", 0);
        }
        cmd = getch();
        switch (cmd) {
        case ' ':
            memset(linebuf, 0, sizeof(linebuf));
            ui_files_get_line(linebuf, max_cols - 1);
            /* process the command */
            if (linebuf[0] == 'q') {
                quit = 1;
            } else {
                p = strtok(linebuf, " \t");
                if (!p)
                    break;
                if (!strncasecmp(p, "rf", 2)) {
                    p = strtok(NULL, " \t");
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Read file: invalid file number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= max_dir_lines) {
                        if (list[i][0] == 'F') {
                            /* ordinary data file, try to read it */
                            numch = snprintf(cmdbuffer, sizeof(cmdbuffer), "/FILE %s", path[i]);
                            cmdproc_cmd(cmdbuffer);
                            /* close the file listing view so op can read file in tfc monitor */
                            quit = 1;
                        } else {
                            ui_print_status("Read file: cannot read directory", 1);
                        }
                    } else {
                        ui_print_status("Read file: invalid file number", 1);
                    }
                } else if (!strncasecmp(p, "cd", 2)) {
                    zoption = 0;
                    p = strtok(NULL, " \t");
                    if (p && !strcmp(p, "-z")) {
                        p = strtok(NULL, " >\t");
                        zoption = 1;
                    }
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Change directory: invalid directory number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= max_dir_lines) {
                        if (list[i][0] == 'D') {
                            /* directory, try to list it */
                            numch = snprintf(cmdbuffer, sizeof(cmdbuffer),
                                             "%s %s", zoption ? "/FLGET -z" : "/FLGET", path[i]);
                            cmdproc_cmd(cmdbuffer);
                            /* close the file listing view, it will reopen when listing received */
                            quit = 1;
                        } else {
                            ui_print_status("Change directory: not a directory", 1);
                        }
                    } else {
                        ui_print_status("Change directory: invalid directory number", 1);
                    }
                    if (show_recents)
                        ui_refresh_recents();
                } else if (!strncasecmp(p, "gf", 2)) {
                    zoption = 0;
                    p = strtok(NULL, " >\t");
                    if (p && !strcmp(p, "-z")) {
                        p = strtok(NULL, " >\t");
                        zoption = 1;
                    }
                    if (!p || !(i = atoi(p))) {
                        ui_print_status("Get file: invalid file number", 1);
                        break;
                    }
                    --i;
                    if (i >= 0 && i <= max_dir_lines) {
                        if (list[i][0] == 'F') {
                            if (arim_get_state() == ST_ARQ_CONNECTED) {
                                /* get destination directory path */
                                destdir = strtok(NULL, "\0");
                                if (destdir) {
                                    snprintf(temp, sizeof(temp), "%s", destdir);
                                    destdir = temp;
                                }
                                /* initiate ARQ file downlaod */
                                if (destdir)
                                    numch = snprintf(cmdbuffer, sizeof(cmdbuffer), "%s %s > %s",
                                                     zoption ? "/FGET -z" : "/FGET", path[i], destdir);
                                else
                                    numch = snprintf(cmdbuffer, sizeof(cmdbuffer), "%s %s",
                                                     zoption ? "/FGET -z" : "/FGET", path[i]);
                                cmdproc_cmd(cmdbuffer);
                            } else {
                                ui_print_status("Get file: cannot download, TNC busy", 1);
                            }
                        } else {
                            ui_print_status("Get file: cannot download directory", 1);
                        }
                    } else {
                        ui_print_status("Get file: invalid file number", 1);
                    }
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
            break;
        case KEY_HOME:
            top = 0;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case KEY_END:
            if (max_dir_lines < max_dir_rows)
                break;
            top = max_dir_lines - max_dir_rows;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case KEY_NPAGE:
            top += max_dir_rows;
            if (top > max_dir_lines - 1)
                top = max_dir_lines - 1;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case '-':
        case KEY_PPAGE:
            top -= max_dir_rows;
            if (top < 0)
                top = 0;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case KEY_UP:
            top -= 1;
            if (top < 0)
                top = 0;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
            break;
        case '\n':
        case KEY_DOWN:
            top += 1;
            if (top > max_dir_lines - 1)
                top = max_dir_lines - 1;
            cur = top;
            wclear(dir_win);
            for (i = 0; i < max_dir_rows && cur < max_dir_lines; i++) {
                mvwprintw(dir_win, i, 1, &(list[cur][1]));
                ++cur;
            }
            wrefresh(dir_win);
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
            cmd = ui_show_dialog("\tAre you sure\n\tyou want to disconnect?\n \n\t[Y]es   [N]o", "yYnN");
            if (cmd == 'y' || cmd == 'Y') {
                arim_arq_send_disconn_req();
                quit = 1;
            }
            break;
        case 27:
            ui_on_cancel();
            quit = 1;
            break;
        default:
            ui_print_cmd_in();
            ui_print_heard_list();
            ui_check_status_dirty();
            /* quit if ARQ session has ended */
            if (!arim_is_arq_state())
                quit = 1;
            break;
        }
        if (g_win_changed)
            quit = 1;
        usleep(100000);
    }
    delwin(dir_win);
    ui_set_active_win(tnc_data_box);
    touchwin(tnc_data_box);
    wrefresh(tnc_data_box);
    if (show_titles)
        ui_print_data_win_title();
    status_timer = 1;
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

