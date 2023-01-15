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
#include "arim_proto.h"
#include "cmdproc.h"
#include "ini.h"
#include "mbox.h"
#include "ui.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "util.h"
#include "bufq.h"
#include "datathread.h"

int arim_send_query(const char *query, const char *to_call)
{
    char mycall[TNC_MYCALL_SIZE];
    unsigned int check;
    size_t len = 0;

    if (!arim_is_idle() || !arim_tnc_is_idle())
        return 0;

    if (atoi(g_arim_settings.pilot_ping)) {
        snprintf(prev_msg, sizeof(prev_msg), "%s", query);
        snprintf(prev_to_call, sizeof(prev_to_call), "%s", to_call);
        arim_on_event(EV_SEND_QRY_PP, atoi(g_arim_settings.pilot_ping));
        return 1;
    }
    arim_copy_mycall(mycall, sizeof(mycall));
    check = ccitt_crc16((unsigned char *)query, strlen(query));
    snprintf(msg_buffer, sizeof(msg_buffer), "|Q%02d|%s|%s|%04zX|%04X|%s",
                    ARIM_PROTO_VERSION,
                    mycall,
                    to_call,
                    len,
                    check,
                    query);
    len = strlen(msg_buffer);
    snprintf(msg_buffer, sizeof(msg_buffer), "|Q%02d|%s|%s|%04zX|%04X|%s",
                    ARIM_PROTO_VERSION,
                    mycall,
                    to_call,
                    len,
                    check,
                    query);
    bufq_queue_data_out(msg_buffer);
    ack_timeout = atoi(g_arim_settings.ack_timeout);
    arim_on_event(EV_SEND_QRY, 0);
    return 1;
}

int arim_send_query_pp()
{
    char mycall[TNC_MYCALL_SIZE];
    unsigned int check, numch;
    size_t len = 0;

    arim_copy_mycall(mycall, sizeof(mycall));
    check = ccitt_crc16((unsigned char *)prev_msg, strlen(prev_msg));
    numch = snprintf(msg_buffer, sizeof(msg_buffer), "|Q%02d|%s|%s|%04zX|%04X|%s",
                     ARIM_PROTO_VERSION,
                     mycall,
                     prev_to_call,
                     len,
                     check,
                     prev_msg);
    len = strlen(msg_buffer);
    numch = snprintf(msg_buffer, sizeof(msg_buffer), "|Q%02d|%s|%s|%04zX|%04X|%s",
                     ARIM_PROTO_VERSION,
                     mycall,
                     prev_to_call,
                     len,
                     check,
                     prev_msg);
    bufq_queue_data_out(msg_buffer);
    ack_timeout = atoi(g_arim_settings.ack_timeout);
    arim_on_event(EV_SEND_QRY, 0);
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return 1;
}

int arim_recv_response(const char *fm_call, const char *to_call,
                            unsigned int check, const char *msg)
{
    char *hdr, buffer[MAX_MBOX_HDR_SIZE];
    int is_mycall, result = 1;

    /* is this message directed to mycall? */
    is_mycall = arim_test_mycall(to_call);
    if (is_mycall) {
        /* verify good checksum */
        result = arim_check(msg, check);
        if (result) {
            /* good checksum, store message into mbox, add to recents */
            hdr = mbox_add_msg(MBOX_INBOX_FNAME, fm_call, to_call, check, msg, 1);
            if (hdr != NULL) {
                pthread_mutex_lock(&mutex_recents);
                cmdq_push(&g_recents_q, hdr);
                pthread_mutex_unlock(&mutex_recents);
            }
            snprintf(buffer, sizeof(buffer), "3[R] %-10s ", fm_call);
        } else {
            snprintf(buffer, sizeof(buffer), "1[!] %-10s ", fm_call);
        }
        /* all done */
        arim_on_event(EV_RCV_RESP, 0);
    } else {
        snprintf(buffer, sizeof(buffer), "7[R] %-10s ", fm_call);
    }
    bufq_queue_heard(buffer);
    return result;
}

int arim_recv_query(const char *fm_call, const char *to_call,
                            unsigned int check, const char *query)
{
    char buffer[MAX_HEARD_SIZE], respbuf[MIN_DATA_BUF_SIZE];
    char mycall[TNC_MYCALL_SIZE];
    int is_mycall, numch, result = 1;
    size_t len = 0;

    /* is this message directed to mycall? */
    is_mycall = arim_test_mycall(to_call);
    if (is_mycall) {
        /* if so, verify good checksum and return response to sender */
        result = arim_check(query, check);
        if (result) {
            arim_copy_mycall(mycall, sizeof(mycall));
            cmdproc_query(query, respbuf, sizeof(respbuf));
            check = ccitt_crc16((unsigned char *)respbuf, strlen(respbuf));
            numch = snprintf(msg_buffer, sizeof(msg_buffer), "|R%02d|%s|%s|%04zX|%04X|%s",
                             ARIM_PROTO_VERSION,
                             mycall,
                             fm_call,
                             len,
                             check,
                             respbuf);
            len = strlen(msg_buffer);
            numch = snprintf(msg_buffer, sizeof(msg_buffer), "|R%02d|%s|%s|%04zX|%04X|%s",
                             ARIM_PROTO_VERSION,
                             mycall,
                             fm_call,
                             len,
                             check,
                             respbuf);
            /* initialize arim_proto global */
            msg_len = len;
            /* start progress meter */
            ui_status_xfer_start(0, msg_len, STATUS_XFER_DIR_UP);
            arim_on_event(EV_RCV_QRY, 0);
            snprintf(buffer, sizeof(buffer), "3[Q] %-10s ", fm_call);
        } else {
            snprintf(buffer, sizeof(buffer), "1[!] %-10s ", fm_call);
        }
    } else {
        snprintf(buffer, sizeof(buffer), "7[Q] %-10s ", fm_call);
    }
    bufq_queue_heard(buffer);
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return result;
}

size_t arim_on_send_response_buffer(size_t size)
{
    static size_t prev_size, bytes_buffered, prev_bytes_buffered;
    bytes_buffered = datathread_get_num_bytes_buffered();
    if (size != prev_size && bytes_buffered >= size) {
        /* update progress meter, handle case where BUFFER notication is lagging behind */
        if (prev_bytes_buffered > size && bytes_buffered > prev_bytes_buffered && size < prev_size)
            ui_status_xfer_update(prev_bytes_buffered - size);
        else
            ui_status_xfer_update(bytes_buffered - size);
        prev_size = size;
        prev_bytes_buffered = bytes_buffered;
        bufq_queue_cmd_out("FECSEND TRUE"); /* seems necessary for ARDOP_Win */
        return size;
    }
    return 1; /* keep alive until BUFFER count from TNC is > 0 */
}

int arim_cancel_query()
{
    char buffer[MAX_LOG_LINE_SIZE];

    /* operator has canceled the query by pressing ESC key,
       print to monitor view and traffic log */
    snprintf(buffer, sizeof(buffer), ">> [X] (Query canceled by operator)");
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    datathread_cancel_send_data_out(); /* cancel data transfer to TNC */
    return 1;
}

