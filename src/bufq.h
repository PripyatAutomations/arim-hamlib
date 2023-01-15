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

#ifndef _BUFQ_H_INCLUDED_
#define _BUFQ_H_INCLUDED_

#define MAX_DATAQUEUE_LEN     128
#define MAX_CMDQUEUE_LEN      128
#define MAX_FILEQUEUE_LEN     4
#define MAX_MSGQUEUE_LEN      4

#include "main.h"

typedef struct data_q {
    int head, tail;
    int size;
    char data[MAX_DATAQUEUE_LEN][MIN_DATA_BUF_SIZE];
} DATAQUEUE;

typedef struct cmd_q {
    int head, tail;
    int size;
    char data[MAX_CMDQUEUE_LEN][MAX_CMD_SIZE];
} CMDQUEUE;

typedef struct fileq_item {
    size_t size;
    unsigned int check;
    char name[MAX_DIR_PATH_SIZE];
    char path[MAX_DIR_PATH_SIZE];
    unsigned char data[MAX_FILE_SIZE];
} FILEQUEUEITEM;

typedef struct file_q {
    int head, tail;
    int size;
    FILEQUEUEITEM data[MAX_FILEQUEUE_LEN];
} FILEQUEUE;

typedef struct msgq_item {
    size_t size;
    unsigned int check;
    char call[MAX_CALLSIGN_SIZE];
    char data[MIN_MSG_BUF_SIZE];
} MSGQUEUEITEM;

typedef struct msg_q {
    int head, tail;
    int size;
    MSGQUEUEITEM data[MAX_MSGQUEUE_LEN];
} MSGQUEUE;

extern CMDQUEUE g_cmd_in_q;
extern CMDQUEUE g_cmd_out_q;
extern DATAQUEUE g_data_in_q;
extern DATAQUEUE g_data_out_q;
extern DATAQUEUE g_traffic_log_q;
extern CMDQUEUE g_heard_q;
extern CMDQUEUE g_recents_q;
extern CMDQUEUE g_ptable_q;
extern CMDQUEUE g_ctable_q;
extern CMDQUEUE g_ftable_q;
extern CMDQUEUE g_debug_log_q;
extern CMDQUEUE g_tncpi9k6_log_q;
extern FILEQUEUE g_file_out_q;
extern MSGQUEUE g_msg_out_q;

extern void cmdq_init(CMDQUEUE *q);
extern int cmdq_get_size(CMDQUEUE *q);
extern int cmdq_push(CMDQUEUE *q, const char *data);
extern char *cmdq_pop(CMDQUEUE *q);

extern void dataq_init(DATAQUEUE *q);
extern int dataq_get_size(DATAQUEUE *q);
extern int dataq_push(DATAQUEUE *q, const char *data);
extern char *dataq_pop(DATAQUEUE *q);

extern void msgq_init(MSGQUEUE *q);
extern int msgq_get_size(MSGQUEUE *q);
extern int msgq_push(MSGQUEUE *q, const MSGQUEUEITEM *data);
extern MSGQUEUEITEM *msgq_pop(MSGQUEUE *q);

extern void fileq_init(FILEQUEUE *q);
extern int fileq_get_size(FILEQUEUE *q);
extern int fileq_push(FILEQUEUE *q, const FILEQUEUEITEM *data);
extern FILEQUEUEITEM *fileq_pop(FILEQUEUE *q);

extern void bufq_queue_heard(const char *text);
extern void bufq_queue_traffic_log(const char *text);
extern void bufq_queue_debug_log(const char *text);
extern void bufq_queue_tncpi9k6_log(const char *text);
extern void bufq_queue_cmd_in(const char *text);
extern void bufq_queue_cmd_out(const char *text);
extern void bufq_queue_data_in(const char *text);
extern void bufq_queue_data_out(const char *text);
extern void bufq_queue_ptable(const char *text);
extern void bufq_queue_ctable(const char *text);
extern void bufq_queue_ftable(const char *text);

#endif

