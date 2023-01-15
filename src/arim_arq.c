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
#include <ctype.h>
#include "main.h"
#include "arim_proto.h"
#include "cmdproc.h"
#include "ui.h"
#include "util.h"
#include "log.h"
#include "arim_arq.h"
#include "arim_arq_files.h"
#include "arim_arq_msg.h"
#include "arim_arq_auth.h"
#include "ini.h"
#include "datathread.h"
#include "bufq.h"
#include "ardop_data.h"
#include "ui_recents.h"
#include "ui_ping_hist.h"
#include "ui_conn_hist.h"
#include "ui_file_hist.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "ui_tnc_cmd_win.h"
#include "tnc_attach.h"

#define ONE_SECOND_TIMER    5 /* 200 msec intervals */

static int arq_rpts, arq_cmd_size, is_outbound, arq_session_bw_any;
static char cached_cmd[MAX_CMD_SIZE];
static char cached_arq_bw[TNC_ARQ_BW_SIZE];
static char arq_session_bw[TNC_ARQ_BW_SIZE];

const char *arq_bw_next_v1[] = {
    "200MAX,2000MAX",
    "500MAX,200MAX",
    "1000MAX,500MAX",
    "2000MAX,1000MAX",
    0,
};

const char *arq_bw_next_v2[] = {
    "200,2500",
    "500,200",
    "2500,500",
    0,
};

int arim_arq_send_conn_req(int repeats, const char *to_call, const char *arqbw)
{
    char mycall[TNC_MYCALL_SIZE], tcall[TNC_MYCALL_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];
    size_t i, len;

    if (!arim_is_idle() || !arim_tnc_is_idle())
        return 0;
    /* force call to uppercase */
    len = strlen(to_call);
    for (i = 0; i < len; i++)
        tcall[i] = toupper(to_call[i]);
    tcall[i] = '\0';
    /* cache remote call and repeat count */
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(g_tnc_settings[g_cur_tnc].arq_remote_call,
        sizeof(g_tnc_settings[g_cur_tnc].arq_remote_call), "%s", tcall);
    pthread_mutex_unlock(&mutex_tnc_set);
    if (repeats)
        arq_rpts = repeats;
    else
        arq_rpts = 10;
    /* cache arq session bandwidth info */
    arim_copy_arq_bw(cached_arq_bw, sizeof(cached_arq_bw));
    arim_copy_arq_bw(arq_session_bw, sizeof(arq_session_bw));
    arq_session_bw_any = 0;
    if (arqbw && !strncasecmp(arqbw, "any", 3)) {
        /* all possible arq bw will be tried, start with default */
        arq_session_bw_any = 1;
    } else if (arqbw) {
        snprintf(arq_session_bw, sizeof(arq_session_bw), "%s", arqbw);
    }
    /* are pilot pings needed first? */
    if (atoi(g_arim_settings.pilot_ping)) {
        snprintf(prev_to_call, sizeof(prev_to_call), "%s", to_call);
        arim_on_event(EV_ARQ_CONNECT_PP, atoi(g_arim_settings.pilot_ping));
        return 1;
    }
    /* print trace to Traffic Monitor view */
    arim_copy_mycall(mycall, sizeof(mycall));
    snprintf(buffer, sizeof(buffer), "<< [@] %s>%s (Connecting... ARQBW=%s)", mycall, tcall, arq_session_bw);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    is_outbound = 1; /* set outbound connection flag */
    /* change state */
    arim_on_event(EV_ARQ_CONNECT, 0);
    snprintf(buffer, sizeof(buffer), "ARQBW %s", arq_session_bw);
    bufq_queue_cmd_out(buffer);
    sleep(1); /* give TNC time to process arqbw change command */
    snprintf(buffer, sizeof(buffer), "ARQCALL %s %d", tcall, arq_rpts);
    bufq_queue_cmd_out(buffer);
    return 1;
}

int arim_arq_send_conn_req_pp()
{
    char mycall[TNC_MYCALL_SIZE], remote_call[TNC_MYCALL_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* print trace to Traffic Monitor view */
    arim_copy_mycall(mycall, sizeof(mycall));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(buffer, sizeof(buffer), "<< [@] %s>%s (Connecting... ARQBW=%s)", mycall, remote_call, arq_session_bw);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    is_outbound = 1; /* set outbound connection flag */
    /* change state */
    arim_on_event(EV_ARQ_CONNECT, 0);
    snprintf(buffer, sizeof(buffer), "ARQBW %s", arq_session_bw);
    bufq_queue_cmd_out(buffer);
    snprintf(buffer, sizeof(buffer), "ARQCALL %s %d", remote_call, arq_rpts);
    bufq_queue_cmd_out(buffer);
    return 1;
}

void arim_arq_restore_arqbw()
{
    char buffer[MAX_CMD_SIZE];

    if (is_outbound) {
        snprintf(buffer, sizeof(buffer), "ARQBW %s", cached_arq_bw);
        bufq_queue_cmd_out(buffer);
    }
    arq_session_bw_any = 0;
}

int arim_arq_on_target()
{
    char buffer[MAX_LOG_LINE_SIZE], target_call[TNC_MYCALL_SIZE];

    /* we are the target of an incoming ARQ connect request so
       print to monitor view and traffic log */
    arim_copy_target_call(target_call, sizeof(target_call));
    /* populate remote call with placeholder value for display
       purposes until CONNECTED async response is received */
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(g_tnc_settings[g_cur_tnc].arq_remote_call,
        sizeof(g_tnc_settings[g_cur_tnc].arq_remote_call), "%s", "?????");
    pthread_mutex_unlock(&mutex_tnc_set);
    snprintf(buffer, sizeof(buffer), ">> [@] %s>%s (Connect request)",
                g_tnc_settings[g_cur_tnc].arq_remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    is_outbound = 0; /* reset outbound connection flag */
    return 1;
}

int arim_arq_on_connected()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char arq_bw_hz[TNC_ARQ_BW_SIZE], gridsq[TNC_GRIDSQ_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    /* check remote call against access control call sign lists */
    if (!is_outbound && !ini_check_ac_calls(remote_call)) {
        snprintf(buffer, sizeof(buffer),
                    ">> [X] %s>%s (Access denied)", remote_call, target_call);
        bufq_queue_traffic_log(buffer);
        bufq_queue_data_in(buffer);
        /* disconnect */
        arim_arq_send_disconn_req();
        return 0;
    }
    /* we are connected to a remote station now so
       print to monitor view and traffic log */
    snprintf(buffer, sizeof(buffer),
                ">> [@] %s>%s (Connected)", remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    /* update calls heard list */
    snprintf(buffer, sizeof(buffer), "9[@] %-10s ", remote_call);
    bufq_queue_heard(buffer);
    /* update connection history */
    if (is_outbound)
        arim_copy_gridsq(gridsq, sizeof(gridsq));
    else
        arim_copy_remote_gridsq(gridsq, sizeof(gridsq));
    arim_copy_arq_bw_hz(arq_bw_hz, sizeof(arq_bw_hz));
    snprintf(buffer, sizeof(buffer), "C%c%-12s%-8s%s",
             is_outbound ? 'O' : 'I', remote_call, gridsq, arq_bw_hz);
    bufq_queue_ctable(buffer);
    /* close recents, ping or connection history view if open */
    show_recents = show_ptable = show_ctable = show_ftable = 0;
    ardop_data_reset_num_bytes(); /* reset ARQ data transfer byte counters */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset sesson authenticated status */
    arim_set_channel_not_busy(); /* force TNC not busy status */
    return 1;
}

int arim_arq_send_disconn_req()
{
    char buffer[MAX_LOG_LINE_SIZE];
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];

    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(buffer, sizeof(buffer),
                "<< [@] %s<%s (Disconnect request)", remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    bufq_queue_cmd_out("DISCONNECT");
    return 1;
}

int arim_arq_on_disconnected()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char arq_bw_hz[TNC_ARQ_BW_SIZE], gridsq[TNC_GRIDSQ_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* we are disconnected from the remote station now so
       print to monitor view and traffic log */
    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(buffer, sizeof(buffer),
                ">> [@] %s>%s (Disconnected)",
                remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    /* show in heard list if valid call sign */
    if (ini_validate_mycall(remote_call)) {
        snprintf(buffer, sizeof(buffer), "9[@] %-10s ", remote_call);
        bufq_queue_heard(buffer);
    }
    /* update connection history */
    arim_copy_gridsq(arq_bw_hz, sizeof(arq_bw_hz));
    arim_copy_arq_bw_hz(arq_bw_hz, sizeof(arq_bw_hz));
    snprintf(buffer, sizeof(buffer), "D%c%-12s%-8s%s",
             is_outbound ? 'O' : 'I', remote_call, gridsq, arq_bw_hz);
    bufq_queue_ctable(buffer);
    arim_arq_restore_arqbw(); /* restore default arq bw */
    is_outbound = 0; /* reset outbound connection flag */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset session authenticated status */
    datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
    return 1;
}

int arim_arq_on_conn_timeout()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char arq_bw_hz[TNC_ARQ_BW_SIZE], gridsq[TNC_GRIDSQ_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* connection to remote station has timed out,
       print to monitor view and traffic log */
    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(buffer, sizeof(buffer),
                ">> [@] %s>%s (Link timeout, disconnected)",
                remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    /* update connection history */
    arim_copy_arq_bw_hz(arq_bw_hz, sizeof(arq_bw_hz));
    snprintf(buffer, sizeof(buffer), "D%c%-12s%-8s%s",
             is_outbound ? 'O' : 'I', remote_call, gridsq, arq_bw_hz);
    bufq_queue_ctable(buffer);
    arim_arq_restore_arqbw(); /* restore default arq bw */
    is_outbound = 0; /* reset outbound connection flag */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset sesson authenticated status */
    datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
    return 1;
}

int arim_arq_on_conn_fail()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* connection to remote station has failed,
       print to monitor view and traffic log */
    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(buffer, sizeof(buffer),
                ">> [@] %s>%s (Connection attempt failed)",
                remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    arim_arq_restore_arqbw(); /* restore default arq bw */
    is_outbound = 0; /* reset outbound connection flag */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset sesson authenticated status */
    datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
    return 1;
}

int arim_arq_on_conn_rej_busy()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* connection to remote station has failed,
       print to monitor view and traffic log */
    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    if (!strlen(remote_call))
        snprintf(remote_call, sizeof(remote_call), "?????");
    snprintf(buffer, sizeof(buffer),
                ">> [@] %s>%s (Connection attempt failed; TNC is busy)",
                remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    arim_arq_restore_arqbw(); /* restore default arq bw */
    is_outbound = 0; /* reset outbound connection flag */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset sesson authenticated status */
    return 1;
}

int arim_arq_bw_downshift()
{
    const char *p, **bw;
    size_t len, i = 0;
    int result;

    /* get the current arq bw */
    arim_copy_arq_bw(arq_session_bw, sizeof(arq_session_bw));
    len = strlen(arq_session_bw);
    if (g_tnc_version.major <= 1)
        bw = arq_bw_next_v1;
    else
        bw = arq_bw_next_v2;
    p = bw[0];
    while (p) {
        result = strncasecmp(bw[i], arq_session_bw, len);
        if (!result && *(p + len) == ',') {
            p += (len + 1);
            snprintf(arq_session_bw, sizeof(arq_session_bw), "%s", p);
            /* if same as cached, we are done */
            if (!strcasecmp(arq_session_bw, cached_arq_bw))
                return 0;
            else
                return 1;
        }
        p = bw[++i];
    }
    return 0;
}

int arim_arq_on_conn_req_repeat()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* try to connect again with a different ARQBW specified */
    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    /* check next ARQBW option */
    if (arim_arq_bw_downshift()) {
        snprintf(buffer, sizeof(buffer), "<< [@] %s>%s (Connecting... ARQBW=%s)", target_call, remote_call, arq_session_bw);
        bufq_queue_traffic_log(buffer);
        bufq_queue_data_in(buffer);
        /* change state */
        arim_on_event(EV_ARQ_CONNECT, 0);
        snprintf(buffer, sizeof(buffer), "ARQBW %s", arq_session_bw);
        bufq_queue_cmd_out(buffer);
        snprintf(buffer, sizeof(buffer), "ARQCALL %s %d", remote_call, arq_rpts);
        bufq_queue_cmd_out(buffer);
        return 1; /* done, return immediately */
    }
    /* repeat attempts are exhausted, clean up */
    arim_arq_restore_arqbw(); /* restore default arq bw */
    is_outbound = 0; /* reset outbound connection flag */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset sesson authenticated status */
    return 0;
}
int arim_arq_on_conn_rej_bw()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* connection to remote station has failed,
       print to monitor view and traffic log */
    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    if (!strlen(remote_call))
        snprintf(remote_call, sizeof(remote_call), "?????");
    /* if bw 'any', do nothing here, a repeat connection attempt will be scheduled */
    if (is_outbound && arq_session_bw_any)
        return 1;
    /* failed, no repeat attempt required */
    snprintf(buffer, sizeof(buffer),
                ">> [@] %s>%s (Connection attempt failed; incompatible bandwidths)",
                remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    arim_arq_restore_arqbw(); /* restore default arq bw */
    is_outbound = 0; /* reset outbound connection flag */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset sesson authenticated status */
    return 0;
}

int arim_arq_on_conn_cancel()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char arq_bw_hz[TNC_ARQ_BW_SIZE], gridsq[TNC_GRIDSQ_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* operator has canceled the connection by pressing ESC key,
       print to monitor view and traffic log */
    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(buffer, sizeof(buffer),
                ">> [X] %s>%s (Connection canceled by operator)",
                remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    /* update connection history */
    arim_copy_arq_bw_hz(arq_bw_hz, sizeof(arq_bw_hz));
    snprintf(buffer, sizeof(buffer), "D%c%-12s%-8s%s",
             is_outbound ? 'O' : 'I', remote_call, gridsq, arq_bw_hz);
    bufq_queue_ctable(buffer);
    arim_arq_restore_arqbw(); /* restore default arq bw */
    is_outbound = 0; /* reset outbound connection flag */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset sesson authenticated status */
    ui_status_xfer_end(); /* hide xfer progress meter */
    datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
    return 1;
}

int arim_arq_on_conn_closed()
{
    char remote_call[TNC_MYCALL_SIZE], target_call[TNC_MYCALL_SIZE];
    char arq_bw_hz[TNC_ARQ_BW_SIZE], gridsq[TNC_GRIDSQ_SIZE];
    char buffer[MAX_LOG_LINE_SIZE];

    /* TNC has shut down, closing TCP connections */
    arim_copy_target_call(target_call, sizeof(target_call));
    if (!strlen(target_call))
        arim_copy_mycall(target_call, sizeof(target_call));
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(buffer, sizeof(buffer),
                ">> [X] %s>%s (Unexpected shutdown of TNC)",
                remote_call, target_call);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    /* update connection history */
    arim_copy_arq_bw_hz(arq_bw_hz, sizeof(arq_bw_hz));
    snprintf(buffer, sizeof(buffer), "D%c%-12s%-8s%s",
             is_outbound ? 'O' : 'I', remote_call, gridsq, arq_bw_hz);
    bufq_queue_ctable(buffer);
    arim_arq_restore_arqbw(); /* restore default arq bw */
    is_outbound = 0; /* reset outbound connection flag */
    arq_cmd_size = 0; /* reset ARQ command size */
    arim_arq_auth_set_status(0); /* reset sesson authenticated status */
    ui_status_xfer_end(); /* hide xfer progress meter */
    datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
    return 1;
}

size_t arim_arq_send_remote(const char *msg)
{
    char sendcr[TNC_ARQ_SENDCR_SIZE], linebuf[MAX_LOG_LINE_SIZE];

    /* check to see if CR must be sent */
    arim_copy_arq_sendcr(sendcr, sizeof(sendcr));
    if (!strncasecmp(sendcr, "TRUE", 4))
        snprintf(linebuf, sizeof(linebuf), "%s\r\n", msg);
    else
        snprintf(linebuf, sizeof(linebuf), "%s\n", msg);
    bufq_queue_data_out(linebuf);
    return strlen(linebuf);
}

size_t arim_arq_on_cmd(const char *cmd, size_t size)
{
    /* called by datathread via arim_arq_on_data() */
    static char buffer[MAX_UNCOMP_DATA_SIZE];
    static size_t cnt = 0;
    static int line_timer = 0;
    char *e, *eol, respbuf[MAX_UNCOMP_DATA_SIZE], cmdbuf[MIN_DATA_BUF_SIZE];
    char sendcr[TNC_ARQ_SENDCR_SIZE], linebuf[MAX_LOG_LINE_SIZE];
    int state, result, numch, send_cr = 0;

    state = arim_get_state();
    if (cmd && cmd[0] == '/') {
        if (size > sizeof(cmdbuf))
            return 0;
        memcpy(cmdbuf, cmd, size);
        cmdbuf[size] = '\0';
        /* check to see if CR must be sent */
        arim_copy_arq_sendcr(sendcr, sizeof(sendcr));
        if (!strncasecmp(sendcr, "TRUE", 4))
            send_cr = 1;
        /* strip EOL from command */
        eol = cmdbuf;
        while (*eol && *eol != '\n' && *eol != '\r')
            ++eol;
        if (*eol && *eol == '\r' && *(eol + 1) == '\n') {
            *eol++ = '\0';
            *eol++ = '\0';
        } else if (*eol && *eol == '\n') {
            *eol++ = '\0';
        } else {
            eol = 0;
        }
        /* print command to traffic monitor */
        numch = snprintf(linebuf, sizeof(linebuf), ">> [@] %s", cmdbuf);
        if (numch >= sizeof(linebuf))
            ui_truncate_line(linebuf, sizeof(linebuf));
        bufq_queue_traffic_log(linebuf);
        bufq_queue_data_in(linebuf);
        /* initialize response buffer for queries */
        memset(respbuf, 0, sizeof(respbuf));
        if (!strncasecmp(cmdbuf, "/FPUT ", 6)) {
            /* remote station sends a file. If in a wait state already,
               abandon that transaction and respond to the /fput to avoid
               deadlock when commands are issued by both parties simultaneously */
            switch (state) {
            case ST_ARQ_FILE_SEND_WAIT:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FLIST_SEND_WAIT:
            case ST_ARQ_FLIST_RCV_WAIT:
            case ST_ARQ_AUTH_RCV_A2_WAIT:
            case ST_ARQ_AUTH_RCV_A3_WAIT:
            case ST_ARQ_MSG_SEND_WAIT:
                arim_on_event(EV_ARQ_CANCEL_WAIT, 0);
                state = arim_get_state();
                break;
            }
            switch (state) {
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                /* /FPUT implies remote stn accepted our /A3, auth successful */
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                /* fallthrough intentional */
            case ST_ARQ_CONNECTED:
                arim_arq_files_on_fput(cmdbuf, size, eol, ARQ_SERVER_STN);
                break;
            case ST_ARQ_FILE_RCV_WAIT:
                arim_arq_files_on_fput(cmdbuf, size, eol, ARQ_CLIENT_STN);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/FGET ", 6)) {
            /* remote station requests a file. If in a wait state already,
               abandon that transaction and respond to the /FGET to avoid
               deadlock when commands are issued by both parties simultaneously */
            switch (state) {
            case ST_ARQ_FILE_SEND_WAIT:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FILE_RCV_WAIT:
            case ST_ARQ_FILE_RCV_WAIT_OK:
            case ST_ARQ_FLIST_SEND_WAIT:
            case ST_ARQ_FLIST_RCV_WAIT:
            case ST_ARQ_AUTH_RCV_A2_WAIT:
            case ST_ARQ_AUTH_RCV_A3_WAIT:
            case ST_ARQ_MSG_SEND_WAIT:
                arim_on_event(EV_ARQ_CANCEL_WAIT, 0);
                state = arim_get_state();
                break;
            }
            switch (state) {
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                /* /FGET implies remote stn accepted our /A3, auth successful */
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                /* fallthrough intentional */
            case ST_ARQ_CONNECTED:
                arim_arq_files_on_fget(cmdbuf, size, eol);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/FLPUT ", 7)) {
            /* remote station sends a file listing. If in a wait state already,
               abandon that transaction and respond to the /fput to avoid
               deadlock when commands are issued by both parties simultaneously */
            switch (state) {
            case ST_ARQ_FILE_SEND_WAIT:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FLIST_SEND_WAIT:
            case ST_ARQ_AUTH_RCV_A2_WAIT:
            case ST_ARQ_AUTH_RCV_A3_WAIT:
            case ST_ARQ_MSG_SEND_WAIT:
                arim_on_event(EV_ARQ_CANCEL_WAIT, 0);
                state = arim_get_state();
                break;
            }
            switch (state) {
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                /* /FLPUT implies remote stn accepted our /A3, auth successful */
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                /* fallthrough intentional */
            case ST_ARQ_FLIST_RCV_WAIT:
                arim_arq_files_on_flput(cmdbuf, size, eol);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/FLGET ", 6)) {
            /* remote station requests a file listing. If in a wait state already,
               abandon that transaction and respond to the /FLGET to avoid
               deadlock when commands are issued by both parties simultaneously */
            switch (state) {
            case ST_ARQ_FILE_SEND_WAIT:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FILE_RCV_WAIT:
            case ST_ARQ_FILE_RCV_WAIT_OK:
            case ST_ARQ_FLIST_SEND_WAIT:
            case ST_ARQ_FLIST_RCV_WAIT:
            case ST_ARQ_AUTH_RCV_A2_WAIT:
            case ST_ARQ_AUTH_RCV_A3_WAIT:
            case ST_ARQ_MSG_SEND_WAIT:
                arim_on_event(EV_ARQ_CANCEL_WAIT, 0);
                state = arim_get_state();
                break;
            }
            switch (state) {
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                /* /FLGET implies remote stn accepted our /A3, auth successful */
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                /* fallthrough intentional */
            case ST_ARQ_CONNECTED:
                arim_arq_files_on_flget(cmdbuf, size, eol);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/MPUT ", 6)) {
            /* remote station sends a message. If in a wait state already,
               abandon that transaction and respond to the /mput to avoid
               deadlock when commands are issued by both parties simultaneously */
            switch (state) {
            case ST_ARQ_FILE_SEND_WAIT:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FILE_RCV_WAIT:
            case ST_ARQ_FILE_RCV_WAIT_OK:
            case ST_ARQ_FLIST_SEND_WAIT:
            case ST_ARQ_FLIST_RCV_WAIT:
            case ST_ARQ_AUTH_RCV_A2_WAIT:
            case ST_ARQ_AUTH_RCV_A3_WAIT:
            case ST_ARQ_MSG_SEND_WAIT:
                arim_on_event(EV_ARQ_CANCEL_WAIT, 0);
                state = arim_get_state();
                break;
            }
            switch (state) {
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                /* /MPUT implies remote stn accepted our /A3, auth successful */
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                /* fallthrough intentional */
            case ST_ARQ_CONNECTED:
                arim_arq_msg_on_mput(cmdbuf, size, eol);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/MGET", 5)) {
            /* remote station requests messages. If in a wait state already,
               abandon that transaction and respond to the /MGET to avoid
               deadlock when commands are issued by both parties simultaneously */
            switch (state) {
            case ST_ARQ_FILE_SEND_WAIT:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FILE_RCV_WAIT:
            case ST_ARQ_FILE_RCV_WAIT_OK:
            case ST_ARQ_FLIST_SEND_WAIT:
            case ST_ARQ_FLIST_RCV_WAIT:
            case ST_ARQ_AUTH_RCV_A2_WAIT:
            case ST_ARQ_AUTH_RCV_A3_WAIT:
            case ST_ARQ_MSG_SEND_WAIT:
                arim_on_event(EV_ARQ_CANCEL_WAIT, 0);
                state = arim_get_state();
                break;
            }
            switch (state) {
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                /* /MGET implies remote stn accepted our /A3, auth successful */
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                /* fallthrough intentional */
            case ST_ARQ_CONNECTED:
                arim_arq_msg_on_mget(cmdbuf, size, eol);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/A1", 3)) {
            /* remote station sends a authentication challenge. This can only be
               in response to a command recently sent by the local station which
               references a authenticated file directory at the remote station. */
            switch (state) {
            case ST_ARQ_CONNECTED:
            case ST_ARQ_FILE_RCV_WAIT:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FLIST_RCV_WAIT:
                arim_arq_auth_on_a1(cmdbuf, size, eol);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/A2", 3)) {
            /* remote station sends a response to an /A1 authentication challenge
               recently send by the local station. This response is itself a challenge
               and it includes a nonce. */
            if (state == ST_ARQ_AUTH_RCV_A2_WAIT)
                arim_arq_auth_on_a2(cmdbuf, size, eol);
        } else if (!strncasecmp(cmdbuf, "/A3", 3)) {
            /* remote station sends a response to an /A2 authentication challenge
               recently send by the local station. */
            if (state == ST_ARQ_AUTH_RCV_A3_WAIT)
                arim_arq_auth_on_a3(cmdbuf, size, eol);
        } else if (!strncasecmp(cmdbuf, "/AUTH", 5)) {
            /* remote station requests that we send an /A1 authentication challenge */
            if (state == ST_ARQ_CONNECTED)
                arim_arq_auth_on_challenge(cmdbuf, size, eol);
        } else if (!strncasecmp(cmdbuf, "/MLIST", 6)) {
            /* remote station requests message list */
            switch (state) {
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                /* receipt of cmd implies remote stn accepted our /A3, auth successful */
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                /* fallthrough intentional */
            case ST_ARQ_CONNECTED:
                /* empty outbound data buffer before handling query or command */
                while (arim_get_buffer_cnt() > 0)
                    sleep(1);
                if (arim_arq_msg_on_mlist(cmdbuf, size, eol, respbuf, sizeof(respbuf))) {
                    /* append response to data out buffer */
                    size = strlen(respbuf);
                    if ((cnt + size) >= sizeof(buffer)) {
                        /* overflow, reset buffer and return */
                        cnt = 0;
                        return cnt;
                    }
                    strncat(&buffer[cnt], respbuf, size);
                    cnt += size;
                }
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/ERROR", 6)) {
            switch (state) {
            case ST_ARQ_FILE_RCV:
            case ST_ARQ_FILE_RCV_WAIT:
            case ST_ARQ_FILE_SEND:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FLIST_RCV:
            case ST_ARQ_FLIST_RCV_WAIT:
            case ST_ARQ_FLIST_SEND:
                arim_on_event(EV_ARQ_FILE_ERROR, 0);
                break;
            case ST_ARQ_MSG_RCV:
            case ST_ARQ_MSG_SEND:
                arim_on_event(EV_ARQ_MSG_ERROR, 0);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/EAUTH", 6)) {
            switch (state) {
            case ST_ARQ_FILE_RCV_WAIT:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FLIST_RCV_WAIT:
            case ST_ARQ_AUTH_RCV_A2_WAIT:
            case ST_ARQ_AUTH_RCV_A3_WAIT:
            case ST_ARQ_AUTH_RCV_A4_WAIT:
            case ST_ARQ_CONNECTED:
                arim_on_event(EV_ARQ_AUTH_ERROR, 0);
                break;
            }
        } else if (!strncasecmp(cmdbuf, "/OK", 3)) {
            switch (state) {
            case ST_ARQ_FILE_SEND:
            case ST_ARQ_FILE_SEND_WAIT_OK:
            case ST_ARQ_FLIST_SEND:
                arim_on_event(EV_ARQ_FILE_OK, 0);
                break;
            case ST_ARQ_MSG_SEND:
                arim_on_event(EV_ARQ_MSG_OK, 0);
                /* must call this to uncompress if -z option invoked */
                arim_arq_msg_on_ok();
                break;
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                break;
            }
        } else {
            switch (state) {
            case ST_ARQ_AUTH_RCV_A4_WAIT:
                /* receipt of cmd implies remote stn accepted our /A3, auth successful */
                arim_on_event(EV_ARQ_AUTH_OK, 0);
                /* fallthrough intentional */
            case ST_ARQ_CONNECTED:
                /* empty outbound data buffer before handling query or command */
                while (arim_get_buffer_cnt() > 0)
                    sleep(1);
                /* execute query or command, skip leading '/' character */
                result = cmdproc_query(cmdbuf + 1, respbuf, sizeof(respbuf));
                if (result == CMDPROC_OK) {
                    /* success, append response to data out buffer */
                    size = strlen(respbuf);
                    if ((cnt + size) >= sizeof(buffer)) {
                        /* overflow, reset buffer and return */
                        cnt = 0;
                        return cnt;
                    }
                    strncat(&buffer[cnt], respbuf, size);
                    cnt += size;
                } else if (result == CMDPROC_FILE_ERR) {
                    /* file access error */
                    size = strlen("/ERROR File not found");
                    if ((cnt + size) >= sizeof(buffer)) {
                        /* overflow, reset buffer and return */
                        cnt = 0;
                        return cnt;
                    }
                    strncat(&buffer[cnt], "/ERROR File not found", size);
                    cnt += size;
                } else if (result == CMDPROC_DIR_ERR) {
                    /* directory access error */
                    size = strlen("/ERROR Directory not found");
                    if ((cnt + size) >= sizeof(buffer)) {
                        /* overflow, reset buffer and return */
                        cnt = 0;
                        return cnt;
                    }
                    strncat(&buffer[cnt], "/ERROR Directory not found", size);
                    cnt += size;
                } else if (result == CMDPROC_AUTH_REQ) {
                    /* authentication required */
                    cnt = 0;
                    return cnt;
                } else if (result == CMDPROC_AUTH_ERR) {
                    /* authentication error */
                    size = strlen("/EAUTH");
                    if ((cnt + size) >= sizeof(buffer)) {
                        /* overflow, reset buffer and return */
                        cnt = 0;
                        return cnt;
                    }
                    strncat(&buffer[cnt], "/EAUTH", size);
                    cnt += size;
                }
                /* unknown queries are ignored in ARQ mode */
            }
        }
    } else if (cnt) {
        if (line_timer && --line_timer > 0)
            return cnt;
        /* send data out one line at a time */
        e = buffer;
        if (*e) {
            while (*e && *e != '\n')
                ++e;
            if (*e) {
                *e = '\0';
                ++e;
            }
            if (send_cr)
                numch = snprintf(respbuf, sizeof(respbuf), "%s\r\n", buffer);
            else
                numch = snprintf(respbuf, sizeof(respbuf), "%s\n", buffer);
            bufq_queue_data_out(respbuf);
            cnt -= (e - buffer);
            memmove(buffer, e, cnt + 1);
        }
        line_timer = ONE_SECOND_TIMER; /* delay to prevent data queue overrun */
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return cnt;
}

size_t arim_arq_on_resp(const char *resp, size_t size)
{
    static char buffer[MAX_UNCOMP_DATA_SIZE];
    static size_t cnt = 0;
    char *e, linebuf[MIN_DATA_BUF_SIZE];
    size_t i, len;
    int numch;

    if (resp) {
        /* append response to buffer */
        if ((cnt + size) >= sizeof(buffer)) {
            /* overflow, reset buffer and return */
            cnt = 0;
            return cnt;
        }
        strncat(&buffer[cnt], resp, size);
        cnt += size;
     } else if (cnt) {
        e = buffer;
        while (*e && *e != '\r' && *e != '\n')
            ++e;
        if (!*e)
            return cnt;
        if (*e == '\r' && *(e + 1) == '\n') {
            *e = '\0';
            ++e;
        }
        *e = '\0';
        ++e;
        len = strlen(buffer);
        for (i = 0; i < len; i++) {
            if (!isprint((int)buffer[i]))
                buffer[i] = ' ';
        }
        numch = snprintf(linebuf, sizeof(linebuf), ">> [@] %s", buffer);
        if (numch >= sizeof(linebuf))
            ui_truncate_line(linebuf, sizeof(linebuf));
        bufq_queue_traffic_log(linebuf);
        bufq_queue_data_in(linebuf);
        cnt -= (e - buffer);
        memmove(buffer, e, cnt + 1);
    }
    return cnt;
}

int arim_arq_on_data(char *data, size_t size)
{
    /* called by datathread */
    static char cmdbuffer[MAX_CMD_SIZE+2];
    char *s, *e, linebuf[MAX_LOG_LINE_SIZE], remote_call[TNC_MYCALL_SIZE];

    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(linebuf, sizeof(linebuf), "9[@] %-10s ", remote_call);
    bufq_queue_heard(linebuf);
    /* pass to command or response handler */
    if (!arq_cmd_size && data[0] == '/') {
        /* all commands start with an alpha character */
        if (size > 1 && !isalpha((int)data[1])){
            /* not a command (all commands start with alpha character) */
            arim_arq_on_resp(data, size);
            return 1;
        }
        /* start of command; a complete command is newline terminated */
        s = data;
        e = data + size;
        while (*s != '\n' && s < e)
            ++s;
        if (s == e) {
            /* incomplete, cache and wait for more data */
            if (size < sizeof(cmdbuffer)) {
                memcpy(cmdbuffer, data, size);
                arq_cmd_size = size;
                bufq_queue_debug_log("Data thread: incomplete ARQ command, buffering");
            } else {
                bufq_queue_debug_log("Data thread: ARQ command too large");
            }
            return 0;
        } else {
            /* complete, process the command */
            bufq_queue_debug_log("Data thread: processing ARQ command");
            arim_arq_on_cmd(data, size);
        }
    } else if (arq_cmd_size) {
        if (arq_cmd_size + size < sizeof(cmdbuffer)) {
            memcpy(cmdbuffer + arq_cmd_size, data, size);
            /* a complete command is newline terminated */
            s = cmdbuffer + arq_cmd_size;
            arq_cmd_size += size;
            e = cmdbuffer + arq_cmd_size;
            while (*s != '\n' && s < e)
                ++s;
            if (s == e) {
                bufq_queue_debug_log("Data thread: incomplete ARQ command, buffering");
                return 0;
            } else {
                /* complete, process the command */
                bufq_queue_debug_log("Data thread: ARQ command completed, processing");
                arim_arq_on_cmd(cmdbuffer, arq_cmd_size);
                arq_cmd_size = 0;
            }
        } else {
            arq_cmd_size = 0;
            bufq_queue_debug_log("Data thread: ARQ command too large");
            return 0;
        }
    } else {
        /* not a command */
        arim_arq_on_resp(data, size);
    }
    return 1;
}

void arim_arq_run_cached_cmd()
{
    char cmdbuffer[MAX_CMD_SIZE], linebuf[MAX_LOG_LINE_SIZE];
    int numch;

    snprintf(cmdbuffer, sizeof(cmdbuffer), "%s", cached_cmd);
    cmdproc_cmd(cmdbuffer);
    numch = snprintf(linebuf, sizeof(linebuf), "ARQ: Running cached command '%s'", cached_cmd);
    if (numch >= sizeof(linebuf))
        ui_truncate_line(linebuf, sizeof(linebuf));
    bufq_queue_debug_log(linebuf);
}

void arim_arq_cache_cmd(const char *cmd)
{
    char linebuf[MAX_LOG_LINE_SIZE];
    int numch;

    snprintf(cached_cmd, sizeof(cached_cmd), "%s", cmd);
    numch = snprintf(linebuf, sizeof(linebuf), "ARQ: Caching command '%s'", cached_cmd);
    if (numch >= sizeof(linebuf))
        ui_truncate_line(linebuf, sizeof(linebuf));
    bufq_queue_debug_log(linebuf);
}

