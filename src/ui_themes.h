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

#ifndef _UI_THEMES_H_INCLUDED_
#define _UI_THEMES_H_INCLUDED_

typedef struct {
    char name[16];                               /* name of theme */
    int ui_bg_color;                             /* background */
    int ui_def_color, ui_def_attr;               /* default text */
    int ui_st_ind_color, ui_st_ind_attr;         /* status bar indicators */
    int ui_st_noti_color, ui_st_noti_attr;       /* status bar notifications */
    int ui_dlg_color, ui_dlg_attr;               /* dialog box text */
    int ui_dlg_bg_color;                         /* dialog box background */
    int ui_clock_color, ui_clock_attr;           /* clock */
    int ui_msg_cntr_color, ui_msg_cntr_attr;     /* new message/file counter */
    int ui_ch_busy_color, ui_ch_busy_attr;       /* channel busy indicator */
    int ui_title_color, ui_title_attr;           /* title */
    int tm_err_color, tm_err_attr;               /* error */
    int tm_msg_color, tm_msg_attr;               /* message */
    int tm_qry_color, tm_qry_attr;               /* query */
    int tm_ping_color, tm_ping_attr;             /* ping */
    int tm_id_color, tm_id_attr;                 /* id */
    int tm_net_color, tm_net_attr;               /* net message */
    int tm_bcn_color, tm_bcn_attr;               /* beacon */
    int tm_arq_color, tm_arq_attr;               /* arq frames */
    int tm_tx_frame_attr;                        /* transmitted data frames */
    int tc_cmd_color, tc_cmd_attr;               /* command from ARIM to TNC */
    int tc_ptt_t_color, tc_ptt_t_attr;           /* PTT TRUE async response */
    int tc_ptt_f_color, tc_ptt_f_attr;           /* PTT FALSE async response */
    int tc_buf_color, tc_buf_attr;               /* BUFFER async response */
    int tc_ping_color, tc_ping_attr;             /* PING async response */
    int tc_busy_color, tc_busy_attr;             /* BUSY async response */
    int tc_newst_color, tc_newst_attr;           /* NEWSTATE async response */
} THEME;

extern THEME themes[];
extern int theme;
extern char g_arim_themes_fname[];

extern int ui_themes_load_themes(void);
extern int ui_themes_validate_theme(const char *theme);

#endif

