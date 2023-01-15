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
#include "bufq.h"
#include "util.h"
#include "log.h"
#include "ui.h"

CMDQUEUE g_cmd_in_q;
CMDQUEUE g_cmd_out_q;
DATAQUEUE g_data_in_q;
DATAQUEUE g_data_out_q;
DATAQUEUE g_traffic_log_q;
CMDQUEUE g_heard_q;
CMDQUEUE g_recents_q;
CMDQUEUE g_ptable_q;
CMDQUEUE g_ctable_q;
CMDQUEUE g_ftable_q;
CMDQUEUE g_debug_log_q;
CMDQUEUE g_tncpi9k6_log_q;
FILEQUEUE g_file_out_q;
MSGQUEUE g_msg_out_q;

void cmdq_init(CMDQUEUE *q)
{
    memset(q, 0, sizeof(CMDQUEUE));
}

int cmdq_get_size(CMDQUEUE *q)
{
    return q->size;
}

int cmdq_push(CMDQUEUE *q, const char *data)
{
    snprintf(q->data[q->head], sizeof(q->data[q->head]), "%s", data);
    if (++q->head == MAX_CMDQUEUE_LEN)
        q->head = 0;
    /* calculate size */
    q->size = q->head - q->tail;
    /* when head catches up to tail buffer holds MAX_CMDQUEUE_LEN elements */
    if (q->size <= 0)
        q->size += MAX_CMDQUEUE_LEN;
    return q->size;
}

char *cmdq_pop(CMDQUEUE *q)
{
    int p;

    if (!q->size)
        return NULL;
    p = q->tail;
    if (++q->tail >= MAX_CMDQUEUE_LEN)
        q->tail = 0;
    /* calculate size */
    q->size = q->head - q->tail;
    /* when tail catches up to head buffer holds 0 elements */
    if (q->size < 0)
        q->size += MAX_CMDQUEUE_LEN;
    return q->data[p];
}

void dataq_init(DATAQUEUE *q)
{
    memset(q, 0, sizeof(DATAQUEUE));
}

int dataq_get_size(DATAQUEUE *q)
{
    return q->size;
}

int dataq_push(DATAQUEUE *q, const char *data)
{
    snprintf(q->data[q->head], sizeof(q->data[q->head]), "%s", data);
    if (++q->head == MAX_DATAQUEUE_LEN)
        q->head = 0;
    /* calculate size */
    q->size = q->head - q->tail;
    /* when head catches up to tail buffer holds MAX_DATAQUEUE_LEN elements */
    if (q->size <= 0)
        q->size += MAX_DATAQUEUE_LEN;
    return q->size;
}

char *dataq_pop(DATAQUEUE *q)
{
    int p;

    if (!q->size)
        return NULL;
    p = q->tail;
    if (++q->tail >= MAX_DATAQUEUE_LEN)
        q->tail = 0;
    /* calculate size */
    q->size = q->head - q->tail;
    /* when tail catches up to head buffer holds 0 elements */
    if (q->size < 0)
        q->size = MAX_DATAQUEUE_LEN;
    return q->data[p];
}

void fileq_init(FILEQUEUE *q)
{
    memset(q, 0, sizeof(FILEQUEUE));
}

int fileq_get_size(FILEQUEUE *q)
{
    return q->size;
}

int fileq_push(FILEQUEUE *q, const FILEQUEUEITEM *data)
{
    memcpy(&q->data[q->head], data, sizeof(q->data[q->head]));
    if (++q->head == MAX_FILEQUEUE_LEN)
        q->head = 0;
    /* calculate size */
    q->size = q->head - q->tail;
    /* when head catches up to tail buffer holds MAX_FILEQUEUE_LEN elements */
    if (q->size <= 0)
        q->size += MAX_FILEQUEUE_LEN;
    return q->size;
}

FILEQUEUEITEM *fileq_pop(FILEQUEUE *q)
{
    int p;

    if (!q->size)
        return NULL;
    p = q->tail;
    if (++q->tail >= MAX_FILEQUEUE_LEN)
        q->tail = 0;
    /* calculate size */
    q->size = q->head - q->tail;
    /* when tail catches up to head buffer holds 0 elements */
    if (q->size < 0)
        q->size = MAX_FILEQUEUE_LEN;
    return &q->data[p];
}

void msgq_init(MSGQUEUE *q)
{
    memset(q, 0, sizeof(MSGQUEUE));
}

int msgq_get_size(MSGQUEUE *q)
{
    return q->size;
}

int msgq_push(MSGQUEUE *q, const MSGQUEUEITEM *data)
{
    memcpy(&q->data[q->head], data, sizeof(q->data[q->head]));
    if (++q->head == MAX_MSGQUEUE_LEN)
        q->head = 0;
    /* calculate size */
    q->size = q->head - q->tail;
    /* when head catches up to tail buffer holds MAX_FILEQUEUE_LEN elements */
    if (q->size <= 0)
        q->size += MAX_MSGQUEUE_LEN;
    return q->size;
}

MSGQUEUEITEM *msgq_pop(MSGQUEUE *q)
{
    int p;

    if (!q->size)
        return NULL;
    p = q->tail;
    if (++q->tail >= MAX_MSGQUEUE_LEN)
        q->tail = 0;
    /* calculate size */
    q->size = q->head - q->tail;
    /* when tail catches up to head buffer holds 0 elements */
    if (q->size < 0)
        q->size = MAX_MSGQUEUE_LEN;
    return &q->data[p];
}

void bufq_queue_heard(const char *text)
{
    pthread_mutex_lock(&mutex_heard);
    cmdq_push(&g_heard_q, text);
    pthread_mutex_unlock(&mutex_heard);
}

void bufq_queue_traffic_log(const char *text)
{
    char buffer[MIN_MSG_BUF_SIZE];
    char timestamp[MAX_TIMESTAMP_SIZE];

    if (g_traffic_log_enable) {
        snprintf(buffer, sizeof(buffer), "[%s] %s",
                util_timestamp(timestamp, sizeof(timestamp)), text);
        pthread_mutex_lock(&mutex_traffic_log);
        dataq_push(&g_traffic_log_q, buffer);
        pthread_mutex_unlock(&mutex_traffic_log);
    }
}

void bufq_queue_debug_log(const char *text)
{
    char buffer[MAX_CMD_SIZE];
    char timestamp[MAX_TIMESTAMP_SIZE];

    if (g_debug_log_enable) {
        snprintf(buffer, sizeof(buffer), "[%s] %s",
                util_timestamp_usec(timestamp, sizeof(timestamp)), text);
        pthread_mutex_lock(&mutex_debug_log);
        cmdq_push(&g_debug_log_q, buffer);
        pthread_mutex_unlock(&mutex_debug_log);
    }
}

void bufq_queue_tncpi9k6_log(const char *text)
{
    char buffer[MAX_CMD_SIZE];
    char timestamp[MAX_TIMESTAMP_SIZE];

    if (g_tncpi9k6_log_enable) {
        snprintf(buffer, sizeof(buffer), "[%s] %s",
                util_timestamp_usec(timestamp, sizeof(timestamp)), text);
        pthread_mutex_lock(&mutex_tncpi9k6_log);
        cmdq_push(&g_tncpi9k6_log_q, buffer);
        pthread_mutex_unlock(&mutex_tncpi9k6_log);
    }
}

void bufq_queue_cmd_in(const char *text)
{
    pthread_mutex_lock(&mutex_cmd_in);
    cmdq_push(&g_cmd_in_q, text);
    pthread_mutex_unlock(&mutex_cmd_in);
}

void bufq_queue_cmd_out(const char *text)
{
    char inbuffer[MAX_CMD_SIZE];

    snprintf(inbuffer, sizeof(inbuffer), "%s\r", text);
    pthread_mutex_lock(&mutex_cmd_out);
    cmdq_push(&g_cmd_out_q, text);
    pthread_mutex_unlock(&mutex_cmd_out);
}

void bufq_queue_data_in(const char *text)
{
    char buffer[MIN_MSG_BUF_SIZE+MAX_TIMESTAMP_SIZE];
    char timestamp[MAX_TIMESTAMP_SIZE];

    pthread_mutex_lock(&mutex_data_in);
    if (mon_timestamp) {
        snprintf(buffer, sizeof(buffer), "[%s] %s",
                util_timestamp(timestamp, sizeof(timestamp)), text);
        dataq_push(&g_data_in_q, buffer);
    } else {
        dataq_push(&g_data_in_q, text);
    }
    pthread_mutex_unlock(&mutex_data_in);
}

void bufq_queue_data_out(const char *text)
{
    pthread_mutex_lock(&mutex_data_out);
    dataq_push(&g_data_out_q, text);
    pthread_mutex_unlock(&mutex_data_out);
}

void bufq_queue_ptable(const char *text)
{
    pthread_mutex_lock(&mutex_ptable);
    cmdq_push(&g_ptable_q, text);
    pthread_mutex_unlock(&mutex_ptable);
}

void bufq_queue_ctable(const char *text)
{
    pthread_mutex_lock(&mutex_ctable);
    cmdq_push(&g_ctable_q, text);
    pthread_mutex_unlock(&mutex_ctable);
}

void bufq_queue_ftable(const char *text)
{
    pthread_mutex_lock(&mutex_ftable);
    cmdq_push(&g_ftable_q, text);
    pthread_mutex_unlock(&mutex_ftable);
}

