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
#include "arim.h"
#include "arim_proto.h"
#include "arim_ping.h"
#include "arim_arq.h"
#include "arim_arq_auth.h"
#include "ini.h"
#include "mbox.h"
#include "bufq.h"
#include "ui.h"
#include "util.h"

void arim_proto_arq_conn_pend_wait(int event, int param)
{
    time_t t;

    switch(event) {
    case EV_RCV_PING:
        /* a PING asynch response was received from the TNC */
        if (arim_proc_ping()) {
            arim_set_state(ST_SEND_PING_ACK_PEND);
            ui_set_status_dirty(STATUS_PING_RCVD);
        } else {
            arim_set_state(ST_IDLE);
        }
        break;
    case EV_TNC_PTT:
        /* a PTT async response was received from the TNC */
        if (param) {
            /* reload timer */
            prev_time = time(NULL);
        }
        break;
    case EV_ARQ_CAN_PENDING:
        /* a CANCELPENDING asynch response was received from the TNC */
        arim_set_state(ST_IDLE);
        break;
    case EV_ARQ_TARGET:
        /* a TARGET asynch response was received from the TNC */
        if (arim_arq_on_target()) {
            ack_timeout = ARDOP_CONNREQ_TIMEOUT;
            t = time(NULL);
            arim_set_state(ST_ARQ_IN_CONNECT_WAIT);
        } else {
            arim_set_state(ST_IDLE);
        }
        break;
    case EV_ARQ_REJ_BUSY:
        /* a REJECTEDBUSY asynch response was received from the TNC */
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_rej_busy();
        ui_set_status_dirty(STATUS_ARQ_CONN_REQ_FAIL);
        break;
    case EV_ARQ_REJ_BW:
        /* a REJECTEDBW asynch response was received from the TNC */
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_rej_bw();
        ui_set_status_dirty(STATUS_ARQ_CONN_REQ_FAIL);
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting */
        if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
        }
        break;
    }
}

void arim_proto_arq_conn_out_wait(int event, int param)
{
    time_t t;
    char buffer[MAX_LOG_LINE_SIZE];

    /* handle every case encountered in testing to date,
       with a timeout mechanism as additional protection */
    switch (event) {
    case EV_CANCEL:
        bufq_queue_cmd_out("ABORT");
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_cancel();
        ui_set_status_dirty(STATUS_ARQ_CONN_CAN);
        break;
    case EV_TNC_PTT:
        /* a PTT async response was received from the TNC */
        if (param) {
            /* reload timer */
            prev_time = time(NULL);
        }
        break;
    case EV_ARQ_CONNECTED:
        /* a CONNECTED async response was received from the TNC */
        arim_set_state(ST_ARQ_CONNECTED);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        arim_arq_on_connected();
        ui_set_status_dirty(STATUS_ARQ_CONNECTED);
        break;
    case EV_ARQ_DISCONNECTED:
        /* a DISCONNECTED async response was received from the TNC */
        arim_set_state(ST_IDLE);
        arim_arq_on_disconnected();
        ui_set_status_dirty(STATUS_ARQ_DISCONNECTED);
        break;
    case EV_TNC_NEWSTATE:
        /* this may be the first or only notice of disconnection in some cases */
        arim_copy_tnc_state(buffer, sizeof(buffer));
        if (!strncasecmp(buffer, "DISC", 4)) {
            arim_set_state(ST_IDLE);
            arim_arq_on_conn_fail();
            ui_set_status_dirty(STATUS_ARQ_CONN_REQ_FAIL);
        } else {
            /* reload timer */
            ack_timeout = ARDOP_CONN_TIMEOUT;
            prev_time = time(NULL);
            ui_set_status_dirty(STATUS_REFRESH);
        }
        break;
    case EV_ARQ_REJ_BUSY:
        /* a REJECTEDBUSY asynch response was received from the TNC */
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_rej_busy();
        ui_set_status_dirty(STATUS_ARQ_CONN_REQ_FAIL);
        break;
    case EV_ARQ_REJ_BW:
        /* a REJECTEDBW asynch response was received from the TNC */
        if (arim_arq_on_conn_rej_bw()) {
            arim_set_state(ST_ARQ_OUT_CONNECT_WAIT_RPT);
            /* trying again, so reload timer */
            ack_timeout = ARDOP_OUT_CONN_RPT_TIMEOUT;
            prev_time = time(NULL);
        } else {
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_ARQ_CONN_REQ_FAIL);
        }
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting for connection ack */
        if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
            arim_arq_on_conn_fail();
            ui_set_status_dirty(STATUS_ARQ_CONN_REQ_FAIL);
        }
        break;
    }
}

void arim_proto_arq_conn_out_wait_rpt(int event, int param)
{
    time_t t;

    /* delay for ARDOP_OUT_CONN_RPT_TIMEOUT seconds before
       repeating a connection request with a new ARQBW */
    switch (event) {
    case EV_CANCEL:
        bufq_queue_cmd_out("ABORT");
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_cancel();
        ui_set_status_dirty(STATUS_ARQ_CONN_CAN);
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if it's time to repeat connection request */
        if (t > prev_time + ack_timeout) {
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_ARQ_CONN_REQ_REPEAT);
        }
        break;
    }
}

void arim_proto_arq_conn_pp_wait(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_CANCEL:
        bufq_queue_cmd_out("ABORT");
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_cancel();
        ui_set_status_dirty(STATUS_ARQ_CONN_CAN);
        break;
    case EV_TNC_PTT:
        /* a PTT async response was received from the TNC */
        if (param) {
            ui_set_status_dirty(STATUS_PING_SENT);
        }
        break;
    case EV_RCV_PING_ACK:
        /* a PINGACK async response was received from the TNC */
        if (param < 0) {
            /* pingack quality below threshold, cancel connection request */
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_ARQ_CONN_PP_ACK_BAD);
        } else {
            arim_set_state(ST_IDLE);
            /* connection request send will be scheduled by this call */
            ui_set_status_dirty(STATUS_ARQ_CONN_PP_SEND);
        }
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting for ping ack */
        if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_ARQ_CONN_PP_ACK_TO);
        }
        break;
    }
}

void arim_proto_arq_conn_in_wait(int event, int param)
{
    time_t t;
    char buffer[MAX_LOG_LINE_SIZE];

    /* handle every case encountered in testing to date,
       with a timeout mechanism as additional protection */
    switch (event) {
    case EV_CANCEL:
        bufq_queue_cmd_out("ABORT");
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_cancel();
        ui_set_status_dirty(STATUS_ARQ_CONN_CAN);
        break;
    case EV_TNC_PTT:
        /* a PTT async response was received from the TNC */
        if (param) {
            /* reload timer */
            prev_time = time(NULL);
        }
        break;
    case EV_ARQ_CONNECTED:
        /* a CONNECTED async response was received from the TNC */
        arim_set_state(ST_ARQ_CONNECTED);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        arim_arq_on_connected();
        ui_set_status_dirty(STATUS_ARQ_CONNECTED);
        break;
    case EV_ARQ_DISCONNECTED:
        /* a DISCONNECTED async response was received from the TNC */
        arim_set_state(ST_IDLE);
        arim_arq_on_disconnected();
        ui_set_status_dirty(STATUS_ARQ_DISCONNECTED);
        break;
    case EV_TNC_NEWSTATE:
        /* this may be the first or only notice of disconnection in some cases */
        arim_copy_tnc_state(buffer, sizeof(buffer));
        if (!strncasecmp(buffer, "DISC", 4)) {
            arim_set_state(ST_IDLE);
            arim_arq_on_disconnected();
            ui_set_status_dirty(STATUS_ARQ_DISCONNECTED);
        } else {
            /* reload timer */
            ack_timeout = ARDOP_CONN_TIMEOUT;
            prev_time = time(NULL);
            ui_set_status_dirty(STATUS_REFRESH);
        }
        break;
    case EV_ARQ_REJ_BUSY:
        /* a REJECTEDBUSY asynch response was received from the TNC */
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_rej_busy();
        ui_set_status_dirty(STATUS_ARQ_CONN_REQ_FAIL);
        break;
    case EV_ARQ_REJ_BW:
        /* a REJECTEDBW asynch response was received from the TNC */
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_rej_bw();
        ui_set_status_dirty(STATUS_ARQ_CONN_REQ_FAIL);
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting */
        if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
            arim_arq_on_conn_timeout();
            ui_set_status_dirty(STATUS_ARQ_CONN_TIMEOUT);
        }
        break;
    }
}

void arim_proto_arq_conn_connected(int event, int param)
{
    char buffer[MAX_LOG_LINE_SIZE];

    /* handle every case encountered in testing to date */
    switch (event) {
    case EV_CANCEL:
        bufq_queue_cmd_out("ABORT");
        arim_set_state(ST_IDLE);
        arim_arq_on_conn_cancel();
        ui_set_status_dirty(STATUS_ARQ_CONN_CAN);
        break;
    case EV_ARQ_FLIST_RCV_WAIT:
        /* after outgoing /FLGET, wait for incoming /FLPUT */
        arim_set_state(ST_ARQ_FLIST_RCV_WAIT);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_FLIST_RCV_WAIT);
        break;
    case EV_ARQ_FLIST_SEND_CMD:
        /* start sending file listing */
        arim_set_state(ST_ARQ_FLIST_SEND_WAIT);
        ack_timeout = ARDOP_CONN_SEND_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_FLIST_SEND);
        break;
    case EV_ARQ_FILE_SEND_CMD:
        /* start sending file */
        arim_set_state(ST_ARQ_FILE_SEND_WAIT);
        ack_timeout = ARDOP_CONN_SEND_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_FILE_SEND);
        break;
    case EV_ARQ_FILE_SEND_CMD_CLIENT:
        /* wait for response from remote station */
        arim_set_state(ST_ARQ_FILE_SEND_WAIT_OK);
        ack_timeout = ARDOP_CONN_SEND_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_FILE_SEND);
        break;
    case EV_ARQ_FILE_RCV_WAIT:
        /* after outgoing /FGET, wait for incoming /FPUT command */
        arim_set_state(ST_ARQ_FILE_RCV_WAIT);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_FILE_RCV_WAIT);
        break;
    case EV_ARQ_FILE_RCV_WAIT_OK:
        /* after incoming /FPUT, wait for outgoing /OK response */
        arim_set_state(ST_ARQ_FILE_RCV_WAIT_OK);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_FILE_RCV);
        break;
    case EV_ARQ_FILE_RCV:
        /* start receiving file */
        arim_set_state(ST_ARQ_FILE_RCV);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_FILE_RCV);
        break;
    case EV_ARQ_MSG_SEND_CMD:
        /* start sending message */
        arim_set_state(ST_ARQ_MSG_SEND_WAIT);
        ack_timeout = ARDOP_CONN_SEND_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_MSG_SEND);
        break;
    case EV_ARQ_MSG_RCV:
        /* start receiving message */
        arim_set_state(ST_ARQ_MSG_RCV);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_MSG_RCV);
        break;
    case EV_ARQ_AUTH_SEND_CMD:
        /* start sending auth command */
        if (param == 1) {
            arim_set_state(ST_ARQ_AUTH_SEND_A1);
        } else {
            /* auth challenge received, send a2 response */
            arim_arq_auth_on_send_a2();
            arim_set_state(ST_ARQ_AUTH_SEND_A2);
        }
        ack_timeout = ARDOP_CONN_SEND_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_AUTH_BUSY);
        break;
    case EV_ARQ_AUTH_ERROR:
        /* remote station can't authenticate this station */
        arim_arq_auth_on_error();
        arim_set_state(ST_ARQ_CONNECTED);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_EAUTH_LOCAL);
        break;
    case EV_ARQ_DISCONNECTED:
        /* a DISCONNECTED async response was received from the TNC */
        arim_set_state(ST_IDLE);
        arim_arq_on_disconnected();
        ui_set_status_dirty(STATUS_ARQ_DISCONNECTED);
        break;
    case EV_TNC_NEWSTATE:
        arim_copy_tnc_state(buffer, sizeof(buffer));
        if (!strncasecmp(buffer, "DISC", 4)) {
            /* this may be the only notice of disconnection in some cases */
            arim_set_state(ST_IDLE);
            arim_arq_on_disconnected();
            ui_set_status_dirty(STATUS_ARQ_DISCONNECTED);
        } else {
            /* reload timer */
            ack_timeout = ARDOP_CONN_TIMEOUT;
            prev_time = time(NULL);
            ui_set_status_dirty(STATUS_REFRESH);
        }
        break;
    }
}

