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

#ifndef _ARIM_PROTO_H_INCLUDED_
#define _ARIM_PROTO_H_INCLUDED_

#include "main.h"
#include "ini.h"

#define ST_IDLE                         0
#define ST_SEND_MSG_BUF_WAIT            1
#define ST_SEND_NET_MSG_BUF_WAIT        2
#define ST_SEND_QRY_BUF_WAIT            3
#define ST_SEND_RESP_BUF_WAIT           4
#define ST_SEND_ACKNAK_BUF_WAIT         5
#define ST_SEND_BCN_BUF_WAIT            6
#define ST_SEND_UN_BUF_WAIT             7
#define ST_SEND_ACKNAK_PEND             8
#define ST_SEND_RESP_PEND               9
#define ST_RCV_ACKNAK_WAIT              10
#define ST_RCV_RESP_WAIT                11
#define ST_RCV_FRAME_WAIT               12
#define ST_ARQ_PEND_WAIT                13
#define ST_RCV_PING_ACK_WAIT            14
#define ST_SEND_PING_ACK_PEND           15
#define ST_RCV_MSG_PING_ACK_WAIT        16
#define ST_RCV_QRY_PING_ACK_WAIT        17
#define ST_ARQ_IN_CONNECT_WAIT          18
#define ST_ARQ_OUT_CONNECT_WAIT         19
#define ST_ARQ_OUT_CONNECT_WAIT_RPT     20
#define ST_RCV_ARQ_CONN_PING_ACK_WAIT   21
#define ST_ARQ_CONNECTED                22
#define ST_ARQ_FILE_SEND_WAIT           23
#define ST_ARQ_FILE_SEND_WAIT_OK        24
#define ST_ARQ_FILE_SEND                25
#define ST_ARQ_FILE_RCV_WAIT_OK         26
#define ST_ARQ_FILE_RCV_WAIT            27
#define ST_ARQ_FILE_RCV                 28
#define ST_ARQ_MSG_RCV                  29
#define ST_ARQ_MSG_SEND_WAIT            30
#define ST_ARQ_MSG_SEND                 31
#define ST_ARQ_AUTH_RCV_A2_WAIT         32
#define ST_ARQ_AUTH_RCV_A3_WAIT         33
#define ST_ARQ_AUTH_RCV_A4_WAIT         34
#define ST_ARQ_AUTH_SEND_A1             35
#define ST_ARQ_AUTH_SEND_A2             36
#define ST_ARQ_AUTH_SEND_A3             37
#define ST_ARQ_FLIST_RCV_WAIT           38
#define ST_ARQ_FLIST_RCV                39
#define ST_ARQ_FLIST_SEND_WAIT          40
#define ST_ARQ_FLIST_SEND               41

#define EV_NULL                         0
#define EV_PERIODIC                     1
#define EV_CANCEL                       2
#define EV_FRAME_START                  3
#define EV_FRAME_END                    4
#define EV_FRAME_TO                     5
#define EV_SEND_MSG                     6
#define EV_SEND_MSG_PP                  7
#define EV_SEND_NET_MSG                 8
#define EV_RCV_MSG                      9
#define EV_RCV_ACK                      10
#define EV_RCV_NAK                      11
#define EV_RCV_NET_MSG                  12
#define EV_SEND_QRY                     13
#define EV_SEND_QRY_PP                  14
#define EV_RCV_RESP                     15
#define EV_RCV_QRY                      16
#define EV_SEND_BCN                     17
#define EV_SEND_UNPROTO                 18
#define EV_SEND_PING                    19
#define EV_SEND_PING_ACK                20
#define EV_RCV_PING                     21
#define EV_RCV_PING_ACK                 22
#define EV_TNC_PTT                      23
#define EV_TNC_NEWSTATE                 24
#define EV_ARQ_PENDING                  25
#define EV_ARQ_CAN_PENDING              26
#define EV_ARQ_CONNECT                  27
#define EV_ARQ_CONNECT_PP               28
#define EV_ARQ_CONNECTED                29
#define EV_ARQ_DISCONNECTED             30
#define EV_ARQ_TARGET                   31
#define EV_ARQ_REJ_BUSY                 32
#define EV_ARQ_REJ_BW                   33
#define EV_ARQ_FILE_SEND                34
#define EV_ARQ_FILE_SEND_CMD            35
#define EV_ARQ_FILE_SEND_CMD_CLIENT     36
#define EV_ARQ_FILE_RCV_WAIT_OK         37
#define EV_ARQ_FILE_RCV_WAIT            38
#define EV_ARQ_FILE_RCV                 39
#define EV_ARQ_FILE_RCV_FRAME           40
#define EV_ARQ_FILE_RCV_DONE            41
#define EV_ARQ_FILE_ERROR               42
#define EV_ARQ_FILE_OK                  43
#define EV_ARQ_MSG_RCV                  44
#define EV_ARQ_MSG_RCV_FRAME            45
#define EV_ARQ_MSG_RCV_DONE             46
#define EV_ARQ_MSG_ERROR                47
#define EV_ARQ_MSG_OK                   48
#define EV_ARQ_MSG_SEND_CMD             49
#define EV_ARQ_MSG_SEND                 50
#define EV_ARQ_MSG_SEND_DONE            51
#define EV_ARQ_CANCEL_WAIT              52
#define EV_ARQ_AUTH_SEND_CMD            53
#define EV_ARQ_AUTH_WAIT_CMD            54
#define EV_ARQ_AUTH_OK                  55
#define EV_ARQ_AUTH_ERROR               56
#define EV_ARQ_FLIST_RCV_WAIT           57
#define EV_ARQ_FLIST_RCV                58
#define EV_ARQ_FLIST_RCV_FRAME          59
#define EV_ARQ_FLIST_RCV_DONE           60
#define EV_ARQ_FLIST_SEND               61
#define EV_ARQ_FLIST_SEND_CMD           62

#define MAX_ACKNAK_SIZE                 50

extern void arim_on_event(int event, int param);
extern int arim_is_idle(void);
extern int arim_get_buffer_cnt(void);
extern int arim_get_send_repeats(void);
extern void arim_set_send_repeats(int val);
extern int arim_get_fec_repeats(void);
extern void arim_set_state(int newstate);
extern int arim_get_state(void);
extern int arim_is_arq_state(void);
extern void arim_reset_msg_rpt_state(void);
extern void arim_copy_fecmode(char *mode, size_t size);
extern void arim_copy_mycall(char *call, size_t size);
extern int arim_copy_netcall(char *call, size_t size, int which);
extern int arim_get_netcall_cnt(void);
extern int arim_test_mycall(const char *call);
extern int arim_test_netcall(const char *call);
extern void arim_copy_gridsq(char *gridsq, size_t size);
extern void arim_copy_remote_call(char *call, size_t size);
extern void arim_copy_remote_gridsq(char *gridsq, size_t size);
extern void arim_copy_target_call(char *call, size_t size);
extern void arim_copy_arq_sendcr(char *val, size_t size);
extern void arim_copy_arq_bw(char *val, size_t size);
extern void arim_copy_arq_bw_hz(char *val, size_t size);
extern void arim_copy_tnc_state(char *state, size_t size);
extern int arim_check(const char *msg, unsigned int cs_rcvd);
extern int arim_store_out(const char *msg, const char *to);
extern int arim_store_sent(const char *msg, const char *to);
extern void arim_copy_listen(char *val, size_t size);
extern void arim_cancel_trans(void);
extern int arim_cancel_unproto(void);
extern int arim_cancel_frame(void);
extern void arim_fecmode_downshift(void);
extern int arim_is_receiving(void);
extern int arim_tnc_is_idle(void);
extern int arim_is_channel_busy(void);
extern void arim_set_channel_not_busy(void);
extern void arim_on_cancel(void);

extern size_t msg_len;
extern time_t prev_time;
extern int rcv_nak_cnt;
extern int ack_timeout;
extern int send_repeats;
extern int fecmode_downshift;
extern char prev_fecmode[TNC_FECMODE_SIZE];
extern char prev_to_call[TNC_MYCALL_SIZE];
extern char prev_msg[MAX_UNCOMP_DATA_SIZE];
extern char msg_buffer[MAX_UNCOMP_DATA_SIZE];
extern char msg_acknak_buffer[MAX_ACKNAK_SIZE];

#endif

