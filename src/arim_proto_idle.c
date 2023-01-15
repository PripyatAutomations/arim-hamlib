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

void arim_proto_idle(int event, int param)
{
    char buffer[MAX_LOG_LINE_SIZE];

    switch (event) {
    case EV_FRAME_START:
        ack_timeout = atoi(g_arim_settings.frame_timeout);
        prev_time = time(NULL);
        arim_set_state(ST_RCV_FRAME_WAIT);
        bufq_queue_cmd_out("LISTEN FALSE");
        switch (param) {
        case 'M':
            ui_set_status_dirty(STATUS_MSG_START);
            break;
        case 'Q':
            ui_set_status_dirty(STATUS_QRY_START);
            break;
        case 'R':
            ui_set_status_dirty(STATUS_RESP_START);
            break;
        case 'B':
            ui_set_status_dirty(STATUS_BCN_START);
            break;
        default:
            ui_set_status_dirty(STATUS_FRAME_START);
            break;
        }
        break;
    case EV_SEND_BCN:
        ack_timeout = ARDOP_BCN_SEND_TIMEOUT;
        prev_time = time(NULL);
        arim_set_state(ST_SEND_BCN_BUF_WAIT);
        bufq_queue_cmd_out("LISTEN FALSE");
        break;
    case EV_SEND_MSG:
        arim_set_state(ST_SEND_MSG_BUF_WAIT);
        bufq_queue_cmd_out("LISTEN FALSE");
        break;
    case EV_SEND_MSG_PP:
        ack_timeout = param * ARDOP_PINGACK_TIMEOUT;
        prev_time = time(NULL);
        if (arim_send_ping(g_arim_settings.pilot_ping, prev_to_call, 0)) {
            arim_set_state(ST_RCV_MSG_PING_ACK_WAIT);
            bufq_queue_cmd_out("LISTEN FALSE");
        } else {
            ui_set_status_dirty(STATUS_PING_TNC_BUSY);
        }
        break;
    case EV_SEND_NET_MSG:
        arim_set_state(ST_SEND_NET_MSG_BUF_WAIT);
        bufq_queue_cmd_out("LISTEN FALSE");
        break;
    case EV_SEND_UNPROTO:
        arim_set_state(ST_SEND_UN_BUF_WAIT);
        ui_set_status_dirty(STATUS_REFRESH);
        bufq_queue_cmd_out("LISTEN FALSE");
        break;
    case EV_SEND_QRY:
        arim_set_state(ST_SEND_QRY_BUF_WAIT);
        bufq_queue_cmd_out("LISTEN FALSE");
        break;
    case EV_SEND_QRY_PP:
        ack_timeout = param * ARDOP_PINGACK_TIMEOUT;
        prev_time = time(NULL);
        if (arim_send_ping(g_arim_settings.pilot_ping, prev_to_call, 0)) {
            arim_set_state(ST_RCV_QRY_PING_ACK_WAIT);
            bufq_queue_cmd_out("LISTEN FALSE");
        } else {
            ui_set_status_dirty(STATUS_PING_TNC_BUSY);
        }
        break;
    case EV_SEND_PING:
        /* a PING command was sent to the TNC */
        ack_timeout = ARDOP_PINGACK_TIMEOUT * param;
        prev_time = time(NULL);
        arim_set_state(ST_RCV_PING_ACK_WAIT);
        bufq_queue_cmd_out("LISTEN FALSE");
        break;
    case EV_ARQ_PENDING:
        /* a PENDING async response was received from the TNC
           heralding arrival of an ARQ connect or ping frame */
        arim_copy_listen(buffer, sizeof(buffer));
        if (!strncasecmp(buffer, "TRUE", 4)) {
            /* respond only if ARQ listen is TRUE */
            bufq_queue_cmd_out("PROTOCOLMODE ARQ");
            ack_timeout = ARDOP_CONNREQ_TIMEOUT;
            prev_time = time(NULL);
            arim_set_state(ST_ARQ_PEND_WAIT);
        }
        break;
    case EV_ARQ_CONNECT:
        /* an ARQ connection attempt is underway */
        ack_timeout = ARDOP_CONNREQ_TIMEOUT;
        prev_time = time(NULL);
        arim_set_state(ST_ARQ_OUT_CONNECT_WAIT);
        bufq_queue_cmd_out("LISTEN FALSE");
        bufq_queue_cmd_out("PROTOCOLMODE ARQ");
        break;
    case EV_ARQ_CONNECT_PP:
        /* an ARQ connection attempt is underway */
        ack_timeout = param * ARDOP_PINGACK_TIMEOUT;
        prev_time = time(NULL);
        if (arim_send_ping(g_arim_settings.pilot_ping, prev_to_call, 0))
            arim_set_state(ST_RCV_ARQ_CONN_PING_ACK_WAIT);
        else
            ui_set_status_dirty(STATUS_PING_TNC_BUSY);
        break;
    }
}

