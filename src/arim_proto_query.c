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
#include "arim_query.h"
#include "arim_ping.h"
#include "arim_arq.h"
#include "ini.h"
#include "mbox.h"
#include "ui.h"
#include "util.h"
#include "bufq.h"
#include "ui_tnc_data_win.h"
#include "datathread.h"

void arim_proto_query_buf_wait(int event, int param)
{
    switch (event) {
    case EV_CANCEL:
        arim_cancel_trans();
        arim_set_state(ST_IDLE);
        arim_cancel_query();
        ui_set_status_dirty(STATUS_QRY_SEND_CAN);
        break;
    case EV_PERIODIC:
        if (arim_get_buffer_cnt()) {
            /* query is buffered, change to response waiting state */
            prev_time = time(NULL);
            arim_set_state(ST_RCV_RESP_WAIT);
            ui_set_status_dirty(STATUS_WAIT_RESP);
        }
        break;
    }
}

void arim_proto_query_resp_buf_wait(int event, int param)
{
    switch (event) {
    case EV_CANCEL:
        arim_cancel_trans();
        arim_set_state(ST_IDLE);
        arim_cancel_query();
        ui_set_status_dirty(STATUS_RESP_SEND_CAN);
        break;
    case EV_PERIODIC:
        /* wait until tx buffer is empty before announcing idle state */
        if (!arim_on_send_response_buffer(arim_get_buffer_cnt())) {
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_RESP_SENT);
        }
        break;
    }
}

void arim_proto_query_resp_pend(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_CANCEL:
        arim_set_state(ST_IDLE);
        arim_cancel_query();
        ui_set_status_dirty(STATUS_RESP_SEND_CAN);
        break;
    case EV_PERIODIC:
        /* 1 second delay for sending response to query */
        t = time(NULL);
        if (t > prev_time) {
            bufq_queue_data_out(msg_buffer);
            arim_set_state(ST_SEND_RESP_BUF_WAIT);
            ui_set_status_dirty(STATUS_RESP_SEND);
        }
        break;
    }
}

void arim_proto_query_pingack_wait(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_CANCEL:
        bufq_queue_cmd_out("ABORT"); /* unconditionally abort */
        arim_set_state(ST_IDLE);
        arim_cancel_query();
        ui_set_status_dirty(STATUS_QRY_SEND_CAN);
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
            /* pingack quality below threshold, cancel query send */
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_PING_QRY_ACK_BAD);
        } else {
            arim_set_state(ST_IDLE);
            /* query send will be scheduled by this call */
            ui_set_status_dirty(STATUS_PING_QRY_SEND);
        }
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting for ack */
        if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_PING_QRY_ACK_TO);
        }
        break;
    }
}

void arim_proto_query_resp_wait(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_FRAME_START:
        if (param == 'R')
            ui_set_status_dirty(STATUS_RESP_START);
        break;
    case EV_RCV_RESP:
        arim_set_state(ST_IDLE);
        ui_set_status_dirty(STATUS_RESP_RCVD);
        break;
    case EV_CANCEL:
        arim_set_state(ST_IDLE);
        arim_cancel_query();
        ui_set_status_dirty(STATUS_RESP_WAIT_CAN);
        break;
    case EV_TNC_NEWSTATE:
        /* reload timer */
        prev_time = time(NULL);
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting for response frame. If TNC is receiving reload
           the timer. The purpose of this state is to block attempts to transmit while
           receiving an incoming response frame. */
        if (arim_is_receiving()) {
            prev_time = t;
        } else if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_RESP_TIMEOUT);
        }
        break;
    }
}

