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
#include "arim_message.h"
#include "arim_ping.h"
#include "arim_arq.h"
#include "ini.h"
#include "mbox.h"
#include "ui.h"
#include "util.h"
#include "bufq.h"
#include "ui_tnc_data_win.h"

void arim_proto_msg_buf_wait(int event, int param)
{
    switch (event) {
    case EV_CANCEL:
        arim_cancel_trans();
        arim_reset_msg_rpt_state();
        arim_set_state(ST_IDLE);
        arim_cancel_msg();
        ui_set_status_dirty(STATUS_MSG_SEND_CAN);
        break;
    case EV_PERIODIC:
        /* wait until tx buffer is empty before starting ack timer */
        if (!arim_msg_on_send_buffer(arim_get_buffer_cnt())) {
            prev_time = time(NULL);
            arim_set_state(ST_RCV_ACKNAK_WAIT);
            ui_set_status_dirty(STATUS_MSG_WAIT_ACK);
        }
        break;
    }
}

void arim_proto_msg_net_buf_wait(int event, int param)
{
    switch (event) {
    case EV_CANCEL:
        arim_cancel_trans();
        arim_set_state(ST_IDLE);
        arim_cancel_msg();
        ui_set_status_dirty(STATUS_MSG_SEND_CAN);
        break;
    case EV_PERIODIC:
        /* no ACK for net message, return to idle state when buffer empty */
        if (!arim_msg_on_send_buffer(arim_get_buffer_cnt())) {
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_NET_MSG_SENT);
        }
        break;
    }
}

extern void arim_proto_msg_acknak_buf_wait(int event, int param)
{
    switch (event) {
    case EV_CANCEL:
        arim_cancel_trans();
        arim_set_state(ST_IDLE);
        arim_cancel_msg();
        ui_set_status_dirty(STATUS_MSG_SEND_CAN);
        break;
    case EV_PERIODIC:
        /* wait until tx buffer is empty before announcing idle state */
        if (!arim_get_buffer_cnt()) {
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_ACKNAK_SENT);
        }
        break;
    }
}

extern void arim_proto_msg_acknak_pend(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_CANCEL:
        arim_set_state(ST_IDLE);
        arim_cancel_msg();
        ui_set_status_dirty(STATUS_MSG_SEND_CAN);
        break;
    case EV_PERIODIC:
        /* 1 to 2 second delay for sending ack/nak */
        t = time(NULL);
        if (t > prev_time + 2) {
            bufq_queue_data_out(msg_acknak_buffer);
            arim_set_state(ST_SEND_ACKNAK_BUF_WAIT);
            ui_set_status_dirty(STATUS_ACKNAK_SEND);
        }
        break;
    }
}

extern void arim_proto_msg_acknak_wait(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_RCV_ACK:
        arim_reset_msg_rpt_state();
        arim_set_state(ST_IDLE);
        ui_set_status_dirty(STATUS_MSG_ACK_RCVD);
        break;
    case EV_RCV_NAK:
        if (++rcv_nak_cnt > arim_get_send_repeats()) {
            /* timeout, all done */
            arim_reset_msg_rpt_state();
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_MSG_NAK_RCVD);
        } else {
            prev_time = time(NULL);
        }
        break;
    case EV_CANCEL:
        arim_reset_msg_rpt_state();
        arim_set_state(ST_IDLE);
        arim_cancel_msg();
        ui_set_status_dirty(STATUS_MSG_SEND_CAN);
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting for ack */
        if (t > prev_time + ack_timeout) {
            if (++rcv_nak_cnt > arim_get_send_repeats()) {
                /* timeout, all done */
                arim_reset_msg_rpt_state();
                arim_set_state(ST_IDLE);
                ui_set_status_dirty(STATUS_MSG_ACK_TIMEOUT);
            } else {
                if (fecmode_downshift)
                    arim_fecmode_downshift();
                bufq_queue_data_out(msg_buffer);
                prev_time = t;
                arim_set_state(ST_SEND_MSG_BUF_WAIT);
                /* start progress meter */
                ui_status_xfer_start(0, msg_len, STATUS_XFER_DIR_UP);
                ui_set_status_dirty(STATUS_MSG_REPEAT);
            }
        }
        break;
    }
}

void arim_proto_msg_pingack_wait(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_CANCEL:
        bufq_queue_cmd_out("ABORT"); /* unconditionally abort */
        arim_set_state(ST_IDLE);
        arim_cancel_msg();
        ui_set_status_dirty(STATUS_MSG_SEND_CAN);
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
            /* pingack quality below threshold, cancel message send */
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_PING_MSG_ACK_BAD);
        } else {
            arim_set_state(ST_IDLE);
            /* message send will be scheduled by this call */
            ui_set_status_dirty(STATUS_PING_MSG_SEND);
        }
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting for ack */
        if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_PING_MSG_ACK_TO);
        }
        break;
    }
}

