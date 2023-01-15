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
#include "main.h"
#include "ini.h"
#include "mbox.h"
#include "ui.h"
#include "ui_tnc_data_win.h"
#include "util.h"
#include "bufq.h"
#include "arim.h"
#include "arim_proto.h"
#include "arim_ping.h"
#include "arim_arq.h"
#include "arim_proto_idle.h"
#include "arim_proto_ping.h"
#include "arim_proto_msg.h"
#include "arim_proto_query.h"
#include "arim_proto_beacon.h"
#include "arim_proto_unproto.h"
#include "arim_proto_frame.h"
#include "arim_proto_arq_conn.h"
#include "arim_proto_arq_msg.h"
#include "arim_proto_arq_files.h"
#include "arim_proto_arq_auth.h"
#include "mbox.h"
#include "tnc_attach.h"
#include "datathread.h"

pthread_mutex_t mutex_arim_state = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_send_repeats = PTHREAD_MUTEX_INITIALIZER;

char msg_acknak_buffer[MAX_ACKNAK_SIZE];
char msg_buffer[MAX_UNCOMP_DATA_SIZE];

size_t msg_len;
time_t prev_time;
char prev_fecmode[TNC_FECMODE_SIZE];
char prev_to_call[TNC_MYCALL_SIZE];
char prev_msg[MAX_UNCOMP_DATA_SIZE];
int rcv_nak_cnt = 0, ack_timeout = 30, send_repeats = 0, fecmode_downshift = 0;
static int arim_state = 0;

const char *downshift_v1[] = {
    /* 4FSK family */
    "4FSK.200.50S,4FSK.200.50S",
    "4FSK.500.100S,4FSK.200.50S",
    "4FSK.500.100,4FSK.500.100S",
    "4FSK.2000.600S,4FSK.500.100",
    "4FSK.2000.600,4FSK.2000.600S",
    /* 4PSK family */
    "4PSK.200.100S,4FSK.200.50S",
    "4PSK.200.100,4PSK.200.100S",
    "4PSK.500.100,4PSK.200.100",
    "4PSK.1000.100,4PSK.500.100",
    "4PSK.2000.100,4PSK.1000.100",
    /* 8PSK family */
    "8PSK.200.100,4FSK.200.50S",
    "8PSK.500.100,8PSK.200.100",
    "8PSK.1000.100,8PSK.500.100",
    "8PSK.2000.100,8PSK.1000.100",
    /* 16QAM family */
    "16QAM.200.100,4FSK.200.50S",
    "16QAM.500.100,16QAM.200.100",
    "16QAM.1000.100,16QAM.500.100",
    "16QAM.2000.100,16QAM.1000.100",
    0,
};

const char *downshift_v2[] = {
    "4PSK.200.50,4PSK.200.50",
    "4PSK.200.100,4PSK.200.50",
    "16QAM.200.100,4PSK.200.100",
    "4FSK.500.50,16QAM.200.100",
    "4PSK.500.50,4FSK.500.50",
    "16QAMR.500.100,4PSK.500.50",
    "16QAM.500.100,16QAMR.500.100",
    "4FSK.1000.50,16QAM.500.100",
    "4PSKR.2500.50,4FSK.1000.50",
    "4PSK.2500.50,4PSKR.2500.50",
    "16QAMR.2500.100,4PSK.2500.50",
    "16QAM.2500.100,16QAMR.2500.100",
    0,
};

const char *states[] = {
    "ST_IDLE",                          /*  0 */
    "ST_SEND_MSG_BUF_WAIT",             /*  1 */
    "ST_SEND_NET_MSG_BUF_WAIT",         /*  2 */
    "ST_SEND_QRY_BUF_WAIT",             /*  3 */
    "ST_SEND_RESP_BUF_WAIT",            /*  4 */
    "ST_SEND_ACKNAK_BUF_WAIT",          /*  5 */
    "ST_SEND_BCN_BUF_WAIT",             /*  6 */
    "ST_SEND_UN_BUF_WAIT",              /*  7 */
    "ST_SEND_ACKNAK_PEND",              /*  8 */
    "ST_SEND_RESP_PEND",                /*  9 */
    "ST_RCV_ACKNAK_WAIT",               /* 10 */
    "ST_RCV_RESP_WAIT",                 /* 11 */
    "ST_RCV_FRAME_WAIT",                /* 12 */
    "ST_ARQ_PEND_WAIT",                 /* 13 */
    "ST_RCV_PING_ACK_WAIT",             /* 14 */
    "ST_SEND_PING_ACK_PEND",            /* 15 */
    "ST_RCV_MSG_PING_ACK_WAIT",         /* 16 */
    "ST_RCV_QRY_PING_ACK_WAIT",         /* 17 */
    "ST_ARQ_IN_CONNECT_WAIT",           /* 18 */
    "ST_ARQ_OUT_CONNECT_WAIT",          /* 19 */
    "ST_ARQ_OUT_CONNECT_WAIT_RPT",      /* 20 */
    "ST_RCV_ARQ_CONN_PING_ACK_WAIT",    /* 21 */
    "ST_ARQ_CONNECTED",                 /* 22 */
    "ST_ARQ_FILE_SEND_WAIT",            /* 23 */
    "ST_ARQ_FILE_SEND_WAIT_OK",         /* 24 */
    "ST_ARQ_FILE_SEND",                 /* 25 */
    "ST_ARQ_FILE_RCV_WAIT_OK",          /* 26 */
    "ST_ARQ_FILE_RCV_WAIT",             /* 27 */
    "ST_ARQ_FILE_RCV",                  /* 28 */
    "ST_ARQ_MSG_RCV",                   /* 29 */
    "ST_ARQ_MSG_SEND_WAIT",             /* 30 */
    "ST_ARQ_MSG_SEND",                  /* 31 */
    "ST_ARQ_AUTH_RCV_A2_WAIT",          /* 32 */
    "ST_ARQ_AUTH_RCV_A3_WAIT",          /* 33 */
    "ST_ARQ_AUTH_RCV_A4_WAIT",          /* 34 */
    "ST_ARQ_AUTH_SEND_A1",              /* 35 */
    "ST_ARQ_AUTH_SEND_A2",              /* 36 */
    "ST_ARQ_AUTH_SEND_A3",              /* 37 */
    "ST_ARQ_FLIST_RCV_WAIT",            /* 38 */
    "ST_ARQ_FLIST_RCV",                 /* 39 */
    "ST_ARQ_FLIST_SEND_WAIT",           /* 40 */
    "ST_ARQ_FLIST_SEND",                /* 41 */
};

const char *events[] = {
    "EV_NULL",                          /*  0 */
    "EV_PERIODIC",                      /*  1 */
    "EV_CANCEL",                        /*  2 */
    "EV_FRAME_START",                   /*  3 */
    "EV_FRAME_END",                     /*  4 */
    "EV_FRAME_TO",                      /*  5 */
    "EV_SEND_MSG",                      /*  6 */
    "EV_SEND_MSG_PP",                   /*  7 */
    "EV_SEND_NET_MSG",                  /*  8 */
    "EV_RCV_MSG",                       /*  9 */
    "EV_RCV_ACK",                       /* 10 */
    "EV_RCV_NAK",                       /* 11 */
    "EV_RCV_NET_MSG",                   /* 12 */
    "EV_SEND_QRY",                      /* 13 */
    "EV_SEND_QRY_PP",                   /* 14 */
    "EV_RCV_RESP",                      /* 15 */
    "EV_RCV_QRY",                       /* 16 */
    "EV_SEND_BCN",                      /* 17 */
    "EV_SEND_UNPROTO",                  /* 18 */
    "EV_SEND_PING",                     /* 19 */
    "EV_SEND_PING_ACK",                 /* 20 */
    "EV_RCV_PING",                      /* 21 */
    "EV_RCV_PING_ACK",                  /* 22 */
    "EV_TNC_PTT",                       /* 23 */
    "EV_TNC_NEWSTATE",                  /* 24 */
    "EV_ARQ_PENDING",                   /* 25 */
    "EV_ARQ_CAN_PENDING",               /* 26 */
    "EV_ARQ_CONNECT",                   /* 27 */
    "EV_ARQ_CONNECT_PP",                /* 28 */
    "EV_ARQ_CONNECTED",                 /* 29 */
    "EV_ARQ_DISCONNECTED",              /* 30 */
    "EV_ARQ_TARGET",                    /* 31 */
    "EV_ARQ_REJ_BUSY",                  /* 32 */
    "EV_ARQ_REJ_BW",                    /* 33 */
    "EV_ARQ_FILE_SEND",                 /* 34 */
    "EV_ARQ_FILE_SEND_CMD",             /* 35 */
    "EV_ARQ_FILE_SEND_CMD_CLIENT",      /* 36 */
    "EV_ARQ_FILE_RCV_WAIT_OK",          /* 37 */
    "EV_ARQ_FILE_RCV_WAIT",             /* 38 */
    "EV_ARQ_FILE_RCV",                  /* 39 */
    "EV_ARQ_FILE_RCV_FRAME",            /* 40 */
    "EV_ARQ_FILE_RCV_DONE",             /* 41 */
    "EV_ARQ_FILE_ERROR",                /* 42 */
    "EV_ARQ_FILE_OK",                   /* 43 */
    "EV_ARQ_MSG_RCV",                   /* 44 */
    "EV_ARQ_MSG_RCV_FRAME",             /* 45 */
    "EV_ARQ_MSG_RCV_DONE",              /* 46 */
    "EV_ARQ_MSG_ERROR",                 /* 47 */
    "EV_ARQ_MSG_OK",                    /* 48 */
    "EV_ARQ_MSG_SEND_CMD",              /* 49 */
    "EV_ARQ_MSG_SEND",                  /* 50 */
    "EV_ARQ_MSG_SEND_DONE",             /* 51 */
    "EV_ARQ_CANCEL_WAIT",               /* 52 */
    "EV_ARQ_AUTH_SEND_CMD",             /* 53 */
    "EV_ARQ_AUTH_WAIT_CMD",             /* 54 */
    "EV_ARQ_AUTH_OK",                   /* 55 */
    "EV_ARQ_AUTH_ERROR",                /* 56 */
    "EV_ARQ_FLIST_RCV_WAIT",            /* 57 */
    "EV_ARQ_FLIST_RCV",                 /* 58 */
    "EV_ARQ_FLIST_RCV_FRAME",           /* 59 */
    "EV_ARQ_FLIST_RCV_DONE",            /* 60 */
    "EV_ARQ_FLIST_SEND",                /* 61 */
    "EV_ARQ_FLIST_SEND_CMD",            /* 62 */
};

void arim_on_cancel()
{
    if (g_tnc_attached) {
        arim_on_event(EV_CANCEL, 0);
        arim_set_channel_not_busy(); /* force TNC not busy status */
        datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
    }
}

void arim_copy_mycall(char *call, size_t size)
{
    if (g_tnc_attached) {
        pthread_mutex_lock(&mutex_tnc_set);
        snprintf(call, size, "%s", g_tnc_settings[g_cur_tnc].mycall);
        pthread_mutex_unlock(&mutex_tnc_set);
    } else {
        snprintf(call, size, "%s", g_arim_settings.mycall);
    }
}

void arim_copy_gridsq(char *gridsq, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(gridsq, size, "%s", g_tnc_settings[g_cur_tnc].gridsq);
    pthread_mutex_unlock(&mutex_tnc_set);
}

void arim_copy_remote_gridsq(char *gridsq, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(gridsq, size, "%s", g_tnc_settings[g_cur_tnc].arq_remote_gridsq);
    pthread_mutex_unlock(&mutex_tnc_set);
}

int arim_get_netcall_cnt()
{
    int cnt;

    pthread_mutex_lock(&mutex_tnc_set);
    cnt = g_tnc_settings[g_cur_tnc].netcall_cnt;
    pthread_mutex_unlock(&mutex_tnc_set);
    return cnt;
}

int arim_copy_netcall(char *call, size_t size, int which)
{
    int result = 0;

    pthread_mutex_lock(&mutex_tnc_set);
    if (which >= 0 && which < g_tnc_settings[g_cur_tnc].netcall_cnt) {
        snprintf(call, size, "%s", g_tnc_settings[g_cur_tnc].netcall[which]);
        result = 1;
    }
    pthread_mutex_unlock(&mutex_tnc_set);
    return result;
}

void arim_copy_remote_call(char *call, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(call, size, "%s", g_tnc_settings[g_cur_tnc].arq_remote_call);
    pthread_mutex_unlock(&mutex_tnc_set);
}

void arim_copy_target_call(char *call, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(call, size, "%s", g_tnc_settings[g_cur_tnc].arq_target_call);
    pthread_mutex_unlock(&mutex_tnc_set);
}

void arim_copy_arq_sendcr(char *val, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(val, size, "%s", g_tnc_settings[g_cur_tnc].arq_sendcr);
    pthread_mutex_unlock(&mutex_tnc_set);
}

void arim_copy_arq_bw(char *val, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(val, size, "%s", g_tnc_settings[g_cur_tnc].arq_bandwidth);
    pthread_mutex_unlock(&mutex_tnc_set);
}

void arim_copy_arq_bw_hz(char *val, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(val, size, "%s", g_tnc_settings[g_cur_tnc].arq_bandwidth_hz);
    pthread_mutex_unlock(&mutex_tnc_set);
}

void arim_copy_listen(char *val, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(val, size, "%s", g_tnc_settings[g_cur_tnc].listen);
    pthread_mutex_unlock(&mutex_tnc_set);
}

void arim_copy_tnc_state(char *state, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(state, size, "%s", g_tnc_settings[g_cur_tnc].state);
    pthread_mutex_unlock(&mutex_tnc_set);
}

int arim_test_mycall(const char *call)
{
    int result;

    pthread_mutex_lock(&mutex_tnc_set);
    result = strcasecmp(call, g_tnc_settings[g_cur_tnc].mycall);
    pthread_mutex_unlock(&mutex_tnc_set);
    return (result ? 0 : 1);
}

int arim_test_netcall(const char *call)
{
    int i, cnt;

    pthread_mutex_lock(&mutex_tnc_set);
    cnt = g_tnc_settings[g_cur_tnc].netcall_cnt;
    for (i = 0; i < cnt; i++) {
        if (!strcasecmp(call, g_tnc_settings[g_cur_tnc].netcall[i]))
            break;
    }
    pthread_mutex_unlock(&mutex_tnc_set);
    return (i == cnt ? 0 : 1);
}

int arim_check(const char *msg, unsigned int cs_rcvd)
{
    unsigned int cs_msg;

    cs_msg = ccitt_crc16((unsigned char *)msg, strlen(msg));
    return (cs_msg == cs_rcvd ? 1 : 0);
}

int arim_store_out(const char *msg, const char *to_call)
{
    unsigned int check;
    char *hdr;

    check = ccitt_crc16((unsigned char *)msg, strlen(msg));
    if (g_tnc_attached)
        hdr =  mbox_add_msg(MBOX_OUTBOX_FNAME, g_tnc_settings[g_cur_tnc].mycall, to_call, check, msg, 0);
    else
        hdr =  mbox_add_msg(MBOX_OUTBOX_FNAME, g_arim_settings.mycall, to_call, check, msg, 0);
    return hdr == NULL ? 0 : 1;
}

int arim_store_sent(const char *msg, const char *to_call)
{
    unsigned int check;
    char *hdr;

    check = ccitt_crc16((unsigned char *)msg, strlen(msg));
    if (g_tnc_attached)
        hdr =  mbox_add_msg(MBOX_SENTBOX_FNAME, g_tnc_settings[g_cur_tnc].mycall, to_call, check, msg, 0);
    else
        hdr =  mbox_add_msg(MBOX_SENTBOX_FNAME, g_arim_settings.mycall, to_call, check, msg, 0);
    return hdr == NULL ? 0 : 1;
}

void arim_restore_prev_fecmode()
{
    char temp[MAX_CMD_SIZE];

    snprintf(temp, sizeof(temp), "FECMODE %s", prev_fecmode);
    bufq_queue_cmd_out(temp);
}

int arim_is_arq_state()
{
    int state;

    pthread_mutex_lock(&mutex_arim_state);
    state = arim_state;
    pthread_mutex_unlock(&mutex_arim_state);
    if (state == ST_ARQ_CONNECTED         ||
        state == ST_ARQ_MSG_RCV           ||
        state == ST_ARQ_MSG_SEND_WAIT     ||
        state == ST_ARQ_MSG_SEND          ||
        state == ST_ARQ_AUTH_SEND_A1      ||
        state == ST_ARQ_AUTH_SEND_A2      ||
        state == ST_ARQ_AUTH_SEND_A3      ||
        state == ST_ARQ_AUTH_RCV_A2_WAIT  ||
        state == ST_ARQ_AUTH_RCV_A3_WAIT  ||
        state == ST_ARQ_AUTH_RCV_A4_WAIT  ||
        state == ST_ARQ_FLIST_RCV_WAIT    ||
        state == ST_ARQ_FLIST_RCV         ||
        state == ST_ARQ_FLIST_SEND_WAIT   ||
        state == ST_ARQ_FLIST_SEND        ||
        state == ST_ARQ_FILE_RCV_WAIT     ||
        state == ST_ARQ_FILE_RCV_WAIT_OK  ||
        state == ST_ARQ_FILE_RCV          ||
        state == ST_ARQ_FILE_SEND_WAIT    ||
        state == ST_ARQ_FILE_SEND_WAIT_OK ||
        state == ST_ARQ_FILE_SEND) {
            return 1;
    }
    return 0;
}

int arim_get_state()
{
    int ret;

    pthread_mutex_lock(&mutex_arim_state);
    ret = arim_state;
    pthread_mutex_unlock(&mutex_arim_state);
    return ret;
}

void arim_set_state(int newstate)
{
    char buffer[TNC_LISTEN_SIZE], cmd[MAX_CMD_SIZE];

    if (newstate == ST_IDLE) {
        bufq_queue_cmd_out("PROTOCOLMODE FEC");
        arim_copy_listen(buffer, sizeof(buffer));
        snprintf(cmd, sizeof(cmd), "LISTEN %s", buffer);
        bufq_queue_cmd_out(cmd);
    }
    pthread_mutex_lock(&mutex_arim_state);
    arim_state = newstate;
    pthread_mutex_unlock(&mutex_arim_state);
}

void arim_reset_msg_rpt_state()
{
    arim_set_send_repeats(0);
    if (fecmode_downshift)
        arim_restore_prev_fecmode();
    rcv_nak_cnt = 0;
}

int arim_is_idle()
{
    return (arim_get_state() == ST_IDLE) ? 1 : 0;
}

int arim_is_channel_busy()
{
    int ret;

    pthread_mutex_lock(&mutex_tnc_set);
    ret = strncmp(g_tnc_settings[g_cur_tnc].busy, "TRUE", 4);
    pthread_mutex_unlock(&mutex_tnc_set);
    return ret ? 0 : 1;
}

void arim_set_channel_not_busy()
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(g_tnc_settings[g_cur_tnc].busy,
        sizeof(g_tnc_settings[g_cur_tnc].busy), "%s", "FALSE");
    pthread_mutex_unlock(&mutex_tnc_set);
    bufq_queue_debug_log("ARIM: TNC is not BUSY");
}

int arim_tnc_is_idle()
{
    int tnc_disc, tnc_ch_not_busy;

    pthread_mutex_lock(&mutex_tnc_set);
    tnc_disc = strncmp(g_tnc_settings[g_cur_tnc].state, "DISC", 4) ? 0 : 1;
    tnc_ch_not_busy = strncmp(g_tnc_settings[g_cur_tnc].busy, "TRUE", 4);
    pthread_mutex_unlock(&mutex_tnc_set);
    return (tnc_disc && tnc_ch_not_busy);
}

int arim_get_send_repeats()
{
    int ret;

    pthread_mutex_lock(&mutex_send_repeats);
    ret = send_repeats;
    pthread_mutex_unlock(&mutex_send_repeats);
    return ret;
}

void arim_set_send_repeats(int val)
{
    pthread_mutex_lock(&mutex_send_repeats);
    send_repeats = val;
    pthread_mutex_unlock(&mutex_send_repeats);
}

int arim_get_fec_repeats()
{
    int ret;

    pthread_mutex_lock(&mutex_tnc_set);
    ret = atoi(g_tnc_settings[g_cur_tnc].fecrepeats);
    pthread_mutex_unlock(&mutex_tnc_set);
    return ret;
}

int arim_get_buffer_cnt()
{
    int ret;

    pthread_mutex_lock(&mutex_tnc_set);
    ret = atoi(g_tnc_settings[g_cur_tnc].buffer);
    pthread_mutex_unlock(&mutex_tnc_set);
    return ret;
}

int arim_is_receiving()
{
    int ret;

    pthread_mutex_lock(&mutex_tnc_set);
    ret = strncasecmp(g_tnc_settings[g_cur_tnc].state, "FECRCV", 6);
    pthread_mutex_unlock(&mutex_tnc_set);
    return ret ? 0 : 1;
}

void arim_copy_fecmode(char *mode, size_t size)
{
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(mode, size, "%s", g_tnc_settings[g_cur_tnc].fecmode);
    pthread_mutex_unlock(&mutex_tnc_set);
}

void arim_fecmode_downshift()
{
    const char *p, **modes;
    char temp[MAX_CMD_SIZE], fecmode[TNC_FECMODE_SIZE];
    size_t len, i = 0;
    int result;

    arim_copy_fecmode(fecmode, sizeof(fecmode));
    len = strlen(fecmode);
    /* test TNC version to determine which FEC mode table to use */
    if (g_tnc_version.major <= 1)
        modes = downshift_v1;
    else
        modes = downshift_v2;
    p = modes[i];
    while (p) {
        result = strncasecmp(modes[i], fecmode, len);
        if (!result && *(p + len) == ',') {
            p += (len + 1);
            snprintf(temp, sizeof(temp), "FECMODE %s", p);
            bufq_queue_cmd_out(temp);
            break;
        }
        p = modes[++i];
    }
}

void arim_cancel_trans()
{
    if (arim_get_buffer_cnt() > 0)
        bufq_queue_cmd_out("ABORT");
    datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
}

int arim_cancel_unproto()
{
    char buffer[MAX_LOG_LINE_SIZE];

    /* operator has canceled the unproto transmission by pressing ESC key,
       print to monitor view and traffic log */
    snprintf(buffer, sizeof(buffer), ">> [X] (Unproto message canceled by operator)");
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
    return 1;
}

int arim_cancel_frame()
{
    char buffer[MAX_LOG_LINE_SIZE];

    /* operator has canceled the data frame receipt by pressing ESC key,
       print to monitor view and traffic log */
    snprintf(buffer, sizeof(buffer), ">> [X] (Wait for ARIM frame canceled by operator)");
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    return 1;
}

void arim_on_event(int event, int param)
{
    int prev_state, next_state;
    char buffer[MAX_LOG_LINE_SIZE];

    prev_state = arim_get_state();

    switch (prev_state) {
    case ST_IDLE:
        arim_proto_idle(event, param);
        break;
    case ST_SEND_MSG_BUF_WAIT:
        arim_proto_msg_buf_wait(event, param);
        break;
    case ST_SEND_NET_MSG_BUF_WAIT:
        arim_proto_msg_net_buf_wait(event, param);
        break;
    case ST_SEND_QRY_BUF_WAIT:
        arim_proto_query_buf_wait(event, param);
        break;
    case ST_SEND_RESP_BUF_WAIT:
        arim_proto_query_resp_buf_wait(event, param);
        break;
    case ST_SEND_ACKNAK_BUF_WAIT:
        arim_proto_msg_acknak_buf_wait(event, param);
        break;
    case ST_SEND_BCN_BUF_WAIT:
        arim_proto_beacon_buf_wait(event, param);
        break;
    case ST_SEND_UN_BUF_WAIT:
        arim_proto_unproto_buf_wait(event, param);
        break;
    case ST_SEND_RESP_PEND:
        arim_proto_query_resp_pend(event, param);
        break;
    case ST_SEND_ACKNAK_PEND:
        arim_proto_msg_acknak_pend(event, param);
        break;
    case ST_RCV_ACKNAK_WAIT:
        arim_proto_msg_acknak_wait(event, param);
        break;
    case ST_ARQ_PEND_WAIT:
        arim_proto_arq_conn_pend_wait(event, param);
        break;
    case ST_SEND_PING_ACK_PEND:
        arim_proto_ping_ack_pend(event, param);
        break;
    case ST_RCV_PING_ACK_WAIT:
        arim_proto_ping_ack_wait(event, param);
        break;
    case ST_ARQ_OUT_CONNECT_WAIT:
        arim_proto_arq_conn_out_wait(event, param);
        break;
    case ST_ARQ_OUT_CONNECT_WAIT_RPT:
        arim_proto_arq_conn_out_wait_rpt(event, param);
        break;
    case ST_RCV_ARQ_CONN_PING_ACK_WAIT:
        arim_proto_arq_conn_pp_wait(event, param);
        break;
    case ST_ARQ_IN_CONNECT_WAIT:
        arim_proto_arq_conn_in_wait(event, param);
        break;
    case ST_ARQ_CONNECTED:
        arim_proto_arq_conn_connected(event, param);
        break;
    case ST_ARQ_FLIST_RCV_WAIT:
        arim_proto_arq_file_flist_rcv_wait(event, param);
        break;
    case ST_ARQ_FLIST_RCV:
        arim_proto_arq_file_flist_rcv(event, param);
        break;
    case ST_ARQ_FLIST_SEND_WAIT:
        arim_proto_arq_file_flist_send_wait(event, param);
        break;
    case ST_ARQ_FLIST_SEND:
        arim_proto_arq_file_flist_send(event, param);
        break;
    case ST_ARQ_FILE_SEND_WAIT:
        arim_proto_arq_file_send_wait(event, param);
        break;
    case ST_ARQ_FILE_SEND_WAIT_OK:
        arim_proto_arq_file_send_wait_ok(event, param);
        break;
    case ST_ARQ_FILE_SEND:
        arim_proto_arq_file_send(event, param);
        break;
    case ST_ARQ_FILE_RCV_WAIT_OK:
        arim_proto_arq_file_rcv_wait_ok(event, param);
        break;
    case ST_ARQ_FILE_RCV_WAIT:
        arim_proto_arq_file_rcv_wait(event, param);
        break;
    case ST_ARQ_FILE_RCV:
        arim_proto_arq_file_rcv(event, param);
        break;
    case ST_ARQ_MSG_SEND_WAIT:
        arim_proto_arq_msg_send_wait(event, param);
        break;
    case ST_ARQ_MSG_SEND:
        arim_proto_arq_msg_send(event, param);
        break;
    case ST_ARQ_MSG_RCV:
        arim_proto_arq_msg_rcv(event, param);
        break;
    case ST_RCV_MSG_PING_ACK_WAIT:
        arim_proto_msg_pingack_wait(event, param);
        break;
    case ST_RCV_QRY_PING_ACK_WAIT:
        arim_proto_query_pingack_wait(event, param);
        break;
    case ST_RCV_RESP_WAIT:
        arim_proto_query_resp_wait(event, param);
        break;
    case ST_ARQ_AUTH_SEND_A1:
        arim_proto_arq_auth_send_a1_wait(event, param);
        break;
    case ST_ARQ_AUTH_SEND_A2:
        arim_proto_arq_auth_send_a2_wait(event, param);
        break;
    case ST_ARQ_AUTH_SEND_A3:
        arim_proto_arq_auth_send_a3_wait(event, param);
        break;
    case ST_ARQ_AUTH_RCV_A2_WAIT:
        arim_proto_arq_auth_rcv_a2_wait(event, param);
        break;
    case ST_ARQ_AUTH_RCV_A3_WAIT:
        arim_proto_arq_auth_rcv_a3_wait(event, param);
        break;
    case ST_ARQ_AUTH_RCV_A4_WAIT:
        arim_proto_arq_auth_rcv_a4_wait(event, param);
        break;
    case ST_RCV_FRAME_WAIT:
        arim_proto_frame_rcv_wait(event, param);
        break;
    }
    next_state = arim_get_state();
    if (event != EV_PERIODIC || prev_state != next_state) {
        snprintf(buffer, sizeof(buffer),
            "ARIM: Event %s, Param %d, State %s==>%s",
                events[event], param, states[prev_state], states[next_state]);
        bufq_queue_debug_log(buffer);
    }
}

