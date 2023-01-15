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
#include "arim_proto.h"
#include "arim_beacon.h"
#include "arim_message.h"
#include "arim_query.h"
#include "arim_arq.h"
#include "arim_arq_auth.h"
#include "arim_arq_msg.h"
#include "arim_arq_files.h"
#include "bufq.h"
#include "cmdproc.h"
#include "ini.h"
#include "log.h"
#include "ui.h"
#include "ui_dialog.h"
#include "ui_fec_menu.h"
#include "ui_files.h"
#include "ui_help_menu.h"
#include "ui_msg.h"
#include "ui_themes.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "ui_tnc_cmd_win.h"
#include "ui_cmd_prompt_win.h"
#include "util.h"
#include "datathread.h"

#define CH_BUSY_IND             "[RF CHANNEL BUSY]    "

WINDOW *main_win;
int win_change_timer;
int status_timer;
int title_dirty;
int status_dirty;
int show_titles;
int show_cmds;
int mon_timestamp;
int color_code;
int status_row, status_col;

int num_new_msgs;
int num_new_files;
int show_prog_meter;
static int xfer_dir, xfer_min, xfer_max, xfer_val;

void ui_status_xfer_start(int min, int max, int dir)
{
    pthread_mutex_lock(&mutex_status);
    xfer_min = xfer_val = min;
    xfer_max = max;
    xfer_dir = dir;
    pthread_mutex_unlock(&mutex_status);
    show_prog_meter = 1;
}

void ui_status_xfer_update(int val)
{
    pthread_mutex_lock(&mutex_status);
    xfer_val = val > xfer_max ? xfer_max : val;
    pthread_mutex_unlock(&mutex_status);
}

void ui_status_xfer_end()
{
    show_prog_meter = 0;
}

void ui_set_tnc_detached()
{
    ui_print_status("Detaching from TNC...", 1);
    pthread_mutex_lock(&mutex_title);
    title_dirty = TITLE_TNC_DETACHED;
    status_dirty = STATUS_REFRESH;
    pthread_mutex_unlock(&mutex_title);
}

void ui_set_title_dirty(int val)
{
    pthread_mutex_lock(&mutex_title);
    title_dirty = val;
    pthread_mutex_unlock(&mutex_title);
}

void ui_set_status_dirty(int val)
{
    pthread_mutex_lock(&mutex_status);
    status_dirty = val;
    pthread_mutex_unlock(&mutex_status);
}

void ui_truncate_line(char *line, size_t size)
{
    line[size-1] = '\0';
    line[size-2] = '.';
    line[size-3] = '.';
    line[size-4] = '.';
}

void ui_on_cancel()
{
    if (g_tnc_attached) {
        arim_on_event(EV_CANCEL, 0);
        arim_set_channel_not_busy(); /* force TNC not busy status */
    }
}

WINDOW *ui_set_active_win(WINDOW *win)
{
    static WINDOW *active_win = NULL;
    WINDOW *prev;

    prev = active_win;
    active_win = win;
    return prev;
}

void ui_print_status_ind()
{
    int start, state, numch;
    char idle_busy, tx_rx, ind[MAX_STATUS_IND_SIZE], fecmode[TNC_FECMODE_SIZE];
    char tnc_state[TNC_STATE_SIZE], remote_call[TNC_MYCALL_SIZE], bw_hz[TNC_ARQ_BW_SIZE];

    if (g_tnc_attached) {
        state = arim_get_state();
        switch (state) {
        case ST_ARQ_CONNECTED:
        case ST_ARQ_IN_CONNECT_WAIT:
        case ST_ARQ_OUT_CONNECT_WAIT_RPT:
        case ST_ARQ_OUT_CONNECT_WAIT:
        case ST_ARQ_MSG_RCV:
        case ST_ARQ_MSG_SEND_WAIT:
        case ST_ARQ_MSG_SEND:
        case ST_ARQ_FILE_RCV_WAIT_OK:
        case ST_ARQ_FILE_RCV_WAIT:
        case ST_ARQ_FILE_RCV:
        case ST_ARQ_FILE_SEND_WAIT:
        case ST_ARQ_FILE_SEND_WAIT_OK:
        case ST_ARQ_FILE_SEND:
        case ST_ARQ_FLIST_RCV_WAIT:
        case ST_ARQ_FLIST_RCV:
        case ST_ARQ_FLIST_SEND_WAIT:
        case ST_ARQ_FLIST_SEND:
        case ST_ARQ_AUTH_RCV_A2_WAIT:
        case ST_ARQ_AUTH_RCV_A3_WAIT:
        case ST_ARQ_AUTH_RCV_A4_WAIT:
        case ST_ARQ_AUTH_SEND_A1:
        case ST_ARQ_AUTH_SEND_A2:
        case ST_ARQ_AUTH_SEND_A3:
            arim_copy_remote_call(remote_call, sizeof(remote_call));
            arim_copy_tnc_state(tnc_state, sizeof(tnc_state));
            arim_copy_arq_bw_hz(bw_hz, sizeof(bw_hz));
            if (!strncasecmp(tnc_state, "IRStoISS", 8))
                snprintf(tnc_state, sizeof(tnc_state), "RtoS");
            numch = snprintf(ind, sizeof(ind),  " %c ARQ:%s%s %s S:%-4.4s",
                             (state == ST_ARQ_CONNECTED ? ' ' : '!'), remote_call,
                             (arim_arq_auth_get_status() ? "+" : ""), bw_hz, tnc_state);
            break;
        default:
            idle_busy = (state == ST_IDLE) ? 'I' : 'B';
            switch (state) {
            case ST_SEND_MSG_BUF_WAIT:
            case ST_SEND_NET_MSG_BUF_WAIT:
            case ST_SEND_QRY_BUF_WAIT:
            case ST_SEND_RESP_BUF_WAIT:
            case ST_SEND_ACKNAK_BUF_WAIT:
            case ST_SEND_BCN_BUF_WAIT:
            case ST_SEND_UN_BUF_WAIT:
            case ST_SEND_ACKNAK_PEND:
            case ST_SEND_RESP_PEND:
            case ST_SEND_PING_ACK_PEND:
            case ST_RCV_PING_ACK_WAIT:
            case ST_RCV_MSG_PING_ACK_WAIT:
            case ST_RCV_QRY_PING_ACK_WAIT:
            case ST_RCV_ARQ_CONN_PING_ACK_WAIT:
                tx_rx = 'T';
                break;
            default:
                tx_rx = 'R';
                break;
            }
            arim_copy_fecmode(fecmode, sizeof(fecmode));
            if (!g_btime)
                numch = snprintf(ind, sizeof(ind), " %c:%c %s:%d B:OFF",
                                 idle_busy, tx_rx, fecmode, arim_get_fec_repeats());
            else
                numch = snprintf(ind, sizeof(ind), " %c:%c %s:%d B:%03d",
                                 idle_busy, tx_rx, fecmode, arim_get_fec_repeats(), g_btime);
            break;
        }
        start = COLS - strlen(ind) - 1;
        if (start < status_col)
            start = status_col;
        wmove(main_win, status_row, start);
        wclrtoeol(main_win);
        if (color_code) {
            wattrset(main_win, COLOR_PAIR(10)|themes[theme].ui_st_ind_attr);
        } else {
            wattron(main_win, A_BOLD);
        }
        mvwprintw(main_win, status_row, start, "%s", ind);
        if (color_code)
            wattrset(main_win, COLOR_PAIR(7)|A_NORMAL);
        else
            wattroff(main_win, A_BOLD);
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

void ui_check_channel_busy()
{
    int start;

    start = COLS - strlen(CH_BUSY_IND) - 1;
    if (start < status_col)
        start = status_col;
    wmove(main_win, status_row + 1, start);
    wclrtoeol(main_win);
    if (!arim_is_arq_state() && arim_is_channel_busy()) {
        if (color_code) {
            wattrset(main_win, COLOR_PAIR(22)|themes[theme].ui_ch_busy_attr);
        } else {
            wattron(main_win, A_BOLD);
        }
        mvwprintw(main_win, status_row + 1, start, "%s", CH_BUSY_IND);
        if (color_code)
            wattrset(main_win, COLOR_PAIR(7)|A_NORMAL);
        else
            wattroff(main_win, A_BOLD);
    }
}

void ui_check_prog_meter()
{
    static int is_showing = 0;
    char meter[MAX_STATUS_BAR_SIZE*2];
    int i, start, stop, meter_len, max, val, dir;
    float pct, num_fill_ch;

    if (!show_prog_meter && is_showing) {
        is_showing = 0;
        wmove(main_win, status_row + 1, status_col);
        wclrtoeol(main_win);
        return;
    } else if (!show_prog_meter) {
        return;
    }
    is_showing = show_prog_meter;
    pthread_mutex_lock(&mutex_status);
    max = xfer_max;
    val = xfer_val;
    dir = xfer_dir;
    pthread_mutex_unlock(&mutex_status);
    /* compute progress meter layout */
    start = status_col;
    stop = COLS - strlen(CH_BUSY_IND) - 17;
    meter_len = stop - start - 7;
    num_fill_ch = ((float)val / (float)max) * (float)meter_len;
    for (i = 0; i < meter_len && i < sizeof(meter) - 1; i++) {
        if (i < (int)num_fill_ch)
            meter[i] = '#';
        else
            meter[i] = ' ';
    }
    meter[i] = '\0';
    /* compute progress meter percentage of completion */
    pct = ((float)val / (float)max) * 100.0;
    /* clear line and draw meter */
    wmove(main_win, status_row + 1, start);
    wclrtoeol(main_win);
    if (color_code) {
        wattrset(main_win, COLOR_PAIR(22)|themes[theme].ui_ch_busy_attr);
    } else {
        wattron(main_win, A_BOLD);
    }
    mvwprintw(main_win, status_row + 1, start, "[%s][%s %2.0f%%]", meter, dir ? "  Upload:" : "Download:", pct);
    if (color_code)
        wattrset(main_win, COLOR_PAIR(7)|A_NORMAL);
    else
        wattroff(main_win, A_BOLD);
}

void ui_print_status(const char *text, int temporary)
{
    static char status[MAX_STATUS_BAR_SIZE];

    if (text)
        snprintf(status, COLS - 2, "%s", text);
    wmove(main_win, status_row, 0);
    wclrtoeol(main_win);
    if (color_code) {
        if (temporary) {
            wattrset(main_win, COLOR_PAIR(12)|themes[theme].ui_st_noti_attr);
        } else {
            wattrset(main_win, COLOR_PAIR(12)|A_NORMAL);
        }
    } else if (temporary) {
        wattron(main_win, A_BOLD);
    }
    mvwprintw(main_win, status_row, status_col, "%s", status);
    if (color_code)
        wattrset(main_win, COLOR_PAIR(7)|A_NORMAL);
    else
        wattroff(main_win, A_BOLD);
    ui_print_status_ind();
    if (temporary)
        status_timer = STATUS_TIMER_COUNT;
}

void ui_print_clock(int now)
{
    static time_t tprev = 0;
    time_t tcur;
    char clock[32];

    tcur = time(NULL);
    if (now || tcur - tprev > 5) {
        tprev = tcur;
        util_clock(clock, sizeof(clock));
        if (color_code) {
            wattrset(main_win, COLOR_PAIR(20)|themes[theme].ui_clock_attr);
        }
        mvwprintw(main_win, TITLE_ROW, 1, "%s ", clock);
        if (color_code)
            wattrset(main_win, COLOR_PAIR(7)|A_NORMAL);
        wrefresh(main_win);
    }
}

void ui_print_new_ctrs()
{
    char alerts[32];

    snprintf(alerts, sizeof(alerts), "New:%dM,%dF", num_new_msgs, num_new_files);
    if (color_code) {
        wattrset(main_win, COLOR_PAIR(21)|themes[theme].ui_msg_cntr_attr);
    }
    mvwprintw(main_win, TITLE_ROW, COLS - strlen(alerts) - 2, " %s ", alerts);
    if (color_code)
        wattrset(main_win, COLOR_PAIR(7)|A_NORMAL);
    wrefresh(main_win);
}

void ui_clear_new_ctrs()
{
    int cmd;

    if (num_new_msgs || num_new_files) {
        cmd = ui_show_dialog("\tAre you sure you want to clear\n\tthe new message and file counters?\n \n\t[Y]es   [N]o", "yYnN");
        if (cmd == 'y' || cmd == 'Y') {
            num_new_msgs = num_new_files = 0;
            ui_print_new_ctrs();
            ui_print_status("Clearing new message and file counters", 1);
        }
    }
}

void ui_check_title_dirty()
{
    char status[MAX_TITLE_STATUS_SIZE];

    if (!title_dirty) {
        ui_print_clock(0);
        return;
    }
    switch (title_dirty) {
    case STATUS_REFRESH:
        ui_print_title(NULL);
        break;
    case TITLE_TNC_DETACHED:
        snprintf(status, sizeof(status), "[Detached from TNC]");
        ui_print_title(status);
        break;
    case TITLE_TNC_ATTACHED:
        pthread_mutex_lock(&mutex_tnc_set);
        snprintf(status, sizeof(status), "[Attached TNC %d: %.20s][L:%c E:%c]",
                     g_cur_tnc + 1, g_tnc_settings[g_cur_tnc].name,
                                    g_tnc_settings[g_cur_tnc].listen[0],
                                    g_tnc_settings[g_cur_tnc].en_pingack[0]);
        pthread_mutex_unlock(&mutex_tnc_set);
        ui_print_title(status);
        break;
    }
    ui_set_title_dirty(0);
}

void ui_check_status_dirty()
{
    char cmd;

    ui_check_title_dirty();
    ui_check_prog_meter();
    ui_check_channel_busy();
    if (!status_dirty)
        return;

    switch (status_dirty) {
    case STATUS_REFRESH:
        ui_print_status(NULL, status_timer ? 1 : 0);
        break;
    case STATUS_MSG_WAIT_ACK:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Busy: message sent, waiting for ACK", 1);
        break;
    case STATUS_WAIT_RESP:
        ui_print_status("ARIM Busy: query sent, waiting for response", 1);
        break;
    case STATUS_NET_MSG_SENT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: net message sent, saving to Sent Messages...", 1);
        arim_store_msg_prev_sent();
        break;
    case STATUS_BEACON_SENT:
        ui_print_status("ARIM Idle: done sending beacon", 1);
        break;
    case STATUS_MSG_ACK_RCVD:
        ui_print_status("ARIM Idle: ACK received, saving message to Sent Messages...", 1);
        arim_store_msg_prev_sent();
        break;
    case STATUS_MSG_NAK_RCVD:
        ui_print_status("ARIM Idle: NAK received", 1);
        ui_set_status_dirty(0); /* must clear flag before opening modal dialog */
        cmd = ui_show_dialog("\tMessage send failed!\n\tDo you want to save\n\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
        if (cmd == 'y' || cmd == 'Y') {
            ui_print_status("Saving message to Outbox...", 1);
            arim_store_msg_prev_out();
        }
        break;
    case STATUS_RESP_RCVD:
        ui_print_status("ARIM Idle: received response to query", 1);
        ++num_new_msgs;
        ui_print_new_ctrs();
        break;
    case STATUS_MSG_ACK_TIMEOUT:
        ui_print_status("ARIM Idle: message ACK wait time out", 1);
        ui_set_status_dirty(0); /* must clear flag before opening modal dialog */
        cmd = ui_show_dialog("\tMessage send failed!\n\tDo you want to save\n\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
        if (cmd == 'y' || cmd == 'Y') {
            ui_print_status("Saving message to Outbox...", 1);
            arim_store_msg_prev_out();
        }
        break;
    case STATUS_RESP_TIMEOUT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: response wait time out", 1);
        break;
    case STATUS_FRAME_START:
        ui_print_status("ARIM Busy: ARIM frame starting", 1);
        break;
    case STATUS_FRAME_END:
        ui_print_status("ARIM Idle: ARIM frame received", 1);
        break;
    case STATUS_MSG_START:
        ui_print_status("ARIM Busy: message frame starting", 1);
        break;
    case STATUS_QRY_START:
        ui_print_status("ARIM Busy: query frame starting", 1);
        break;
    case STATUS_BCN_START:
        ui_print_status("ARIM Busy: beacon frame starting", 1);
        break;
    case STATUS_RESP_START:
        ui_print_status("ARIM Busy: response frame starting", 1);
        break;
    case STATUS_MSG_END:
        ui_print_status("ARIM Idle: message frame received", 1);
        break;
    case STATUS_QRY_END:
        ui_print_status("ARIM Idle: query frame received", 1);
        break;
    case STATUS_BCN_END:
        ui_print_status("ARIM Idle: beacon frame received", 1);
        break;
    case STATUS_RESP_END:
        ui_print_status("ARIM Idle: response frame received", 1);
        break;
    case STATUS_ARIM_FRAME_TO:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARIM frame receive time out", 1);
        break;
    case STATUS_RESP_SEND:
        ui_print_status("ARIM Busy: sending response to query", 1);
        break;
    case STATUS_ACKNAK_SEND:
        ui_print_status("ARIM Busy: sending ACK/NAK", 1);
        break;
    case STATUS_MSG_RCVD:
        ui_print_status("ARIM Busy: received message for this station", 1);
        ++num_new_msgs;
        ui_print_new_ctrs();
        break;
    case STATUS_NET_MSG_RCVD:
        ui_print_status("ARIM Busy: received message for the net", 1);
        ++num_new_msgs;
        ui_print_new_ctrs();
        break;
    case STATUS_QUERY_RCVD:
        ui_print_status("ARIM Busy: received query for this station", 1);
        break;
    case STATUS_MSG_REPEAT:
        ui_print_status("ARIM Busy: ACK time out, repeating message send", 1);
        break;
    case STATUS_RESP_SENT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: done sending response", 1);
        break;
    case STATUS_ACKNAK_SENT:
        ui_print_status("ARIM Idle: done sending ACK/NAK", 1);
        break;
    case STATUS_QRY_SEND_CAN:
        ui_print_status("ARIM Idle: query send canceled!", 1);
        break;
    case STATUS_RESP_SEND_CAN:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: response send canceled!", 1);
        break;
    case STATUS_ACKNAK_SEND_CAN:
        ui_print_status("ARIM Idle: ACK/NAK send canceled!", 1);
        break;
    case STATUS_BCN_SEND_CAN:
        ui_print_status("ARIM Idle: beacon send canceled!", 1);
        break;
    case STATUS_SEND_UNPROTO_CAN:
        ui_print_status("ARIM Idle: unproto send canceled!", 1);
        break;
    case STATUS_MSG_SEND_CAN:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: message send canceled!", 1);
        ui_set_status_dirty(0); /* must clear flag before opening modal dialog */
        cmd = ui_show_dialog("\tMessage send canceled!\n\tDo you want to save\n\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
        if (cmd == 'y' || cmd == 'Y') {
            ui_print_status("Saving message to Outbox...", 1);
            arim_store_msg_prev_out();
        }
        break;
    case STATUS_RESP_WAIT_CAN:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: wait for response canceled!", 1);
        break;
    case STATUS_FRAME_WAIT_CAN:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: wait for ARIM frame canceled!", 1);
        break;
    case STATUS_PING_SENT:
        ui_print_status("ARIM Busy: ping sent, waiting for ACK", 1);
        break;
    case STATUS_PING_ACK_RCVD:
        ui_print_status("ARIM Idle: ping ACK received", 1);
        break;
    case STATUS_PING_TNC_BUSY:
        ui_print_status("ARIM Idle: cannot send ping; TNC is busy", 1);
        break;
    case STATUS_PING_RCVD:
        ui_print_status("ARIM Busy: received ping", 1);
        break;
    case STATUS_PING_SEND_CAN:
        ui_print_status("ARIM Idle: ping repeats canceled!", 1);
        break;
    case STATUS_PING_MSG_SEND:
        ui_print_status("ARIM Busy: ping ACK quality >= threshold, sending message", 1);
        sleep(2); /* don't rush the ardopc TNC */
        arim_send_msg_pp();
        break;
    case STATUS_PING_ACK_TIMEOUT:
        ui_print_status("ARIM Idle: ping ACK wait time out", 1);
        break;
    case STATUS_PING_MSG_ACK_TO:
        ui_print_status("ARIM Idle: ping ACK timeout, message send canceled", 1);
        ui_set_status_dirty(0); /* must clear flag before opening modal dialog */
        cmd = ui_show_dialog("\tMessage send canceled!\n\tDo you want to save\n\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
        if (cmd == 'y' || cmd == 'Y') {
            ui_print_status("Saving message to Outbox...", 1);
            arim_store_msg_prev_out();
        }
        break;
    case STATUS_PING_MSG_ACK_BAD:
        ui_print_status("ARIM Idle: ping ACK quality < threshold, message send canceled", 1);
        ui_set_status_dirty(0); /* must clear flag before opening modal dialog */
        cmd = ui_show_dialog("\tMessage send canceled!\n\tDo you want to save\n\tthe message to your Outbox?\n \n\t[Y]es   [N]o", "yYnN");
        if (cmd == 'y' || cmd == 'Y') {
            ui_print_status("Saving message to Outbox...", 1);
            arim_store_msg_prev_out();
        }
        break;
    case STATUS_PING_QRY_SEND:
        ui_print_status("ARIM Busy: ping ACK quality >= threshold, sending query", 1);
        sleep(2); /* don't rush the ardopc TNC */
        arim_send_query_pp();
        break;
    case STATUS_PING_QRY_ACK_TO:
        ui_print_status("ARIM Idle: ping ACK timeout, query send canceled", 1);
        break;
    case STATUS_PING_QRY_ACK_BAD:
        ui_print_status("ARIM Idle: ping ACK quality < threshold, query send canceled", 1);
        break;
    case STATUS_ARQ_CONN_REQ:
        ui_print_status("ARIM Busy: received ARQ connection request", 1);
        break;
    case STATUS_ARQ_CONN_CAN:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ connection canceled!", 1);
        break;
    case STATUS_ARQ_CONNECTED:
        ui_print_status("ARIM Busy: ARQ connection started", 1);
        break;
    case STATUS_ARQ_DISCONNECTED:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ connection ended", 1);
        break;
    case STATUS_ARQ_CONN_REQ_SENT:
        ui_print_status("ARIM Busy: Sending ARQ connection request", 1);
        break;
    case STATUS_ARQ_CONN_REQ_FAIL:
        ui_print_status("ARIM Idle: ARQ connection request failed", 1);
        break;
    case STATUS_ARQ_CONN_REQ_REPEAT:
        ui_print_status("ARIM Busy: Repeating connection request", 1);
        arim_arq_on_conn_req_repeat();
        break;
    case STATUS_ARQ_CONN_PP_SEND:
        ui_print_status("ARIM Busy: ping ACK quality >= threshold, connecting...", 1);
        sleep(2); /* don't rush the ardopc TNC */
        arim_arq_send_conn_req_pp();
        break;
    case STATUS_ARQ_CONN_PP_ACK_TO:
        ui_print_status("ARIM Idle: ping ACK timeout, connection request canceled", 1);
        break;
    case STATUS_ARQ_CONN_PP_ACK_BAD:
        ui_print_status("ARIM Idle: ping ACK quality < threshold, connection request canceled", 1);
        break;
    case STATUS_ARQ_CONN_TIMEOUT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ connection timeout", 1);
        break;
    case STATUS_ARQ_FILE_RCV_WAIT:
        ui_print_status("ARIM Busy: ARQ file download requested", 1);
        break;
    case STATUS_ARQ_FILE_RCV:
        ui_print_status("ARIM Busy: ARQ file download in progress", 1);
        break;
    case STATUS_ARQ_FILE_RCV_DONE:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ file download complete", 1);
        ++num_new_files;
        ui_print_new_ctrs();
        break;
    case STATUS_ARQ_FILE_RCV_ERROR:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ file download failed", 1);
        break;
    case STATUS_ARQ_FILE_RCV_TIMEOUT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ file download timeout", 1);
        break;
    case STATUS_ARQ_FILE_SEND:
        ui_print_status("ARIM Busy: ARQ file upload in progress", 1);
        break;
    case STATUS_ARQ_FILE_SEND_DONE:
        ui_print_status("ARIM Idle: ARQ file upload complete", 1);
        break;
    case STATUS_ARQ_FILE_SEND_ERROR:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ file upload failed", 1);
        break;
    case STATUS_ARQ_FILE_SEND_ACK:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Busy: ARQ file upload acknowleged", 1);
        break;
    case STATUS_ARQ_FILE_SEND_TIMEOUT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ file upload timeout", 1);
        break;
    case STATUS_ARQ_MSG_RCV:
        ui_print_status("ARIM Busy: ARQ message download in progress", 1);
        break;
    case STATUS_ARQ_MSG_RCV_DONE:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ message download complete", 1);
        ++num_new_msgs;
        ui_print_new_ctrs();
        break;
    case STATUS_ARQ_MSG_RCV_ERROR:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ message download failed", 1);
        break;
    case STATUS_ARQ_MSG_RCV_TIMEOUT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ message download timeout", 1);
        break;
    case STATUS_ARQ_MSG_SEND:
        ui_print_status("ARIM Busy: ARQ message upload in progress", 1);
        break;
    case STATUS_ARQ_MSG_SEND_DONE:
        ui_print_status("ARIM Idle: ARQ message upload complete", 1);
        break;
    case STATUS_ARQ_MSG_SEND_ERROR:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ message upload failed", 1);
        break;
    case STATUS_ARQ_MSG_SEND_ACK:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Busy: ARQ message upload acknowleged", 1);
        arim_arq_msg_on_send_next();
        break;
    case STATUS_ARQ_MSG_SEND_TIMEOUT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ message upload timeout", 1);
        break;
    case STATUS_ARQ_AUTH_BUSY:
        ui_print_status("ARIM Busy: ARQ session authentication in progress", 1);
        break;
    case STATUS_ARQ_AUTH_ERROR:
        ui_print_status("ARIM Idle: ARQ session authentication failed", 1);
        break;
    case STATUS_ARQ_EAUTH_REMOTE:
        ui_print_status("ARIM Idle: ARQ session authentication failed", 1);
        ui_set_status_dirty(0); /* must clear flag before opening modal dialog */
        cmd = ui_show_dialog("\tThis station cannot authenticate\n\tthe remote station!\n"
                             "\tDo you want to disconnect now?\n \n\t[Y]es   [N]o", "yYnN");
        if (cmd == 'y' || cmd == 'Y') {
            arim_arq_send_disconn_req();
            ui_print_status("Disconnecting...", 1);
        }
        break;
    case STATUS_ARQ_EAUTH_LOCAL:
        ui_print_status("ARIM Idle: ARQ session authentication failed", 1);
        ui_set_status_dirty(0); /* must clear flag before opening modal dialog */
        cmd = ui_show_dialog("\tRemote station cannot authenticate\n\tthis station!\n"
                             "\tDo you want to disconnect now?\n \n\t[Y]es   [N]o", "yYnN");
        if (cmd == 'y' || cmd == 'Y') {
            arim_arq_send_disconn_req();
            ui_print_status("Disconnecting...", 1);
        }
        break;
    case STATUS_ARQ_AUTH_OK:
        ui_print_status("ARIM Idle: ARQ session authenticated", 1);
        break;
    case STATUS_ARQ_RUN_CACHED_CMD:
        ui_print_status("ARIM Busy: ARQ session authenticated", 1);
        arim_arq_run_cached_cmd();
        break;
    case STATUS_ARQ_FLIST_RCV_WAIT:
        ui_print_status("ARIM Busy: ARQ remote file listing requested", 1);
        break;
    case STATUS_ARQ_FLIST_RCV:
        ui_print_status("ARIM Busy: ARQ remote file listing in progress", 1);
        break;
    case STATUS_ARQ_FLIST_RCV_DONE:
        ui_status_xfer_end(); /* end progress meter */
        ui_set_status_dirty(0); /* must clear flag before opening remote file view */
        arim_arq_files_on_flget_done();
        break;
    case STATUS_ARQ_FLIST_RCV_ERROR:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ remote file listing failed", 1);
        break;
    case STATUS_ARQ_FLIST_RCV_TIMEOUT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ remote file listing download timeout", 1);
        break;
    case STATUS_ARQ_FLIST_SEND:
        ui_print_status("ARIM Busy: ARQ remote file listing upload in progress", 1);
        break;
    case STATUS_ARQ_FLIST_SEND_DONE:
        ui_print_status("ARIM Idle: ARQ remote file listing upload complete", 1);
        break;
    case STATUS_ARQ_FLIST_SEND_ERROR:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ remote file listing upload failed", 1);
        break;
    case STATUS_ARQ_FLIST_SEND_ACK:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Busy: ARQ remote file listing upload acknowleged", 1);
        break;
    case STATUS_ARQ_FLIST_SEND_TIMEOUT:
        ui_status_xfer_end(); /* end progress meter */
        ui_print_status("ARIM Idle: ARQ remote file listing upload timeout", 1);
        break;
    }
    ui_set_status_dirty(0);
}

void ui_print_title(const char *new_status)
{
    static char status[MAX_TITLE_STATUS_SIZE];
    static int once = 0;
    char title[MAX_TITLE_SIZE];
    int center, startx;

    if (!once) {
        snprintf(status, sizeof(status), "[Detached from TNC]");
        once = 1;
    }
    if (new_status) {
        snprintf(status, sizeof(status), "%s", new_status);
    }
    snprintf(title, sizeof(title), "ARIM v%s %s", ARIM_VERSION, status);
    center = COLS / 2;
    startx = center - (strlen(title) / 2);
    if (startx < 0) {
        startx = 0;
        title[COLS] = '\0';
    }
    wmove(main_win, TITLE_ROW, 0);
    wclrtoeol(main_win);
    if (color_code)
        wattrset(main_win, COLOR_PAIR(23)|themes[theme].ui_title_attr);
    mvwprintw(main_win, TITLE_ROW, startx, "%s", title);
    if (color_code)
        wattrset(main_win, COLOR_PAIR(7)|A_NORMAL);
    wrefresh(main_win);
    ui_print_clock(1);
    ui_print_new_ctrs();
}

void ui_apply_theme()
{
    init_pair(1, themes[theme].tm_err_color, themes[theme].ui_bg_color);
    init_pair(2, themes[theme].tm_msg_color, themes[theme].ui_bg_color);
    init_pair(3, themes[theme].tm_qry_color, themes[theme].ui_bg_color);
    init_pair(4, themes[theme].tm_ping_color, themes[theme].ui_bg_color);
    init_pair(5, themes[theme].tm_bcn_color, themes[theme].ui_bg_color);
    init_pair(6, themes[theme].tm_net_color, themes[theme].ui_bg_color);
    init_pair(7, themes[theme].ui_def_color, themes[theme].ui_bg_color);
    init_pair(8, themes[theme].tm_id_color, themes[theme].ui_bg_color);
    init_pair(9, themes[theme].tm_arq_color, themes[theme].ui_bg_color);
    init_pair(10, themes[theme].ui_st_ind_color, themes[theme].ui_bg_color);
    init_pair(11, themes[theme].ui_dlg_color, themes[theme].ui_dlg_bg_color);
    init_pair(12, themes[theme].ui_st_noti_color, themes[theme].ui_bg_color);
    init_pair(13, themes[theme].tc_cmd_color, themes[theme].ui_bg_color);
    init_pair(14, themes[theme].tc_ptt_t_color, themes[theme].ui_bg_color);
    init_pair(15, themes[theme].tc_ptt_f_color, themes[theme].ui_bg_color);
    init_pair(16, themes[theme].tc_buf_color, themes[theme].ui_bg_color);
    init_pair(17, themes[theme].tc_ping_color, themes[theme].ui_bg_color);
    init_pair(18, themes[theme].tc_busy_color, themes[theme].ui_bg_color);
    init_pair(19, themes[theme].tc_newst_color, themes[theme].ui_bg_color);
    init_pair(20, themes[theme].ui_clock_color, themes[theme].ui_bg_color);
    init_pair(21, themes[theme].ui_msg_cntr_color, themes[theme].ui_bg_color);
    init_pair(22, themes[theme].ui_ch_busy_color, themes[theme].ui_bg_color);
    init_pair(23, themes[theme].ui_title_color, themes[theme].ui_bg_color);
}

int ui_get_theme(void)
{
    int temp;

    ui_themes_load_themes();
    temp = ui_themes_validate_theme(g_ui_settings.theme);
    /* default to "DARK" theme if theme name not valid */
    return temp == -1 ? 0 : temp;
}

int ui_init_color()
{
    if (!has_colors())
        return 0;
    if (start_color() != OK)
        return 0;
    if (COLORS < 8 || COLOR_PAIRS < 32)
        return 0;
    theme = ui_get_theme();
    return 1;
}

void ui_end()
{
    endwin();
}

int ui_init()
{
    static int once = 0;
    int temp;

    if (!once) {
        /* one-time initialization of screen and key parameters */
        once = 1;
        main_win = initscr();
        if (!strncasecmp(g_ui_settings.last_time_heard, "ELAPSED", 7))
            last_time_heard = LT_HEARD_ELAPSED;
        else
            last_time_heard = LT_HEARD_CLOCK;
        if (!strncasecmp(g_ui_settings.show_titles, "TRUE", 4))
            show_titles = 1;
        else
            show_titles = 0;
        if (!strncasecmp(g_ui_settings.mon_timestamp, "TRUE", 4))
            mon_timestamp = 1;
        else
            mon_timestamp = 0;
        if (!strncasecmp(g_ui_settings.color_code, "TRUE", 4))
            color_code = ui_init_color();
        else
            color_code = 0;
    }
    /* everything from this point on repeated if terminal resized */
    ui_apply_theme();
    if (color_code)
        bkgd(COLOR_PAIR(7));
    status_row = LINES - 2;
    status_col = 1;
    /* create calls heard list box and text window */
    if (!ui_heard_list_init(2, COLS - LIST_BOX_WIDTH - 1,
            LIST_BOX_WIDTH, LINES - 4)) {
        ui_end();
        return 0;
    }
    /* create command prompt box and text input window */
    if (!ui_cmd_prompt_init(LINES - 5, 1,
            COLS - ui_heard_list_get_width() - 3, 3)) {
        ui_end();
        return 0;
    }
    /* create TNC command box and text window */
    temp = (LINES / 3) - 1;
    if (show_titles && temp < 5)
        temp = 5;
    else if (temp < 4)
        temp = 4;
    if (!ui_cmd_win_init(prompt_box_y - temp, 1,
            COLS - ui_heard_list_get_width() - 3, temp)) {
        ui_end();
        return 0;
    }
    show_cmds = 1;
    /* intialize TNC command/response box sub-views */
    ui_recents_init();
    ui_ptable_init();
    ui_ctable_init();
    /* create traffic monitor box and text window */
    if (!ui_data_win_init(2, 1, COLS - ui_heard_list_get_width() - 3,
            LINES - tnc_cmd_box_h - prompt_box_h - 4)) {
        ui_end();
        return 0;
    }
    /* hide cursor and draw the views */
    curs_set(0);
    wrefresh(main_win);
    wrefresh(ui_list_box);
    wrefresh(tnc_data_box);
    wrefresh(tnc_cmd_box);
    ui_set_active_win(tnc_data_box);
    return 1;
}

int ui_run()
{
    int cmd, temp, quit = 0;

    cbreak();
    keypad(stdscr, TRUE);
    noecho();
    nodelay(main_win, TRUE);
    clear();
    ui_print_title(NULL);
    ui_print_status(MENU_PROMPT_STR, 0);

    while (!quit) {
        if ((status_timer && --status_timer == 0) ||
            (data_buf_scroll_timer && --data_buf_scroll_timer == 0)) {
            if (arim_is_arq_state())
                ui_print_status(ARQ_PROMPT_STR, 0);
            else
                ui_print_status(MENU_PROMPT_STR, 0);
        }
        cmd = getch();
        switch (cmd) {
        case 'q':
        case 'Q':
            cmd = ui_show_dialog("\tAre you sure\n\tyou want to quit?\n \n\t[Y]es   [N]o", "yYnN");
            if (cmd == 'y' || cmd == 'Y') {
                quit = 1;
                temp = 0;
                if (arim_get_state() == ST_ARQ_CONNECTED) {
                    arim_arq_send_disconn_req();
                    while (arim_get_state() != ST_IDLE && temp < 50) {
                        /* wait for disconnect, time out after 5 seconds */
                        ui_print_cmd_in();
                        if (!data_buf_scroll_timer)
                            ui_print_data_in();
                        ui_check_status_dirty();
                        ++temp;
                        usleep(100000);
                    }
                }
                ui_print_status("Shutting down...", 0);
            }
            break;
        case 27:
            ui_on_cancel();
            break;
        case 't':
        case 'T':
            if (last_time_heard == LT_HEARD_ELAPSED) {
                last_time_heard = LT_HEARD_CLOCK;
                ui_print_status("Showing clock time in Heard List and Ping History, press 't' to toggle", 1);
            } else {
                last_time_heard = LT_HEARD_ELAPSED;
                ui_print_status("Showing elapsed time in Heard List and Ping History, press 't' to toggle", 1);
            }
            ui_refresh_heard_list();
            if (show_ptable)
                ui_refresh_ptable();
            break;
        case 'r':
        case 'R':
            if (show_ptable || show_ctable || show_ftable)
                break;
            if (show_recents) {
                show_recents = 0;
                ui_print_status("Showing TNC cmds, press 'r' to toggle", 1);
                break;
            }
            if (!arim_is_arq_state()) {
                if (!show_recents) {
                    show_recents = 1;
                    ui_print_status("Showing Recents, <SP> 'rr n' read, 'u' or 'd' to scroll, 'r' to toggle", 1);
                }
            } else {
                ui_print_status("Recent Messages view not available in ARQ session", 1);
            }
            break;
        case 'p':
        case 'P':
            if (show_recents || show_ctable || show_ftable)
                break;
            if (show_ptable) {
                show_ptable = 0;
                ui_print_status("Showing TNC cmds, press 'p' to toggle", 1);
                break;
            }
            if (!arim_is_arq_state()) {
                if (!show_ptable) {
                    show_ptable = 1;
                    ui_print_status("Showing Pings, <SP> 'u' or 'd' to scroll, 'p' to toggle", 1);
                }
            } else {
                ui_print_status("Ping History view not available in ARQ session", 1);
            }
            break;
        case 'c':
        case 'C':
            if (show_recents || show_ptable || show_ftable)
                break;
            if (show_ctable) {
                show_ctable = 0;
                ui_print_status("Showing TNC cmds, press 'c' to toggle", 1);
                break;
            }
            if (!arim_is_arq_state()) {
                if (!show_ctable) {
                    show_ctable = 1;
                    ui_print_status("Showing Connections, <SP> 'u' or 'd' to scroll, 'c' to toggle", 1);
                }
            } else {
                ui_print_status("Connection History view not available in ARQ session", 1);
            }
            break;
        case 'l':
        case 'L':
            if (show_recents || show_ptable || show_ctable)
                break;
            if (show_ftable) {
                show_ftable = 0;
                ui_print_status("Showing TNC cmds, press 'l' to toggle", 1);
                break;
            }
            if (!arim_is_arq_state()) {
                if (!show_ftable) {
                    show_ftable = 1;
                    ui_print_status("Showing ARQ File History, <SP> 'u' or 'd' to scroll, 'l' to toggle", 1);
                }
            } else {
                ui_print_status("ARQ File History view not available in ARQ session", 1);
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
        case '#':
            /* recall previously loaded remote list file view */
            if (arim_is_arq_state())
                ui_list_remote_files(NULL, NULL);
            break;
        case 'f':
        case 'F':
            if (g_tnc_attached) {
                if (!arim_is_arq_state()) {
                    ui_show_fec_menu();
                } else {
                    ui_print_status("Listing shared files directory", 1);
                    ui_list_shared_files();
                }
                status_timer = 1;
            } else {
                ui_print_status("FEC control menu only available when TNC attached", 1);
            }
            break;
        case 'O':
        case 'o':
            if (arim_is_arq_state()) {
                ui_print_status("Listing messages in outbox", 1);
                ui_list_msg(MBOX_OUTBOX_FNAME, MBOX_TYPE_OUT);
            }
            break;
        case 'I':
        case 'i':
            if (arim_is_arq_state()) {
                ui_print_status("Listing messages in inbox", 1);
                ui_list_msg(MBOX_INBOX_FNAME, MBOX_TYPE_IN);
            }
            break;
        case 'S':
        case 's':
            if (arim_is_arq_state()) {
                ui_print_status("Listing sent messages", 1);
                ui_list_msg(MBOX_SENTBOX_FNAME, MBOX_TYPE_SENT);
            }
            break;
        case 'n':
        case 'N':
            ui_clear_new_ctrs();
            break;
        case 'h':
        case 'H':
            ui_show_help();
            status_timer = 1;
            break;
        case ' ':
            ui_cmd_prompt();
            break;
        case KEY_HOME:
            ui_data_win_on_key_home();
            break;
        case KEY_END:
            ui_data_win_on_key_end();
            break;
        case KEY_PPAGE:
            ui_data_win_on_key_pg_up();
            break;
        case KEY_UP:
            ui_data_win_on_key_up();
            break;
        case KEY_NPAGE:
            ui_data_win_on_key_pg_dwn();
            break;
        case KEY_DOWN:
            ui_data_win_on_key_dwn();
            break;
        case 'E':
        case 'e':
            ui_data_win_on_end_scroll();
            break;
        case 24: /* CTRL-X */
            if (arim_is_arq_state()) {
                cmd = ui_show_dialog("\tAre you sure\n\tyou want to disconnect?\n \n\t[Y]es   [N]o", "yYnN");
                if (cmd == 'y' || cmd == 'Y')
                    arim_arq_send_disconn_req();
            }
            break;
        default:
            ui_print_cmd_in();
            ui_print_recents();
            ui_print_ptable();
            ui_print_ctable();
            ui_print_ftable();
            if (!data_buf_scroll_timer)
                ui_print_data_in();
            ui_print_heard_list();
            ui_check_status_dirty();
            box(prompt_box, 0, 0);
            wrefresh(prompt_box);
            break;
        }
        if (g_new_install) {
            g_new_install = 0;
            ui_show_dialog("\tThis is a new installation!\n"
                           "\tYou should edit the " DEFAULT_INI_FNAME " file\n"
                           "\tto set your call sign and configure ARIM.\n \n\t[O]k", "oO \n");
        }
        if (g_win_changed) {
            /* terminal size changed, prepare to redraw ui */
            g_win_changed = 0;
            win_change_timer = WIN_CHANGE_TIMER_COUNT;
            show_recents = show_ptable = show_ctable = show_ftable = 0;
        } else if (win_change_timer && --win_change_timer == 0) {
                /* wipe screen and redraw ui */
                ui_end();
                clear();
                refresh();
                ui_init();
                ui_refresh_heard_list();
                ui_set_title_dirty(TITLE_REFRESH);
                status_timer = 1;
        }
        usleep(100000);
    }
    return 0;
}

