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
#include "ui.h"
#include "util.h"

void arim_proto_frame_rcv_wait(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_FRAME_TO:
        arim_set_state(ST_IDLE);
        ui_set_status_dirty(STATUS_ARIM_FRAME_TO);
        break;
    case EV_FRAME_END:
        switch (param) {
        case 'M':
            ui_set_status_dirty(STATUS_MSG_END);
            break;
        case 'Q':
            ui_set_status_dirty(STATUS_QRY_END);
            break;
        case 'R':
            ui_set_status_dirty(STATUS_RESP_END);
            break;
        case 'B':
            ui_set_status_dirty(STATUS_BCN_END);
            break;
        default:
            ui_set_status_dirty(STATUS_FRAME_END);
            break;
        }
        arim_set_state(ST_IDLE);
        break;
    case EV_RCV_MSG:
        prev_time = time(NULL);
        arim_set_state(ST_SEND_ACKNAK_PEND);
        ui_set_status_dirty(STATUS_MSG_RCVD);
        break;
    case EV_RCV_QRY:
        prev_time = time(NULL);
        arim_set_state(ST_SEND_RESP_PEND);
        ui_set_status_dirty(STATUS_QUERY_RCVD);
        break;
    case EV_RCV_NET_MSG:
        arim_set_state(ST_IDLE);
        ui_set_status_dirty(STATUS_NET_MSG_RCVD);
        break;
    case EV_CANCEL:
        arim_set_state(ST_IDLE);
        arim_cancel_frame();
        ui_set_status_dirty(STATUS_FRAME_WAIT_CAN);
        break;
    case EV_TNC_NEWSTATE:
        /* reload timer */
        prev_time = time(NULL);
        break;
    case EV_PERIODIC:
        t = time(NULL);
        /* see if we timed out while receiving ARIM frame not addressed to this TNC. If
           TNC is receiving reload the timer. The purpose of this state is to block attempts
           to transmit while receiving an incoming ARIM frame. */
        if (arim_is_receiving()) {
            prev_time = t;
        } else if (t > prev_time + ack_timeout) {
            /* timeout, all done */
            arim_set_state(ST_IDLE);
        }
        break;
    }
}

