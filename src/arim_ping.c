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
#include <ctype.h>
#include "main.h"
#include "arim_proto.h"
#include "cmdproc.h"
#include "ini.h"
#include "ui.h"
#include "ui_ping_hist.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "bufq.h"
#include "util.h"

static char ping_tcall[TNC_MYCALL_SIZE], ping_scall[TNC_MYCALL_SIZE];
static char ping_sn[8], ping_qual[8], ping_data[MAX_PING_SIZE];
static int ping_rpts, ping_count;

int arim_send_ping(const char *repeats, const char *to_call, int event)
{
    char buffer[MAX_LOG_LINE_SIZE];
    char mycall[TNC_MYCALL_SIZE], tcall[TNC_MYCALL_SIZE];
    size_t i, len;

    if (!arim_tnc_is_idle() || !repeats || !to_call)
        return 0;
    /* force call to uppercase */
    len = strlen(to_call);
    for (i = 0; i < len; i++)
        tcall[i] = toupper(to_call[i]);
    tcall[i] = '\0';
    /* cache target call and repeat count */
    snprintf(ping_tcall, sizeof(ping_tcall), "%s", tcall);
    if (repeats)
        ping_rpts = atoi(repeats);
    else
        ping_rpts = 2;
    ping_count = 0;
    /* change state if indicated, otherwise caller is taking care of that */
    if (event)
        arim_on_event(EV_SEND_PING, ping_rpts);
    /* print trace to traffic monitor view */
    arim_copy_mycall(mycall, sizeof(mycall));
    snprintf(buffer, sizeof(buffer), "<< [P] %s>%s (Pinging...)", mycall, tcall);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    /* queue command for TNC */
    snprintf(buffer, sizeof(buffer), "PING %s %d", tcall, ping_rpts);
    bufq_queue_cmd_out(buffer);
    return 1;
}

int arim_recv_ping(const char *data)
{
    snprintf(ping_data, sizeof(ping_data), "%s", data);
    arim_on_event(EV_RCV_PING, 0);
    return 1;
}

int arim_proc_ping()
{
    char *e, *scall, *tcall, *sn, *qual, *rpts;
    char pingdata[MAX_PING_SIZE], buffer[MAX_LOG_LINE_SIZE];
    char mycall[TNC_MYCALL_SIZE];
    int mycall_is_target = 0;

    /* inbound ping */
    scall = tcall = sn = qual = rpts = 0;
    snprintf(pingdata, sizeof(pingdata), "%s", ping_data + 5);
    e = pingdata;
    /* notification of received ping packet */
    while (*e && *e == ' ')
        ++e;
    if (*e) {
        scall = e;
        while (*e && *e != '>')
            ++e;
        if (*e && *(e + 1)) {
            *e++ = '\0';
            tcall = e;
            while (*e && *e != ' ')
                ++e;
            if (*e && *(e + 1)) {
                *e++ = '\0';
                sn = e;
                while (*e && *e != ' ')
                    ++e;
                if (*e && *(e + 1)) {
                    *e = '\0';
                    qual = e + 1;
                }
            }
        }
    }
    if (scall && tcall && sn && qual) {
        arim_copy_mycall(mycall, sizeof(mycall));
        if (!strncasecmp(mycall, tcall, strlen(mycall))) {
            snprintf(buffer, sizeof(buffer), "4[P] %-10s ", scall);
            bufq_queue_heard(buffer);
            /* cache info until pingreply notification from TNC */
            snprintf(ping_tcall, sizeof(ping_tcall), "%s", tcall);
            snprintf(ping_scall, sizeof(ping_scall), "%s", scall);
            snprintf(ping_sn, sizeof(ping_sn), "%s", sn);
            snprintf(ping_qual, sizeof(ping_qual), "%s", qual);
            mycall_is_target = 1;
        } else {
            snprintf(buffer, sizeof(buffer), "7[P] %-10s ", scall);
            bufq_queue_heard(buffer);
        }
        snprintf(buffer, sizeof(buffer), ">> [P] %s>%s", scall, tcall);
        bufq_queue_traffic_log(buffer);
        bufq_queue_data_in(buffer);
    }
    return mycall_is_target;
}

int arim_send_ping_ack()
{
    char mycall[TNC_MYCALL_SIZE], buffer[MAX_LOG_LINE_SIZE];
    int db;

    arim_copy_mycall(mycall, sizeof(mycall));
    /* A PINGREPLY asynch response has been received from the TNC */
    db = atoi(ping_sn);
    snprintf(buffer, sizeof(buffer),
             "<< [p] %s>%s S/N: %sdB, Quality: %s",
                mycall, ping_scall, db > 20 ? ">20" : ping_sn, ping_qual);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    snprintf(buffer, sizeof(buffer), "S%-12s------%3s%3s",
             ping_scall, db > 20 ? ">20" : ping_sn, ping_qual);
    bufq_queue_ptable(buffer);
    arim_on_event(EV_SEND_PING_ACK, 0);
    return 1;
}

int arim_recv_ping_ack(const char *data)
{
    char *e, *sn, *qual;
    char pingdata[MAX_PING_SIZE], buffer[MAX_LOG_LINE_SIZE];
    char mycall[TNC_MYCALL_SIZE];
    int state, db = 0, status = 0;

    state = arim_get_state();
    if (state == ST_RCV_PING_ACK_WAIT     ||
        state == ST_RCV_MSG_PING_ACK_WAIT ||
        state == ST_RCV_QRY_PING_ACK_WAIT ||
        state == ST_RCV_ARQ_CONN_PING_ACK_WAIT) {
        /* a  PINGACK asynch response has been received from the TNC */
        sn = qual = 0;
        snprintf(pingdata, sizeof(pingdata), "%s", data + 8);
        e = pingdata;
        while (*e && *e == ' ')
            ++e;
        if (*e) {
            sn = e;
            while (*e && *e != ' ')
                ++e;
            if (*e && *(e + 1)) {
                *e = '\0';
                qual = e + 1;
            }
        }
        if (sn && qual) {
            db = atoi(sn);
            snprintf(buffer, sizeof(buffer), "4[p] %-10s ", ping_tcall);
            bufq_queue_heard(buffer);
            arim_copy_mycall(mycall, sizeof(mycall));
            snprintf(buffer, sizeof(buffer), ">> [p] %s>%s S/N: %sdB, Quality: %s",
                     ping_tcall, mycall, db > 20 ? ">20" : sn, qual);
            bufq_queue_traffic_log(buffer);
            bufq_queue_data_in(buffer);
            snprintf(buffer, sizeof(buffer), "R%-12s%3s%3s------",
                     ping_tcall, db > 20 ? ">20" : sn, qual);
            bufq_queue_ptable(buffer);
            if (atoi(qual) < atoi(g_arim_settings.pilot_ping_thr)) {
                status = -1;
                snprintf(buffer, sizeof(buffer),
                         "PP: Send canceled, PINGACK quality %s below threshold %s",
                             qual, g_arim_settings.pilot_ping_thr);
                bufq_queue_debug_log(buffer);
            }
        }
        arim_on_event(EV_RCV_PING_ACK, status);
    }
    return 1;
}

int arim_cancel_ping()
{
    char buffer[MAX_LOG_LINE_SIZE];
    char mycall[TNC_MYCALL_SIZE];

    /* operator has canceled the ping sequence by pressing ESC key,
       print to monitor view and traffic log */
    arim_copy_mycall(mycall, sizeof(mycall));
    snprintf(buffer, sizeof(buffer), ">> [X] %s>%s (Ping canceled by operator)",
             mycall, ping_tcall);
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    return 1;
}

