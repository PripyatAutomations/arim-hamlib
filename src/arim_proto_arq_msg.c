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
#include "arim_proto_msg.h"
#include "arim_ping.h"
#include "arim_arq.h"
#include "arim_arq_msg.h"
#include "arim_arq_auth.h"
#include "ini.h"
#include "mbox.h"
#include "bufq.h"
#include "ui.h"
#include "util.h"

void arim_proto_arq_msg_send_wait(int event, int param)
{
    time_t t;
    char buffer[MAX_LOG_LINE_SIZE];

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
            /* still sending, reload timer */
            prev_time = time(NULL);
        }
        break;
    case EV_ARQ_CANCEL_WAIT:
        /* wait canceled, return to connected state */
        arim_set_state(ST_ARQ_CONNECTED);
        ui_set_status_dirty(STATUS_ARQ_CONN_CAN);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
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
    case EV_PERIODIC:
        if (arim_get_buffer_cnt()) {
            /* data is buffered, change state to sending */
            ack_timeout = ARDOP_CONN_SEND_TIMEOUT;
            prev_time = time(NULL);
            arim_set_state(ST_ARQ_MSG_SEND);
            arim_arq_msg_on_send_msg();
            ui_set_status_dirty(STATUS_ARQ_MSG_SEND);
        } else {
            t = time(NULL);
            /* see if we timed out */
            if (t > prev_time + ack_timeout) {
                /* timeout, return to connected state */
                ack_timeout = ARDOP_CONN_TIMEOUT;
                prev_time = time(NULL);
                arim_set_state(ST_ARQ_CONNECTED);
                ui_set_status_dirty(STATUS_ARQ_MSG_SEND_TIMEOUT);
            }
        }
        break;
    }
}

void arim_proto_arq_msg_send(int event, int param)
{
    time_t t;
    char buffer[MAX_LOG_LINE_SIZE];

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
            /* still sending, reload timer */
            prev_time = time(NULL);
        }
        break;
    case EV_ARQ_MSG_OK:
        /* success, return to connected state */
        arim_set_state(ST_ARQ_CONNECTED);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_MSG_SEND_ACK);
        break;
    case EV_ARQ_MSG_ERROR:
        /* something went wrong */
        arim_set_state(ST_ARQ_CONNECTED);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_MSG_SEND_ERROR);
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
    case EV_PERIODIC:
        if (!arim_arq_msg_on_send_buffer(arim_get_buffer_cnt())) {
            /* done sending message, will wait for ack */
            ui_set_status_dirty(STATUS_ARQ_MSG_SEND_DONE);
        } else {
            t = time(NULL);
            /* see if we timed out */
            if (t > prev_time + ack_timeout) {
                /* timeout, return to connected state */
                ack_timeout = ARDOP_CONN_TIMEOUT;
                prev_time = time(NULL);
                arim_set_state(ST_ARQ_CONNECTED);
                ui_set_status_dirty(STATUS_ARQ_MSG_SEND_TIMEOUT);
            }
        }
        break;
    }
}

void arim_proto_arq_msg_rcv(int event, int param)
{
    time_t t;
    char buffer[MAX_LOG_LINE_SIZE];

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
            /* still sending, reload timer */
            prev_time = time(NULL);
        }
        break;
    case EV_ARQ_MSG_RCV_FRAME:
        /* still receiving, reload timer */
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_MSG_RCV);
        break;
    case EV_ARQ_MSG_RCV_DONE:
        /* done */
        arim_set_state(ST_ARQ_CONNECTED);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_MSG_RCV_DONE);
        break;
    case EV_ARQ_MSG_ERROR:
        /* done */
        arim_set_state(ST_ARQ_CONNECTED);
        ack_timeout = ARDOP_CONN_TIMEOUT;
        prev_time = time(NULL);
        ui_set_status_dirty(STATUS_ARQ_MSG_RCV_ERROR);
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
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out */
        if (t > prev_time + ack_timeout) {
            /* timeout, return to connected state */
            ack_timeout = ARDOP_CONN_TIMEOUT;
            prev_time = time(NULL);
            arim_set_state(ST_ARQ_CONNECTED);
            ui_set_status_dirty(STATUS_ARQ_MSG_RCV_TIMEOUT);
        }
        break;
    }
}

