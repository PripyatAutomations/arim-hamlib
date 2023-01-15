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

#ifndef _INI_H_INCLUDED_
#define _INI_H_INCLUDED_

#include "main.h"

#define TNC_IPADDR_SIZE          256
#define TNC_PORT_SIZE            8
#define TNC_MYCALL_SIZE          12
#define TNC_NETCALL_SIZE         12
#define TNC_GRIDSQ_SIZE          12
#define TNC_AUXCALLS_SIZE        128
#define TNC_VERSION_SIZE         32
#define TNC_NAME_SIZE            32
#define TNC_INFO_SIZE            128
#define TNC_FECMODE_SIZE         24
#define TNC_FECID_SIZE           8
#define TNC_FECREPEATS_SIZE      4
#define TNC_LEADER_SIZE          4
#define TNC_TRAILER_SIZE         4
#define TNC_SQUELCH_SIZE         4
#define TNC_BUSYDET_SIZE         4
#define TNC_BUSY_SIZE            8
#define TNC_BTIME_SIZE           4
#define TNC_VERSION_SIZE         32
#define TNC_BUFFER_SIZE          12
#define TNC_STATE_SIZE           12
#define TNC_LISTEN_SIZE          8
#define TNC_EN_PINGACK_SIZE      8
#define TNC_ARQ_BW_SIZE          16
#define TNC_ARQ_TO_SIZE          8
#define TNC_ARQ_SENDCR_SIZE      8
#define TNC_RESET_BT_TX_SIZE     8
#define TNC_NEGOTIATE_BW_SIZE    8
#define TNC_INIT_CMDS_MAX_CNT    32
#define TNC_INIT_CMD_SIZE        128
#define TNC_INTERFACE_SIZE       12
#define TNC_SERIAL_PORT_SIZE     64
#define TNC_SERIAL_BAUD_SIZE     16
#define TNC_DEBUG_EN_SIZE        8
#define TNC_TRAFFIC_EN_SIZE      8
#define TNC_TNCPI9K6_EN_SIZE     8

#define TNC_MAX_COUNT            10
#define TNC_NETCALL_MAX_CNT      8

#define DEFAULT_TNC_IPADDR       "127.0.0.1"
#define DEFAULT_TNC_PORT         "8515"
#define DEFAULT_TNC_GRIDSQ       "DM65"
#define DEFAULT_TNC_MYCALL       "NOCALL"
#define DEFAULT_TNC_NETCALL      "QST"
#define DEFAULT_TNC_FECMODE      "4PSK.200.50"
#define DEFAULT_TNC_FECID        "FALSE"
#define DEFAULT_TNC_FECREPEATS   "0"
#define DEFAULT_TNC_LEADER       "240"
#define DEFAULT_TNC_TRAILER      "0"
#define DEFAULT_TNC_SQUELCH      "5"
#define DEFAULT_TNC_BUSYDET      "5"
#define DEFAULT_TNC_BUSY         "FALSE"
#define DEFAULT_TNC_BTIME        "0"
#define DEFAULT_TNC_STATE        "DISC"
#define DEFAULT_TNC_LISTEN       "TRUE"
#define DEFAULT_TNC_EN_PINGACK   "TRUE"
#define DEFAULT_TNC_ARQ_BW       "500"
#define DEFAULT_TNC_ARQ_TO       "120"
#define DEFAULT_TNC_ARQ_SENDCR   "TRUE"
#define DEFAULT_TNC_RESET_BT_TX  "FALSE"
#define DEFAULT_TNC_NEGOTIATE_BW "TRUE"
#define DEFAULT_TNC_INTERFACE    "TCP"
#define DEFAULT_TNC_SERIAL_PORT  "/dev/serial0"
#define DEFAULT_TNC_SERIAL_BAUD  "115200"
#define DEFAULT_TNC_DEBUG_EN     "FALSE"
#define DEFAULT_TNC_TRAFFIC_EN   "FALSE"
#define DEFAULT_TNC_TNCPI9K6_EN  "FALSE"

#define MAX_TNC_GRIDSQ_STRLEN    8
#define MAX_TNC_NETCALL_STRLEN   10
#define MAX_TNC_MYCALL_STRLEN    10
#define MAX_TNC_BTIME_VALUE      999
#define MAX_TNC_FECREPEATS_VALUE 5
#define MIN_TNC_LEADER_VALUE     120
#define MAX_TNC_LEADER_VALUE     2500
#define MAX_TNC_TRAILER_VALUE    200
#define MIN_TNC_SQUELCH_VALUE    1
#define MAX_TNC_SQUELCH_VALUE    10
#define MIN_TNC_BUSYDET_VALUE    0
#define MAX_TNC_BUSYDET_VALUE    10
#define MIN_TNC_ARQ_TO           30
#define MAX_TNC_ARQ_TO           600

typedef struct tnc_set {
    char ipaddr[TNC_IPADDR_SIZE];
    char port[TNC_PORT_SIZE];
    char mycall[TNC_MYCALL_SIZE];
    char netcall[TNC_NETCALL_MAX_CNT+1][TNC_NETCALL_SIZE];
    int  netcall_cnt;
    char gridsq[TNC_GRIDSQ_SIZE];
    char btime[TNC_BTIME_SIZE];
    char version[TNC_VERSION_SIZE];
    char buffer[TNC_BUFFER_SIZE];
    char name[TNC_NAME_SIZE];
    char info[TNC_INFO_SIZE];
    char fecmode[TNC_FECMODE_SIZE];
    char fecrepeats[TNC_FECREPEATS_SIZE];
    char fecid[TNC_FECID_SIZE];
    char leader[TNC_LEADER_SIZE];
    char trailer[TNC_TRAILER_SIZE];
    char squelch[TNC_SQUELCH_SIZE];
    char busydet[TNC_BUSYDET_SIZE];
    char busy[TNC_BUSY_SIZE];
    char state[TNC_STATE_SIZE];
    char arq_target_call[TNC_MYCALL_SIZE];
    char arq_remote_call[TNC_MYCALL_SIZE];
    char arq_remote_gridsq[TNC_GRIDSQ_SIZE];
    char arq_bandwidth[TNC_ARQ_BW_SIZE];
    char arq_bandwidth_hz[TNC_ARQ_BW_SIZE];
    char arq_negotiate_bw[TNC_NEGOTIATE_BW_SIZE];
    char arq_timeout[TNC_ARQ_TO_SIZE];
    char arq_sendcr[TNC_ARQ_SENDCR_SIZE];
    char listen[TNC_LISTEN_SIZE];
    char tmp_listen[TNC_LISTEN_SIZE];
    char en_pingack[TNC_EN_PINGACK_SIZE];
    char reset_btime_tx[TNC_RESET_BT_TX_SIZE];
    char interface[TNC_INTERFACE_SIZE];
    char serial_port[TNC_SERIAL_PORT_SIZE];
    char serial_baudrate[TNC_SERIAL_BAUD_SIZE];
    char log_dir[MAX_DIR_PATH_SIZE];
    char debug_en[TNC_DEBUG_EN_SIZE];
    char traffic_en[TNC_TRAFFIC_EN_SIZE];
    char tncpi9k6_en[TNC_TNCPI9K6_EN_SIZE];
    char tnc_init_cmds[TNC_INIT_CMDS_MAX_CNT][TNC_INIT_CMD_SIZE];
    int tnc_init_cmds_cnt;
    // Added hamlib - 20230114
    int hamlib_model;
    RIG *hamlib_rig;
} TNC_SET;

extern TNC_SET g_tnc_settings[];
extern int g_cur_tnc;
extern int g_num_tnc;

#define ARIM_MYCALL_SIZE             12
#define ARIM_SEND_REPEATS_SIZE       4
#define ARIM_PILOT_PING_SIZE         4
#define ARIM_PILOT_PING_THR_SIZE     4
#define ARIM_ACK_TIMEOUT_SIZE        4
#define ARIM_FRAME_TIMEOUT_SIZE      4
#define ARIM_FILES_MAX_SIZE          12
#define ARIM_DYN_FILES_MAX_CNT       16
#define ARIM_DYN_FILES_SIZE          128
#define ARIM_ADD_FILES_DIR_MAX_CNT   16
#define ARIM_AC_FILES_DIR_MAX_CNT    16
#define ARIM_FECMODE_DOWN_SIZE       8
#define ARIM_MAX_MSG_DAYS_SIZE       8
#define ARIM_MSG_TRACE_EN_SIZE       8
#define ARIM_AC_LIST_MAX_CNT         512
#define DEFAULT_ARIM_MYCALL          "NOCALL"
#define DEFAULT_ARIM_SEND_REPEATS    "0"
#define DEFAULT_ARIM_PILOT_PING      "0"
#define DEFAULT_ARIM_PILOT_PING_THR  "60"
#define DEFAULT_ARIM_ACK_TIMEOUT     "30"
#define DEFAULT_ARIM_FRAME_TIMEOUT   "30"
#define DEFAULT_ARIM_FILES_DIR       "files/"
#define DEFAULT_ARIM_FILES_MAX_SIZE  "4096"
#define DEFAULT_ARIM_FECMODE_DOWN    "FALSE"
#define DEFAULT_ARIM_MSG_MAX_DAYS    "0"
#define DEFAULT_ARIM_MSG_TRACE_EN    "FALSE"

#define MAX_ARIM_SEND_REPEATS        5
#define MIN_ARIM_PILOT_PING          2
#define MAX_ARIM_PILOT_PING          5
#define MIN_ARIM_PILOT_PING_THR      50
#define MAX_ARIM_PILOT_PING_THR      100
#define MIN_ARIM_ACK_TIMEOUT         10
#define MAX_ARIM_ACK_TIMEOUT         999
#define MIN_ARIM_FRAME_TIMEOUT       10
#define MAX_ARIM_FRAME_TIMEOUT       999
#define MIN_ARIM_MSG_DAYS            0
#define MAX_ARIM_MSG_DAYS            9999

// default to using rigctld
#define	DEFAULT_HAMLIB_MODEL		2
// default to using flrig
//#define DEFAULT_HAMLIB_MODEL		4

typedef struct arim_set {
    char mycall[ARIM_MYCALL_SIZE];
    char send_repeats[ARIM_SEND_REPEATS_SIZE];
    char pilot_ping[ARIM_PILOT_PING_SIZE];
    char pilot_ping_thr[ARIM_PILOT_PING_THR_SIZE];
    char fecmode_downshift[ARIM_FECMODE_DOWN_SIZE];
    char ack_timeout[ARIM_ACK_TIMEOUT_SIZE];
    char frame_timeout[ARIM_FRAME_TIMEOUT_SIZE];
    char files_dir[MAX_DIR_PATH_SIZE];
    char max_file_size[ARIM_FILES_MAX_SIZE];
    char max_msg_days[ARIM_MAX_MSG_DAYS_SIZE];
    char msg_trace_en[ARIM_MSG_TRACE_EN_SIZE];
    char dyn_files[ARIM_DYN_FILES_MAX_CNT][ARIM_DYN_FILES_SIZE];
    int dyn_files_cnt;
    char add_files_dir[ARIM_ADD_FILES_DIR_MAX_CNT][MAX_DIR_PATH_SIZE];
    int add_files_dir_cnt;
    char ac_files_dir[ARIM_AC_FILES_DIR_MAX_CNT][MAX_DIR_PATH_SIZE];
    int ac_files_dir_cnt;
    char ac_allow_calls[ARIM_AC_LIST_MAX_CNT+1][TNC_MYCALL_SIZE];
    int ac_allow_calls_cnt;
    char ac_deny_calls[ARIM_AC_LIST_MAX_CNT+1][TNC_MYCALL_SIZE];
    int ac_deny_calls_cnt;
} ARIM_SET;

extern ARIM_SET g_arim_settings;

#define LOG_DEBUG_EN_SIZE           8
#define LOG_TRAFFIC_EN_SIZE         8
#define LOG_TNCPI9K6_EN_SIZE        8

#define DEFAULT_LOG_DEBUG_EN        "FALSE"
#define DEFAULT_LOG_TRAFFIC_EN      "TRUE"
#define DEFAULT_LOG_TNCPI9K6_EN     "FALSE"

typedef struct log_set {
    char debug_en[LOG_DEBUG_EN_SIZE];
    char traffic_en[LOG_TRAFFIC_EN_SIZE];
    char tncpi9k6_en[LOG_TRAFFIC_EN_SIZE];
} LOG_SET;

extern LOG_SET g_log_settings;

#define UI_SHOW_TITLES_SIZE         8
#define UI_LAST_TIME_HEARD_SIZE     12
#define UI_MON_TIMESTAMP_SIZE       8
#define UI_COLOR_CODE_SIZE          8
#define UI_UTC_TIME_SIZE            8
#define UI_THEME_SIZE               16

#define DEFAULT_UI_SHOW_TITLES      "TRUE"
#define DEFAULT_UI_LAST_TIME_HEARD  "CLOCK"
#define DEFAULT_UI_MON_TIMESTAMP    "FALSE"
#define DEFAULT_UI_COLOR_CODE       "TRUE"
#define DEFAULT_UI_UTC_TIME         "TRUE"
#define DEFAULT_UI_THEME            "DARK"

typedef struct ui_set {
    char show_titles[UI_SHOW_TITLES_SIZE];
    char last_time_heard[UI_LAST_TIME_HEARD_SIZE];
    char mon_timestamp[UI_MON_TIMESTAMP_SIZE];
    char color_code[UI_COLOR_CODE_SIZE];
    char utc_time[UI_UTC_TIME_SIZE];
    char theme[UI_THEME_SIZE];
} UI_SET;

extern UI_SET g_ui_settings;

extern int ini_read_settings(void);
extern int ini_validate_mycall(const char *call);
extern int ini_validate_netcall(const char *call);
extern int ini_validate_gridsq(const char *gridsq);
extern int ini_validate_name(const char *name);
extern int ini_validate_arq_bw(const char *val);
extern int ini_validate_fecmode(const char *val);
extern int ini_check_add_files_dir(const char *path);
extern int ini_check_ac_files_dir(const char *path);
extern int ini_check_ac_calls(const char *call);
extern char g_arim_path[];
extern char g_config_fname[];
extern char g_print_config_fname[];
extern int g_config_clo;

#endif

