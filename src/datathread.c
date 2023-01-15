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

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include "main.h"
#include "datathread.h"
#include "arim.h"
#include "arim_proto.h"
#include "arim_arq.h"
#include "bufq.h"
#include "ardop_data.h"
#include "tnc_attach.h"

/* 10 second wait before next check of TNC's BUFFER count */
#define TNC_BUFFER_UPDATE_WAIT  50
#define TNC_DATA_BLOCK_SIZE     2048

static size_t data_send_nrem, file_send_nrem, msg_send_nrem, send_bytes_buffered;
static int data_send_nblk, data_send_timer;
static int file_send_nblk, file_send_timer;
static int msg_send_nblk, msg_send_timer;

size_t datathread_get_num_bytes_buffered()
{
    return send_bytes_buffered;
}

void datathread_cancel_send_data_out()
{
    /* reset TNC transmit data buffering state */
    data_send_nrem = file_send_nrem = msg_send_nrem = send_bytes_buffered = 0;
    data_send_nblk = file_send_nblk = msg_send_nblk = 0;
    data_send_timer = file_send_timer = msg_send_timer = 0;
}

void datathread_send_file_out(int sock)
{
    static FILEQUEUEITEM *item;
    static char *p, buffer[MAX_FILE_SIZE+4];
    static unsigned char *s;
    size_t sent;

    if (msg_send_nblk || msg_send_nrem || data_send_nblk || data_send_nrem)
        return;
    if (!file_send_nblk && !file_send_nrem) {
        pthread_mutex_lock(&mutex_file_out);
        item = fileq_pop(&g_file_out_q);
        pthread_mutex_unlock(&mutex_file_out);
        if (!item)
            return;
        bufq_queue_debug_log("Data thread: sending file to TNC");
        p = buffer;
        s = item->data;
        file_send_nblk = item->size / TNC_DATA_BLOCK_SIZE;
        file_send_nrem = item->size % TNC_DATA_BLOCK_SIZE;
        send_bytes_buffered = 0;
        file_send_timer = 0;
    }
    /*  BUFFER notifications from TNC are several seconds apart when transmitting.
        Don't proceed until timer has expired to save the overhead of checking
        the BUFFER value every time function is called (multiple times per sec). */
    if (file_send_timer && --file_send_timer > 0)
        return;
    if (file_send_nblk) {
        if (arim_get_buffer_cnt() >= TNC_DATA_BLOCK_SIZE) {
            file_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
            return;
        }
        bufq_queue_debug_log("Data thread: writing block of data to socket");
        *p++ = (TNC_DATA_BLOCK_SIZE >> 8) & 0xFF;
        *p++ = TNC_DATA_BLOCK_SIZE & 0xFF;
        memcpy(p, s, TNC_DATA_BLOCK_SIZE);
        sent = write(sock, buffer, TNC_DATA_BLOCK_SIZE + 2);
        if (sent < 0) {
            bufq_queue_debug_log("Data thread: write to socket failed");
            datathread_cancel_send_data_out();
            return;
        }
        s += TNC_DATA_BLOCK_SIZE;
        p = buffer;
        ardop_data_inc_num_bytes_out(TNC_DATA_BLOCK_SIZE);
        send_bytes_buffered += TNC_DATA_BLOCK_SIZE;
        --file_send_nblk;
        file_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
        return;
    }
    if (!file_send_nblk && file_send_nrem) {
        if (arim_get_buffer_cnt() >= TNC_DATA_BLOCK_SIZE) {
            file_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
            return;
        }
        bufq_queue_debug_log("Data thread: writing remainder of data to socket");
        *p++ = (file_send_nrem >> 8) & 0xFF;
        *p++ = file_send_nrem & 0xFF;
        memcpy(p, s, file_send_nrem);
        sent = write(sock, buffer, file_send_nrem + 2);
        if (sent < 0) {
            bufq_queue_debug_log("Data thread: write to socket failed");
            datathread_cancel_send_data_out();
            return;
        }
        ardop_data_inc_num_bytes_out(file_send_nrem + 2);
        send_bytes_buffered += file_send_nrem;
        file_send_nrem = 0;
    }
}

void datathread_send_msg_out(int sock)
{
    static MSGQUEUEITEM *item;
    static char *p, *s, buffer[MIN_MSG_BUF_SIZE];
    size_t sent;

    if (file_send_nblk || file_send_nrem || data_send_nblk || data_send_nrem)
        return;
    if (!msg_send_nblk && !msg_send_nrem) {
        pthread_mutex_lock(&mutex_msg_out);
        item = msgq_pop(&g_msg_out_q);
        pthread_mutex_unlock(&mutex_msg_out);
        if (!item)
            return;
        bufq_queue_debug_log("Data thread: sending message to TNC");
        p = buffer;
        s = item->data;
        msg_send_nblk = item->size / TNC_DATA_BLOCK_SIZE;
        msg_send_nrem = item->size % TNC_DATA_BLOCK_SIZE;
        send_bytes_buffered = 0;
        msg_send_timer = 0;
    }
    /*  BUFFER notifications from TNC are several seconds apart when transmitting.
        Don't proceed until timer has expired to save the overhead of checking
        the BUFFER value every time function is called (multiple times per sec). */
    if (msg_send_timer && --msg_send_timer > 0)
        return;
    if (msg_send_nblk) {
        if (arim_get_buffer_cnt() >= TNC_DATA_BLOCK_SIZE) {
            msg_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
            return;
        }
        bufq_queue_debug_log("Data thread: writing block of data to socket");
        *p++ = (TNC_DATA_BLOCK_SIZE >> 8) & 0xFF;
        *p++ = TNC_DATA_BLOCK_SIZE & 0xFF;
        memcpy(p, s, TNC_DATA_BLOCK_SIZE);
        sent = write(sock, buffer, TNC_DATA_BLOCK_SIZE + 2);
        if (sent < 0) {
            bufq_queue_debug_log("Data thread: write to socket failed");
            datathread_cancel_send_data_out();
            return;
        }
        s += TNC_DATA_BLOCK_SIZE;
        p = buffer;
        ardop_data_inc_num_bytes_out(TNC_DATA_BLOCK_SIZE);
        send_bytes_buffered += TNC_DATA_BLOCK_SIZE;
        --msg_send_nblk;
        msg_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
        return;
    }
    if (!msg_send_nblk && msg_send_nrem) {
        if (arim_get_buffer_cnt() >= TNC_DATA_BLOCK_SIZE) {
            msg_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
            return;
        }
        bufq_queue_debug_log("Data thread: writing remainder of data to socket");
        *p++ = (msg_send_nrem >> 8) & 0xFF;
        *p++ = msg_send_nrem & 0xFF;
        memcpy(p, s, msg_send_nrem);
        sent = write(sock, buffer, msg_send_nrem + 2);
        if (sent < 0) {
            bufq_queue_debug_log("Data thread: write to socket failed");
            datathread_cancel_send_data_out();
            return;
        }
        ardop_data_inc_num_bytes_out(msg_send_nrem + 2);
        send_bytes_buffered += msg_send_nrem;
        msg_send_nrem = 0;
    }
}

void datathread_send_data_out(int sock)
{
    static char *p, *s, *data, buffer[MIN_DATA_BUF_SIZE];
    size_t len, sent;

    if (file_send_nblk || file_send_nrem || msg_send_nblk || msg_send_nrem)
        return;
    if (!data_send_nblk && !data_send_nrem) {
        pthread_mutex_lock(&mutex_data_out);
        data = dataq_pop(&g_data_out_q);
        pthread_mutex_unlock(&mutex_data_out);
        if (!data)
            return;
        bufq_queue_debug_log("Data thread: sending data to TNC");
        len = strlen(data);
        if (arim_test_frame(data, len))
            snprintf(buffer, sizeof(buffer), "<< [%c] %s", data[1], data);
        else if (arim_is_arq_state())
            snprintf(buffer, sizeof(buffer), "<< [@] %s", data);
        else
            snprintf(buffer, sizeof(buffer), "<< [U] %s", data);
        bufq_queue_data_in(buffer);
        bufq_queue_traffic_log(buffer);
        p = buffer;
        s = data;
        data_send_nblk = len / TNC_DATA_BLOCK_SIZE;
        data_send_nrem = len % TNC_DATA_BLOCK_SIZE;
        send_bytes_buffered = 0;
        data_send_timer = 0;
    }
    if (data_send_timer && --data_send_timer > 0)
        return;
    if (data_send_nblk) {
        if (arim_get_buffer_cnt() >= TNC_DATA_BLOCK_SIZE) {
            data_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
            return;
        }
        bufq_queue_debug_log("Data thread: writing block of data to socket");
        *p++ = (TNC_DATA_BLOCK_SIZE >> 8) & 0xFF;
        *p++ = TNC_DATA_BLOCK_SIZE & 0xFF;
        memcpy(p, s, TNC_DATA_BLOCK_SIZE);
        sent = write(sock, buffer, TNC_DATA_BLOCK_SIZE + 2);
        if (sent < 0) {
            bufq_queue_debug_log("Data thread: write to socket failed");
            datathread_cancel_send_data_out();
            return;
        }
        s += TNC_DATA_BLOCK_SIZE;
        p = buffer;
        ardop_data_inc_num_bytes_out(TNC_DATA_BLOCK_SIZE);
        send_bytes_buffered += TNC_DATA_BLOCK_SIZE;
        --data_send_nblk;
        if (!arim_is_arq_state())
            bufq_queue_cmd_out("FECSEND TRUE");
        data_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
        return;
    }
    if (!data_send_nblk && data_send_nrem) {
        if (arim_get_buffer_cnt() >= TNC_DATA_BLOCK_SIZE) {
            data_send_timer = TNC_BUFFER_UPDATE_WAIT; /* wait for next BUFFER notification */
            return;
        }
        bufq_queue_debug_log("Data thread: writing remainder of data to socket");
        *p++ = (data_send_nrem >> 8) & 0xFF;
        *p++ = data_send_nrem & 0xFF;
        memcpy(p, s, data_send_nrem);
        sent = write(sock, buffer, data_send_nrem + 2);
        if (sent < 0) {
            bufq_queue_debug_log("Data thread: write to socket failed");
            datathread_cancel_send_data_out();
            return;
        }
        ardop_data_inc_num_bytes_out(data_send_nrem + 2);
        send_bytes_buffered += data_send_nrem;
        data_send_nrem = 0;
        if (!arim_is_arq_state())
            bufq_queue_cmd_out("FECSEND TRUE");
    }
}

void *datathread_func(void *data)
{
    unsigned char buffer[MIN_DATA_BUF_SIZE];
    struct addrinfo hints, *res = NULL;
    fd_set datareadfds, dataerrorfds;
    struct timeval timeout;
    ssize_t rsize;
    int result, portnum, datasock, arim_timeout;
    time_t cur_time;

    memset(&hints, 0, sizeof hints);
    bufq_queue_debug_log("Data thread: initializing");
    hints.ai_family = AF_UNSPEC;  /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    portnum = atoi(g_tnc_settings[g_cur_tnc].port) + 1;
    snprintf((char *)buffer, sizeof(buffer), "%d", portnum);
    getaddrinfo(g_tnc_settings[g_cur_tnc].ipaddr, (char *)buffer, &hints, &res);
    if (!res)
    {
        bufq_queue_debug_log("Data thread: failed to resolve IP address");
        g_datathread_stop = 1;
        pthread_exit(data);
    }
    datasock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(datasock, res->ai_addr, res->ai_addrlen) == -1) {
        bufq_queue_debug_log("Data thread: failed to open TCP socket");
        g_datathread_stop = 1;
        pthread_exit(data);
    }
    freeaddrinfo(res);
    g_datathread_ready = 1;
    /* timeout specified in secs */
    arim_timeout = atoi(g_arim_settings.frame_timeout);
    arim_reset();
    while (1) {
        FD_ZERO(&datareadfds);
        FD_ZERO(&dataerrorfds);
        FD_SET(datasock, &datareadfds);
        FD_SET(datasock, &dataerrorfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        result = select(datasock + 1, &datareadfds, (fd_set *)0, &dataerrorfds, &timeout);
        switch (result) {
        case 0:
            /* select timeout */
            arim_on_event(EV_PERIODIC, 0);
            datathread_send_data_out(datasock);
            datathread_send_file_out(datasock);
            datathread_send_msg_out(datasock);
            if (arim_data_waiting) {
                cur_time = time(NULL);
                if (cur_time - arim_start_time > arim_timeout) {
                    /* timeout, reset arim state */
                    arim_reset();
                    arim_data_waiting = arim_start_time = 0;
                    bufq_queue_debug_log("Data thread: ARIM frame time out");
                    arim_on_event(EV_FRAME_TO, 0);
                }
            }
            /* pump outbound and inbound arq line queues */
            arim_arq_on_cmd(NULL, 0);
            arim_arq_on_resp(NULL, 0);
            break;
        case -1:
            bufq_queue_debug_log("Data thread: Socket select error (-1)");
            break;
        default:
            if (FD_ISSET(datasock, &datareadfds)) {
                rsize = read(datasock, buffer, sizeof(buffer) - 1);
                if (rsize == 0) {
                    bufq_queue_debug_log("Data thread: Socket closed by TNC");
                    tnc_detach(); /* close TCP connection to TNC */
                } else if (rsize == -1) {
                    bufq_queue_debug_log("Data thread: Socket read error (-1)");
                } else {
                    ardop_data_handle_data(buffer, rsize);
                }
            }
            if (FD_ISSET(datasock, &dataerrorfds)) {
                bufq_queue_debug_log("Data thread: Socket select error (FD_ISSET)");
                break;
            }
        }
        if (g_datathread_stop) {
            break;
        }
    }
    bufq_queue_debug_log("Data thread: terminating");
    sleep(2);
    close(datasock);
    return data;
}

