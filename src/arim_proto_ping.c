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
#include "ini.h"
#include "mbox.h"
#include "bufq.h"
#include "ui.h"
#include "util.h"

void arim_proto_ping_ack_pend(int event, int param)
{
    time_t t;

    switch(event) {
    case EV_SEND_PING_ACK:
        /* a PINGREPLY asynch response was received from the TNC */
        ui_set_status_dirty(STATUS_ACKNAK_SEND);
        arim_set_state(ST_IDLE);
        break;
    case EV_ARQ_CAN_PENDING:
        /* a CANCELPENDING asynch response was received from the TNC.
           This can happen if a ping arrives when ENABLEPINGACK is false
           but LISTEN is true. In this case the ping packet is decoded
           and a PING async response is sent. This is is followed by a
           CANCELPENDING so it must be handled here */
        arim_set_state(ST_IDLE);
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting for ack send */
        if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
        }
        break;
    }
}

void arim_proto_ping_ack_wait(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_CANCEL:
        bufq_queue_cmd_out("ABORT");
        arim_set_state(ST_IDLE);
        arim_cancel_ping();
        ui_set_status_dirty(STATUS_PING_SEND_CAN);
        break;
    case EV_TNC_PTT:
        /* a PTT async response was received from the TNC */
        if (param) {
            ui_set_status_dirty(STATUS_PING_SENT);
        }
        break;
    case EV_RCV_PING_ACK:
        /* a PINGACK async response was received from the TNC */
        arim_set_state(ST_IDLE);
        ui_set_status_dirty(STATUS_PING_ACK_RCVD);
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out waiting for ack */
        if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_PING_ACK_TIMEOUT);
        }
        break;
    }
}

