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

#ifndef _MAIN_H_INCLUDED_
#define _MAIN_H_INCLUDED_

#include <pthread.h>
#include <signal.h>
// Added hamlib - 20230115
#include <hamlib/rig.h>

#define ARIM_PROTO_VERSION     (1)
#define MAX_ARIM_HDR_SIZE      64
#define MAX_CMD_SIZE           256
#define MAX_DATA_SIZE          16384
#define MAX_UNCOMP_DATA_SIZE   (MAX_DATA_SIZE*5)
#define MIN_DATA_BUF_SIZE      (MAX_DATA_SIZE+256)
#define MAX_MSG_SIZE           MAX_DATA_SIZE
#define MIN_MSG_BUF_SIZE       (MAX_ARIM_HDR_SIZE+MAX_MSG_SIZE+1)
#define MAX_MSG_LINE_SIZE      1024
#define MAX_FILE_SIZE          MAX_DATA_SIZE
#define MAX_BEACON_SIZE        128
#define MIN_BEACON_BUF_SIZE    (MAX_ARIM_HDR_SIZE+MAX_BEACON_SIZE+1)
#define MAX_HEARD_SIZE         32
#define MAX_HEARD_LIST_LEN     64
#define MAX_RECENTS_LIST_LEN   32
#define MAX_PTABLE_LIST_LEN    32
#define MAX_PTABLE_ROW_SIZE    128
#define MAX_CTABLE_LIST_LEN    64
#define MAX_CTABLE_ROW_SIZE    128
#define MAX_FTABLE_LIST_LEN    64
#define MAX_FTABLE_ROW_SIZE    128
#define MAX_MBOX_HDR_SIZE      80
#define MAX_MBOX_LIST_LEN      256
#define MAX_CHECK_SIZE         8
#define MAX_LOG_LINE_SIZE      MAX_CMD_SIZE
#define MAX_TITLE_STATUS_SIZE  60
#define MAX_TITLE_SIZE         80
#define MAX_STATUS_BAR_SIZE    128
#define MAX_STATUS_IND_SIZE    32
#define MAX_CALLSIGN_SIZE      12
#define MAX_DIR_LINE_SIZE      256
#define MAX_DIR_LIST_LEN       256
#define MAX_FILE_NAME_SIZE     256
#define MAX_DIR_PATH_SIZE      768
#define MAX_PATH_SIZE          2048
#define MAX_PING_SIZE          64
#define MAX_METHOD_SIZE        32

#define ALARM_INTERVAL_SEC         6
#define ARDOP_BCN_SEND_TIMEOUT     90
#define ARDOP_PINGACK_TIMEOUT      5
#define ARDOP_CONNREQ_TIMEOUT      90
#define ARDOP_CONN_TIMEOUT         180
#define ARDOP_CONN_SEND_TIMEOUT    120
#define ARDOP_OUT_CONN_RPT_TIMEOUT 5

#define MBOX_TYPE_IN           0
#define MBOX_TYPE_OUT          1
#define MBOX_TYPE_SENT         2

#define DEFAULT_DIGEST_FNAME   "arim-digest"
#define DEFAULT_THEMES_FNAME   "arim-themes"
#define DEFAULT_INI_FNAME      "arim.ini"
#define DEFAULT_FILE_FNAME     "test.txt"
#define DEFAULT_DOWNLOAD_DIR   "download"
#define DEFAULT_PDF_HELP_FNAME "arim-help.pdf"
#define MBOX_INBOX_FNAME       "in.mbox"
#define MBOX_OUTBOX_FNAME      "out.mbox"
#define MBOX_SENTBOX_FNAME     "sent.mbox"

extern pthread_t g_cmdthread;
extern pthread_t g_datathread;
extern pthread_t g_serialthread;
extern int g_cmdthread_stop;
extern int g_cmdthread_ready;
extern int g_datathread_stop;
extern int g_datathread_ready;
extern int g_serialthread_stop;
extern int g_serialthread_ready;
extern int g_timerthread_stop;
extern int g_tnc_attached;
extern int g_win_changed;
extern int g_new_install;
extern int g_print_config;

extern pthread_mutex_t mutex_title;
extern pthread_mutex_t mutex_status;
extern pthread_mutex_t mutex_cmd_in;
extern pthread_mutex_t mutex_cmd_out;
extern pthread_mutex_t mutex_data_in;
extern pthread_mutex_t mutex_data_out;
extern pthread_mutex_t mutex_heard;
extern pthread_mutex_t mutex_debug_log;
extern pthread_mutex_t mutex_tncpi9k6_log;
extern pthread_mutex_t mutex_df_error_log;
extern pthread_mutex_t mutex_traffic_log;
extern pthread_mutex_t mutex_recents;
extern pthread_mutex_t mutex_ptable;
extern pthread_mutex_t mutex_ctable;
extern pthread_mutex_t mutex_ftable;
extern pthread_mutex_t mutex_time;
extern pthread_mutex_t mutex_tnc_set;
extern pthread_mutex_t mutex_file_out;
extern pthread_mutex_t mutex_msg_out;
extern pthread_mutex_t mutex_tnc_busy;
extern pthread_mutex_t mutex_num_bytes;

#endif

