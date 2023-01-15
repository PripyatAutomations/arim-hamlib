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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <curses.h>
#include "main.h"
#include "ini.h"
#include "ui_themes.h"

#define MAX_THEME_LINE_SIZE  128
#define NUM_BUILT_IN_THEMES  2
#define MAX_NUM_THEMES       7

THEME themes[MAX_NUM_THEMES] = {
    {
        "dark",
        COLOR_BLACK,                /* background */
        COLOR_WHITE, A_NORMAL,      /* default text */
        COLOR_WHITE, A_BOLD,        /* status bar indicators */
        COLOR_WHITE, A_BOLD,        /* status bar notifications */
        COLOR_WHITE, A_BOLD,        /* dialog box text */
        COLOR_BLACK,                /* dialog box background */
        COLOR_WHITE, A_NORMAL,      /* clock */
        COLOR_WHITE, A_NORMAL,      /* new message/file counter */
        COLOR_WHITE, A_BOLD,        /* channel busy indicator */
        COLOR_WHITE, A_NORMAL,      /* title */
        COLOR_RED, A_NORMAL,        /* error */
        COLOR_GREEN, A_NORMAL,      /* message */
        COLOR_YELLOW, A_NORMAL,     /* query */
        COLOR_BLUE, A_BOLD,         /* ping */
        COLOR_BLUE, A_BOLD,         /* id */
        COLOR_CYAN, A_NORMAL,       /* net message */
        COLOR_MAGENTA, A_NORMAL,    /* beacon */
        COLOR_WHITE, A_NORMAL,      /* arq frame */
        A_BOLD,                     /* transmitted data frames */
        COLOR_CYAN, A_BOLD,         /* command to TNC */
        COLOR_RED, A_NORMAL,        /* PTT TRUE async response */
        COLOR_GREEN, A_NORMAL,      /* PTT FALSE async response */
        COLOR_YELLOW, A_NORMAL,     /* BUFFER async response */
        COLOR_BLUE, A_BOLD,         /* PING, PINGACK, PINGREPLY async response */
        COLOR_MAGENTA, A_NORMAL,    /* BUSY async response */
        COLOR_YELLOW, A_NORMAL,     /* NEWSTATE async response */
    },
    {
        "light",
        COLOR_WHITE,                /* background */
        COLOR_BLACK, A_NORMAL,      /* default text */
        COLOR_BLACK, A_BOLD,        /* status bar indicators */
        COLOR_BLACK, A_BOLD,        /* status bar notifications */
        COLOR_BLACK, A_BOLD,        /* dialog box text */
        COLOR_WHITE,                /* dialog box background */
        COLOR_BLACK, A_NORMAL,      /* clock */
        COLOR_BLACK, A_NORMAL,      /* new message/file counter */
        COLOR_BLACK, A_BOLD,        /* channel busy indicator */
        COLOR_BLACK, A_NORMAL,      /* title */
        COLOR_RED, A_NORMAL,        /* error */
        COLOR_GREEN, A_NORMAL,      /* message */
        COLOR_YELLOW, A_NORMAL,     /* query */
        COLOR_BLUE, A_NORMAL,       /* ping */
        COLOR_BLUE, A_NORMAL,       /* id */
        COLOR_CYAN, A_NORMAL,       /* net message */
        COLOR_MAGENTA, A_NORMAL,    /* beacon */
        COLOR_BLACK, A_NORMAL,      /* arq frame */
        A_BOLD,                     /* transmitted data frames */
        COLOR_CYAN, A_BOLD,         /* command to TNC */
        COLOR_RED, A_NORMAL,        /* PTT TRUE async response */
        COLOR_GREEN, A_NORMAL,      /* PTT FALSE async response */
        COLOR_YELLOW, A_NORMAL,     /* BUFFER async response */
        COLOR_BLUE, A_NORMAL,       /* PING, PINGACK, PINGREPLY async response */
        COLOR_MAGENTA, A_NORMAL,    /* BUSY async response */
        COLOR_BLUE, A_NORMAL,       /* NEWSTATE async response */
    },
};
int theme;
int which_theme;
char g_arim_themes_fname[MAX_PATH_SIZE];

char *ui_themes_get_value(const char *key, char *line)
{
    static char *v;
    char *p, *s, *e;

    if ((s = strstr(line, "="))) {
        /* key/value pair, find start of key */
        p = line;
        while (*p && (*p == ' ' || *p == '\t'))
            ++p;
        /* find end of key */
        e = p;
        while (*e && *e != ' ' && *e != '\t' && *e != '=')
            ++e;
        /* test for exact match */
        if (p == strstr(p, key) && strlen(key) == (e - p)) {
            /* matches, find start of value */
            ++s;
            while (*s && (*s == ' ' || *s == '\t'))
                ++s;
            /* find end of value and null-terminate the key/value pair */
            e = s;
            while (*e && *e != '\r' && *e != '\n')
                ++e;
            --e;
            while (*e == ' ' || *e == '\t')
                --e;
            *(e + 1) = '\0';
            v = s;
            return v;
        }
    }
    return NULL;
}

int ui_themes_validate_attr(const char *val)
{
    if (!strncasecmp(val, "BLINK", 5))
        return A_BLINK;
    if (!strncasecmp(val, "BOLD", 4))
        return A_BOLD;
    if (!strncasecmp(val, "DIM", 3))
        return A_DIM;
#ifdef A_ITALIC
    if (!strncasecmp(val, "ITALIC", 6))
        return A_ITALIC;
#endif
    if (!strncasecmp(val, "NORMAL", 6))
        return A_NORMAL;
    if (!strncasecmp(val, "REVERSE", 7))
        return A_REVERSE;
    if (!strncasecmp(val, "STANDOUT", 8))
        return A_STANDOUT;
    if (!strncasecmp(val, "UNDERLINE", 9))
        return A_UNDERLINE;
    return A_NORMAL;
}

/**** from curses.h *****
#define COLOR_BLACK     0
#define COLOR_RED       1
#define COLOR_GREEN     2
#define COLOR_YELLOW    3
#define COLOR_BLUE      4
#define COLOR_MAGENTA   5
#define COLOR_CYAN      6
#define COLOR_WHITE     7
************************/

int ui_themes_validate_color(const char *val)
{
    if (!strncasecmp(val, "BLACK", 5))
        return COLOR_BLACK;
    if (!strncasecmp(val, "RED", 3))
        return COLOR_RED;
    if (!strncasecmp(val, "GREEN", 5))
        return COLOR_GREEN;
    if (!strncasecmp(val, "YELLOW", 6))
        return COLOR_YELLOW;
    if (!strncasecmp(val, "BLUE", 4))
        return COLOR_BLUE;
    if (!strncasecmp(val, "MAGENTA", 7))
        return COLOR_MAGENTA;
    if (!strncasecmp(val, "CYAN", 4))
        return COLOR_CYAN;
    if (!strncasecmp(val, "WHITE", 5))
        return COLOR_WHITE;
    return COLOR_BLACK;
}

int ui_themes_validate_theme(const char *theme)
{
    int i;

    for (i = 0; i < MAX_NUM_THEMES; i++) {
        if (strlen(theme) == strlen(themes[i].name))
            if (!strncasecmp(themes[i].name, theme, strlen(theme)))
                return i;
    }
    return -1;
}

int ui_themes_read_theme(FILE *themesfp, int which)
{
    char linebuf[MAX_THEME_LINE_SIZE];
    char *p, *v;
    int done = 0;

    /* populate with sane default values */
    memset(&themes[which], 0, sizeof(THEME));
    themes[which].ui_bg_color = COLOR_WHITE;
    themes[which].ui_dlg_bg_color = COLOR_WHITE;

    p = fgets(linebuf, sizeof(linebuf), themesfp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '[') {
                /* start of next section, done */
                fseek(themesfp, -(strlen(linebuf)), SEEK_CUR);
                done = 1;
                break;
            }
            else if ((v = ui_themes_get_value("name", p))) {
                snprintf(themes[which].name, sizeof(themes[which].name), "%s", v);
            }
            else if ((v = ui_themes_get_value("ui-background-color", p))) {
                themes[which].ui_bg_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-default-color", p))) {
                themes[which].ui_def_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-default-attr", p))) {
                themes[which].ui_def_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("ui-status-indicator-color", p))) {
                themes[which].ui_st_ind_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-status-indicator-attr", p))) {
                themes[which].ui_st_ind_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("ui-status-notify-color", p))) {
                themes[which].ui_st_noti_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-status-notify-attr", p))) {
                themes[which].ui_st_noti_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("ui-dialog-color", p))) {
                themes[which].ui_dlg_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-dialog-attr", p))) {
                themes[which].ui_dlg_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("ui-dialog-background-color", p))) {
                themes[which].ui_dlg_bg_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-clock-color", p))) {
                themes[which].ui_clock_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-clock-attr", p))) {
                themes[which].ui_clock_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("ui-msg-cntr-color", p))) {
                themes[which].ui_msg_cntr_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-msg-cntr-attr", p))) {
                themes[which].ui_msg_cntr_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("ui-ch-busy-color", p))) {
                themes[which].ui_ch_busy_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-ch-busy-attr", p))) {
                themes[which].ui_ch_busy_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("ui-title-color", p))) {
                themes[which].ui_title_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("ui-title-attr", p))) {
                themes[which].ui_title_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-error-color", p))) {
                themes[which].tm_err_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tm-error-attr", p))) {
                themes[which].tm_err_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-message-color", p))) {
                themes[which].tm_msg_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tm-message-attr", p))) {
                themes[which].tm_msg_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-query-color", p))) {
                themes[which].tm_qry_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tm-query-attr", p))) {
                themes[which].tm_qry_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-ping-color", p))) {
                themes[which].tm_ping_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tm-ping-attr", p))) {
                themes[which].tm_ping_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-id-color", p))) {
                themes[which].tm_id_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tm-id-attr", p))) {
                themes[which].tm_id_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-net-color", p))) {
                themes[which].tm_net_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tm-net-attr", p))) {
                themes[which].tm_net_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-beacon-color", p))) {
                themes[which].tm_bcn_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tm-beacon-attr", p))) {
                themes[which].tm_bcn_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-arq-color", p))) {
                themes[which].tm_arq_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tm-arq-attr", p))) {
                themes[which].tm_arq_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tm-tx-frame-attr", p))) {
                themes[which].tm_tx_frame_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tc-cmd-color", p))) {
                themes[which].tc_cmd_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tc-cmd-attr", p))) {
                themes[which].tc_cmd_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tc-ptt-true-color", p))) {
                themes[which].tc_ptt_t_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tc-ptt-true-attr", p))) {
                themes[which].tc_ptt_t_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tc-ptt-false-color", p))) {
                themes[which].tc_ptt_f_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tc-ptt-false-attr", p))) {
                themes[which].tc_ptt_f_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tc-buffer-color", p))) {
                themes[which].tc_buf_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tc-buffer-attr", p))) {
                themes[which].tc_buf_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tc-ping-color", p))) {
                themes[which].tc_ping_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tc-ping-attr", p))) {
                themes[which].tc_ping_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tc-busy-color", p))) {
                themes[which].tc_busy_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tc-busy-attr", p))) {
                themes[which].tc_busy_attr = ui_themes_validate_attr(v);
            }
            else if ((v = ui_themes_get_value("tc-newstate-color", p))) {
                themes[which].tc_newst_color = ui_themes_validate_color(v);
            }
            else if ((v = ui_themes_get_value("tc-newstate-attr", p))) {
                themes[which].tc_newst_attr = ui_themes_validate_attr(v);
            }
        }
        p = fgets(linebuf, sizeof(linebuf), themesfp);
    }
    return done;
}

int ui_themes_get_themes(const char *fn)
{
    FILE *themesfp;
    char *p, linebuf[MAX_THEME_LINE_SIZE];
    int which_theme;

    themesfp = fopen(fn, "r");
    if (themesfp == NULL)
        return 0;
    which_theme = NUM_BUILT_IN_THEMES;
    p = fgets(linebuf, sizeof(linebuf), themesfp);
    while (p) {
        if (*p != '#') {
            while (*p == ' ' || *p == '\t')
                p++;
            if (p == strstr(p, "[theme]")) {
                if (!ui_themes_read_theme(themesfp, which_theme))
                    break;
                which_theme++;
                if (which_theme == MAX_NUM_THEMES)
                    break;
            }
        }
        p = fgets(linebuf, sizeof(linebuf), themesfp);
    }
    fclose(themesfp);
    return which_theme;
}

int ui_themes_load_themes()
{
#ifndef PORTABLE_BIN
    FILE *themesfp, *srcfp;
    char *p, linebuf[MAX_THEME_LINE_SIZE];
    char src_fname[MAX_PATH_SIZE];
#endif

    snprintf(g_arim_themes_fname, sizeof(g_arim_themes_fname), "%s/%s", g_arim_path, DEFAULT_THEMES_FNAME);
    if (access(g_arim_themes_fname, F_OK) != 0) {
#ifndef PORTABLE_BIN
        if (errno == ENOENT) {
            themesfp = fopen(g_arim_themes_fname, "w");
            if (themesfp == NULL)
                return 0;
            snprintf(src_fname, sizeof(src_fname), ARIM_FILESDIR "/" DEFAULT_THEMES_FNAME);
            srcfp = fopen(src_fname, "r");
            if (srcfp == NULL) {
                fclose(themesfp);
                return 0;
            }
            p = fgets(linebuf, sizeof(linebuf), srcfp);
            while (p) {
                fprintf(themesfp, "%s", linebuf);
                p = fgets(linebuf, sizeof(linebuf), srcfp);
            }
            fclose(srcfp);
            fclose(themesfp);
        }
#else
        return 0;
#endif
    }
    return ui_themes_get_themes(g_arim_themes_fname);
}

