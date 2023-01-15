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
#include <string.h>
#include <ctype.h>
#include "main.h"
#include "ardop_data.h"
#include "arim.h"
#include "arim_proto.h"
#include "arim_arq.h"
#include "arim_arq_files.h"
#include "arim_arq_msg.h"
#include "bufq.h"

int arim_data_waiting = 0;
time_t arim_start_time = 0;
size_t num_bytes_in, num_bytes_out;

void ardop_data_inc_num_bytes_in(size_t num)
{
    pthread_mutex_lock(&mutex_num_bytes);
    num_bytes_in += num;
    pthread_mutex_unlock(&mutex_num_bytes);
}

void ardop_data_inc_num_bytes_out(size_t num)
{
    pthread_mutex_lock(&mutex_num_bytes);
    num_bytes_out += num;
    pthread_mutex_unlock(&mutex_num_bytes);
}

size_t ardop_data_get_num_bytes_in()
{
    size_t num;

    pthread_mutex_lock(&mutex_num_bytes);
    num = num_bytes_in;
    pthread_mutex_unlock(&mutex_num_bytes);
    return num;
}

size_t ardop_data_get_num_bytes_out()
{
    size_t num;

    pthread_mutex_lock(&mutex_num_bytes);
    num = num_bytes_out;
    pthread_mutex_unlock(&mutex_num_bytes);
    return num;
}

void ardop_data_reset_num_bytes()
{
    pthread_mutex_lock(&mutex_num_bytes);
    num_bytes_out = num_bytes_in = 0;
    pthread_mutex_unlock(&mutex_num_bytes);
}

void ardop_data_on_fec(char *data, size_t size)
{
    char inbuffer[MIN_DATA_BUF_SIZE];
    size_t i, j;

    snprintf(inbuffer, size + 8, ">> [U] %s", data);
    bufq_queue_data_in(inbuffer);
    /* for traffic log, replace unprintable chars (except newlines) with spaces */
    for (i = 7, j = 0; j < size; i++, j++) {
        if (!isprint((int)inbuffer[i]) && inbuffer[i] != '\n')
            inbuffer[i] = ' ';
    }
    bufq_queue_traffic_log(inbuffer);
    bufq_queue_debug_log("Data thread: received ARDOP FEC frame from TNC");
}

void ardop_data_on_idf(char *data, size_t size)
{
    char *s, *e, inbuffer[MIN_DATA_BUF_SIZE];

    snprintf(inbuffer, size + 8, ">> [I] %s", data);
    s = strstr(inbuffer, "ID:");
    if (s) {
        e = s;
        while (isprint((int)*e))
            ++e;
        *e = '\0';
        bufq_queue_data_in(inbuffer);
        bufq_queue_traffic_log(inbuffer);
        s += 3;
        while (*s && *s == ' ')
            ++s;
        e = s;
        while (*e && *e != ' ')
            ++e;
        *e = '\0';
        snprintf(inbuffer, sizeof(inbuffer), "8[I] %-10s ", s);
        bufq_queue_heard(inbuffer);
        bufq_queue_debug_log("Data thread: received ARDOP IDF frame from TNC");
    } else {
        /* this sent by tnc to host when SENDID invoked */
        snprintf(inbuffer, size + 8, "<< [I] %s", data);
        bufq_queue_data_in(inbuffer);
        bufq_queue_traffic_log(inbuffer);
    }
}

void ardop_data_on_arq(char *data, size_t size)
{
    char *s, *e, inbuffer[MIN_DATA_BUF_SIZE], remote_call[TNC_MYCALL_SIZE];
    int state;

    bufq_queue_debug_log("Data thread: received ARDOP ARQ frame from TNC");
    arim_copy_remote_call(remote_call, sizeof(remote_call));
    snprintf(inbuffer, sizeof(inbuffer), "9[@] %-10s ", remote_call);
    state = arim_get_state();
    switch(state) {
    case ST_ARQ_FLIST_RCV:
        arim_arq_files_flist_on_rcv_frame(data, size);
        break;
    case ST_ARQ_FILE_RCV:
        arim_arq_files_on_rcv_frame(data, size);
        break;
    case ST_ARQ_MSG_RCV:
        arim_arq_msg_on_rcv_frame(data, size);
        break;
    case ST_ARQ_CONNECTED:
    case ST_ARQ_FILE_RCV_WAIT:
    case ST_ARQ_FILE_RCV_WAIT_OK:
    case ST_ARQ_FILE_SEND_WAIT:
    case ST_ARQ_FILE_SEND_WAIT_OK:
    case ST_ARQ_FILE_SEND:
    case ST_ARQ_FLIST_RCV_WAIT:
    case ST_ARQ_FLIST_SEND_WAIT:
    case ST_ARQ_FLIST_SEND:
    case ST_ARQ_MSG_SEND_WAIT:
    case ST_ARQ_MSG_SEND:
    case ST_ARQ_AUTH_RCV_A2_WAIT:
    case ST_ARQ_AUTH_RCV_A3_WAIT:
    case ST_ARQ_AUTH_RCV_A4_WAIT:
    case ST_ARQ_AUTH_SEND_A1:
    case ST_ARQ_AUTH_SEND_A2:
    case ST_ARQ_AUTH_SEND_A3:
        arim_arq_on_data(data, size);
        break;
    default:
        /* special case, call sign is embedded in data,
           remove leading ':' character if present */
        snprintf(inbuffer, size + 8, ">> [@] %s", data);
        s = strstr(inbuffer, ":");
        if (s) {
            e = s;
            while (isprint((int)*e))
                ++e;
            *e = '\0';
            bufq_queue_data_in(inbuffer);
            bufq_queue_traffic_log(inbuffer);
            ++s;
            while (*s && *s == ' ')
                ++s;
            e = s;
            while (*e && *e != ' ')
                ++e;
            *e = '\0';
            snprintf(inbuffer, sizeof(inbuffer), "9[@] %-10s ", s);
        }
        break;
    }
    bufq_queue_heard(inbuffer);
}

void ardop_data_on_err(char *data, size_t size)
{
    char *p, inbuffer[MIN_DATA_BUF_SIZE];

    snprintf(inbuffer, size + 8, ">> [E] %s", data);
    p = inbuffer;
    while (isprint((int)*p))
        ++p;
    *p = '\0';
    bufq_queue_data_in(inbuffer);
    bufq_queue_traffic_log(inbuffer);
    bufq_queue_debug_log("Data thread: received ARDOP ERR frame from TNC");
}

//#define VIEW_DATA_IN
size_t ardop_data_handle_data(unsigned char *data, size_t size)
{
    static unsigned char buffer[MIN_DATA_BUF_SIZE];
    static size_t cnt = 0;
    static int arim_frame_type = 0;
    int is_new_frame, is_arim_frame, datasize = 0;

#ifdef VIEW_DATA_IN
char buf[MIN_DATA_BUF_SIZE];
#endif

    if ((cnt + size) > sizeof(buffer)) {
        /* too much data, can't be a valid ARIM payload */
        cnt = 0;
        return cnt;
    }
    ardop_data_inc_num_bytes_in(size);
    memcpy(buffer + cnt, data, size);
    cnt += size;
    /* extract data size */
    if (cnt < 5) /* not enough data yet, wait for more */
        return cnt;
    /* is this a valid frame? */
    if ((buffer[2] == 'A' && buffer[3] == 'R' && buffer[4] == 'Q') ||
        (buffer[2] == 'F' && buffer[3] == 'E' && buffer[4] == 'C') ||
        (buffer[2] == 'E' && buffer[3] == 'R' && buffer[4] == 'R') ||
        (buffer[2] == 'I' && buffer[3] == 'D' && buffer[4] == 'F')) {
        /* yes, extract payload size */
        datasize = buffer[0] << 8;
        datasize += buffer[1];
    }
    if (datasize <= 0) {
        /* invalid frame or bad payload size */
        bufq_queue_debug_log("Data thread: received bad ARDOP ARQ frame from TNC");
        cnt = 0;
        return cnt;
    }
    if (datasize <= (cnt - 2)) {
        /* got all data, dispatch on frame type */
#ifdef VIEW_DATA_IN
snprintf(buf, datasize + 7, "[%04X]%s", datasize, buffer + 2);
bufq_queue_debug_log(buf);
sleep(1);
#endif
        if (buffer[2] == 'F') { /* FEC frame */
            is_arim_frame = 0;
            is_arim_frame = arim_test_frame((char *)&buffer[5], datasize - 3);
            is_new_frame = 0;
            is_new_frame = (!arim_data_waiting && is_arim_frame);
            if (is_new_frame) {
                arim_frame_type = is_arim_frame;
                arim_on_event(EV_FRAME_START, arim_frame_type);
                bufq_queue_debug_log("Data thread: received start of ARIM frame");
            }
            if (arim_data_waiting || is_new_frame)
                arim_data_waiting = arim_on_data((char *)&buffer[5], datasize - 3);
            else
                ardop_data_on_fec((char *)&buffer[5], datasize - 3);
            /* clear start time if done, otherwise update with current time */
            if (!arim_data_waiting) {
                arim_start_time = 0;
                arim_on_event(EV_FRAME_END, arim_frame_type);
            } else {
                arim_start_time = time(NULL);
            }
        }
        else if (buffer[2] == 'I') /* IDF frame */
            ardop_data_on_idf((char *)&buffer[5], datasize - 3);
        else if (buffer[2] == 'A') /* ARQ frame */
            ardop_data_on_arq((char *)&buffer[5], datasize - 3);
        else if (buffer[2] == 'E') /* ERR frame */
            ardop_data_on_err((char *)&buffer[5], datasize - 3);
        /* reset buffer */
        cnt = 0;
    }
    return cnt;
}

