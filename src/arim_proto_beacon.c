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
#include "arim_beacon.h"
#include "arim_ping.h"
#include "arim_arq.h"
#include "ini.h"
#include "mbox.h"
#include "ui.h"
#include "util.h"

void arim_proto_beacon_buf_wait(int event, int param)
{
    time_t t;

    switch (event) {
    case EV_CANCEL:
        arim_cancel_trans();
        arim_set_state(ST_IDLE);
        arim_beacon_cancel();
        ui_set_status_dirty(STATUS_BCN_SEND_CAN);
        break;
    case EV_PERIODIC:
        /* wait until tx buffer is empty before announcing idle state */
        if (!arim_get_buffer_cnt()) {
            arim_set_state(ST_IDLE);
            ui_set_status_dirty(STATUS_BEACON_SENT);
        } else {
            t = time(NULL);
            /* see if we timed out waiting for buffer to empty */
            if (t > prev_time + ack_timeout) {
                /* timeout, abandon attempt to send beacon */
                arim_cancel_trans();
                arim_set_state(ST_IDLE);
                arim_beacon_timeout();
            }
        }
        break;
    }
}

