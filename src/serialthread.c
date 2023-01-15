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
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include "main.h"
#include "arim.h"
#include "arim_proto.h"
#include "arim_arq.h"
#include "bufq.h"
#include "ini.h"
#include "log.h"
#include "ardop_cmds.h"
#include "ardop_data.h"
#include "util.h"
#include "ui.h"

#define IO_STATE_ERROR            (-1)
#define IO_STATE_IDLE               0
#define IO_STATE_BUSY               1
#define IO_STATE_TEST_CMD_MODE_1ST  2
#define IO_STATE_TEST_CMD_MODE_2ND  3
#define IO_STATE_SET_ARDOP_MODE     4
#define IO_STATE_EXIT_HOST_MODE     5
#define IO_STATE_ENTER_HOST_MODE    6

#define IO_CHAN_CMD              0x20
#define IO_CHAN_DATA             0x21
#define IO_CHAN_LOG              0xF8
#define IO_CHAN_STATUS           0xFE
#define IO_CHAN_GEN_POLL         0xFF

#define IO_TIME_OUT                40 /* 40 50 msec ticks = 2 sec */
#define IO_DATA_BLOCK_SIZE        240 /* reserve 16 bytes for framing overheads */

#define HOST_TNC_DATA               0
#define HOST_TNC_CMD                1

#define TNC_HOST_RESP_OK            0
#define TNC_HOST_RESP_OK_MSG        1
#define TNC_HOST_RESP_FAIL_MSG      2
#define TNC_HOST_DATA               7

static int io_state, io_timer, io_seq, io_rpts;
static int respsize, tnc_init;
static char respbuf[MAX_CMD_SIZE*2];

unsigned short crc16_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF, 
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7, 
    0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E, 
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876, 
    0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD, 
    0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5, 
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C, 
    0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974, 
    0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB, 
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3, 
    0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A, 
    0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72, 
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9, 
    0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1, 
    0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738, 
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70, 
    0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7, 
    0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF, 
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036, 
    0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E, 
    0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5, 
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD, 
    0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134, 
    0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C, 
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3, 
    0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB, 
    0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232, 
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A, 
    0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1, 
    0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9, 
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330, 
    0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78 
}; 

unsigned int calc_crc16(const unsigned char *data, size_t size)
{
    int i;
    unsigned short fcs = 0xFFFF;

    for (i = 0; i < size; i++)
        fcs = (fcs >> 8) ^ crc16_table[(fcs ^ data[i]) & 0xFF];
    return fcs;
}

int serialthread_baud_rate(const char *rate)
{
    if (!strncasecmp(rate, "9600", 4))
        return B9600;
    if (!strncasecmp(rate, "19200", 5))
        return B19200;
    if (!strncasecmp(rate, "38400", 5))
        return B38400;
    if (!strncasecmp(rate, "57600", 5))
        return B57600;
    if (!strncasecmp(rate, "115200", 6))
        return B115200;
    return 0;
}

int serialthread_send_frame(int fd, unsigned char *data, int size)
{
    static unsigned char framebuf[MAX_CMD_SIZE*2];
    static int framesize;
    unsigned char *in, *end, *out, databuf[MAX_CMD_SIZE*2];
    int state, sent;
    unsigned int crc16;

    if (data == NULL) /* repeat previous frame cached in framebuf */
        goto repeat;
    if (size > MAX_CMD_SIZE+3)
        return IO_STATE_IDLE; /* too much data, drop frame */
    /* prepare data for framing */
    memcpy(databuf, data, size);
    databuf[1] |= io_seq; /* set sequence counter */
    if (tnc_init) { /* sequence counter not defined yet */
        tnc_init= 0;
        databuf[1] |= 0x40; /* set bit 6 after host mode start */
    }
    crc16 = calc_crc16(databuf, size);
    crc16 ^= 0xFFFF;
    databuf[size++] = crc16 & 0xFF;
    databuf[size++] = (crc16 >> 8) & 0xFF;
    /* construct frame, byte stuff as needed */
    framebuf[0] = 0xAA;
    framebuf[1] = 0xAA;
    in = databuf;
    end = databuf + size;
    out = framebuf + 2;
    while (in < end) {
        *out++ = *in;
        if (*in++ == 0xAA)
            *out++ = 0;
    }
    framesize = out - framebuf;
    io_rpts = 5; /* set counter for repeat attempts after NACK condition */
repeat:
    sent = write(fd, framebuf, framesize);
    if (sent < 0) {
        bufq_queue_debug_log("Serial thread: Send frame to TNC, write to serial port failed");
        state = IO_STATE_ERROR;
    } else {
        io_timer = IO_TIME_OUT;
        state = IO_STATE_BUSY;
    }
    return state;
}

int serialthread_next_cmd_out(int fd)
{
    char *cmd;
    unsigned char msgbuf[MAX_CMD_SIZE*2];
    int state;

    state = io_state;
    pthread_mutex_lock(&mutex_cmd_out);
    cmd = cmdq_pop(&g_cmd_out_q);
    pthread_mutex_unlock(&mutex_cmd_out);
    if (cmd) {
        msgbuf[0] = IO_CHAN_CMD;
        msgbuf[1] = HOST_TNC_DATA;
        msgbuf[2] = strlen(cmd); /* implicitly include terminating '\r' in count */
        snprintf((char *)&msgbuf[3], MAX_CMD_SIZE, "%s\r", cmd);
        state = serialthread_send_frame(fd, msgbuf, strlen(cmd) + 4);
        if (state == IO_STATE_ERROR)
            bufq_queue_debug_log("Serial thread: Send cmd data frame to TNC, write to serial port failed");
    }
    return state;
}

int serialthread_send_file_out(int fd)
{
    static size_t sent = 0, nblk = 0, nrem = 0;
    static unsigned char databuf[MAX_FILE_SIZE+4], framebuf[MAX_CMD_SIZE*2];
    FILEQUEUEITEM *item;
    unsigned char *p, *s;
    int state;

    state = io_state;
    if (!nblk && !nrem) { /* nothing to send, check for queued file */
        pthread_mutex_lock(&mutex_file_out);
        item = fileq_pop(&g_file_out_q);
        pthread_mutex_unlock(&mutex_file_out);
        if (!item)
            return state;
        memcpy(databuf, item->data, item->size);
        nblk = item->size / IO_DATA_BLOCK_SIZE;
        nrem = item->size % IO_DATA_BLOCK_SIZE;
        sent = 0;
    }
    if (nblk) {
        --nblk;
        framebuf[0] = IO_CHAN_DATA;
        framebuf[1] = HOST_TNC_DATA;
        framebuf[2] = (IO_DATA_BLOCK_SIZE+2) - 1; /* accomodate the 2 ARDOP length bytes */
        p = &framebuf[3];
        *p++ = (IO_DATA_BLOCK_SIZE >> 8) & 0xFF;
        *p++ = IO_DATA_BLOCK_SIZE & 0xFF;
        s = databuf + sent;
        memcpy(p, s, IO_DATA_BLOCK_SIZE);
        state = serialthread_send_frame(fd, (unsigned char *)framebuf, IO_DATA_BLOCK_SIZE + 5);
        if (state == IO_STATE_ERROR) {
            bufq_queue_debug_log("Serial thread: Send file frame to TNC, write to serial port failed");
        } else {
            sent += IO_DATA_BLOCK_SIZE;
            ardop_data_inc_num_bytes_out(IO_DATA_BLOCK_SIZE);
        }
    } else if (nrem) {
        framebuf[0] = IO_CHAN_DATA;
        framebuf[1] = HOST_TNC_DATA;
        framebuf[2] = (nrem + 2) - 1; /* accomodate the 2 ARDOP length bytes */
        p = &framebuf[3];
        *p++ = (nrem >> 8) & 0xFF;
        *p++ = nrem & 0xFF;
        s = databuf + sent;
        memcpy(p, s, nrem);
        state = serialthread_send_frame(fd, (unsigned char *)framebuf, nrem + 5);
        if (state == IO_STATE_ERROR) {
            bufq_queue_debug_log("Serial thread: Send file frame to TNC, write to serial port failed");
        } else {
            sent += nrem;
            ardop_data_inc_num_bytes_out(nrem);
        }
        nrem = sent = 0;
    }
    return state;
}

int serialthread_send_msg_out(int fd)
{
    static size_t sent = 0, nblk = 0, nrem = 0;
    static unsigned char databuf[MIN_MSG_BUF_SIZE], framebuf[MAX_CMD_SIZE*2];
    MSGQUEUEITEM *item;
    unsigned char *p, *s;
    int state;

    state = io_state;
    if (!nblk && !nrem) { /* nothing to send, check for queued data */
        pthread_mutex_lock(&mutex_msg_out);
        item = msgq_pop(&g_msg_out_q);
        pthread_mutex_unlock(&mutex_msg_out);
        if (!item)
            return state;
        memcpy(databuf, item->data, item->size);
        nblk = item->size / IO_DATA_BLOCK_SIZE;
        nrem = item->size % IO_DATA_BLOCK_SIZE;
        sent = 0;
    }
    if (nblk) {
        bufq_queue_debug_log("Serial thread: writing message to serial port");
        --nblk;
        framebuf[0] = IO_CHAN_DATA;
        framebuf[1] = HOST_TNC_DATA;
        framebuf[2] = (IO_DATA_BLOCK_SIZE+2) - 1; /* accomodate the 2 ARDOP length bytes */
        p = &framebuf[3];
        *p++ = (IO_DATA_BLOCK_SIZE >> 8) & 0xFF;
        *p++ = IO_DATA_BLOCK_SIZE & 0xFF;
        s = databuf + sent;
        memcpy(p, s, IO_DATA_BLOCK_SIZE);
        state = serialthread_send_frame(fd, (unsigned char *)framebuf, IO_DATA_BLOCK_SIZE + 5);
        if (state == IO_STATE_ERROR) {
            bufq_queue_debug_log("Serial thread: Send message frame to TNC, write to serial port failed");
        } else {
            sent += IO_DATA_BLOCK_SIZE;
            ardop_data_inc_num_bytes_out(IO_DATA_BLOCK_SIZE);
        }
    } else if (nrem) {
        bufq_queue_debug_log("Serial thread: writing remainder of message to serial port");
        framebuf[0] = IO_CHAN_DATA;
        framebuf[1] = HOST_TNC_DATA;
        framebuf[2] = (nrem + 2) - 1; /* accomodate the 2 ARDOP length bytes */
        p = &framebuf[3];
        *p++ = (nrem >> 8) & 0xFF;
        *p++ = nrem & 0xFF;
        s = databuf + sent;
        memcpy(p, s, nrem);
        state = serialthread_send_frame(fd, (unsigned char *)framebuf, nrem + 5);
        if (state == IO_STATE_ERROR) {
            bufq_queue_debug_log("Serial thread: Send message frame to TNC, write to serial port failed");
        } else {
            sent += nrem;
            ardop_data_inc_num_bytes_out(nrem);
        }
        nrem = sent = 0;
    }
    return state;
}

int serialthread_send_data_out(int fd)
{
    static size_t sent = 0, nblk = 0, nrem = 0, done = 0, len = 0;
    static char databuf[MIN_DATA_BUF_SIZE];
    static unsigned char framebuf[MAX_CMD_SIZE*2];
    char buffer[MAX_LOG_LINE_SIZE];
    unsigned char *p;
    char *s, *data;
    int state, numch;

    state = io_state;
    if (!nblk && !nrem) { /* nothing to send, check for queued data */
        pthread_mutex_lock(&mutex_data_out);
        data = dataq_pop(&g_data_out_q);
        pthread_mutex_unlock(&mutex_data_out);
        if (!data)
            return state;
        snprintf(databuf, sizeof(databuf), "%s", data);
        len = strlen(databuf);
        nblk = len / IO_DATA_BLOCK_SIZE;
        nrem = len % IO_DATA_BLOCK_SIZE;
        if (!nblk && !nrem)
            return state;
        sent = 0;
        done = 0;
    }
    if (nblk) {
        bufq_queue_debug_log("Serial thread: writing block of data to serial port");
        --nblk;
        framebuf[0] = IO_CHAN_DATA;
        framebuf[1] = HOST_TNC_DATA;
        framebuf[2] = (IO_DATA_BLOCK_SIZE+2) - 1; /* accomodate the 2 ARDOP length bytes */
        p = &framebuf[3];
        *p++ = (IO_DATA_BLOCK_SIZE >> 8) & 0xFF;
        *p++ = IO_DATA_BLOCK_SIZE & 0xFF;
        s = databuf + sent;
        memcpy(p, s, IO_DATA_BLOCK_SIZE);
        state = serialthread_send_frame(fd, (unsigned char *)framebuf, IO_DATA_BLOCK_SIZE + 5);
        if (state == IO_STATE_ERROR) {
            bufq_queue_debug_log("Serial thread: Send data frame to TNC, write to serial port failed");
        } else {
            sent += IO_DATA_BLOCK_SIZE;
            ardop_data_inc_num_bytes_out(IO_DATA_BLOCK_SIZE);
        }
        if (!nblk && !nrem)
            done = 1;
    } else if (nrem) {
        bufq_queue_debug_log("Serial thread: writing remainder of data to serial port");
        framebuf[0] = IO_CHAN_DATA;
        framebuf[1] = HOST_TNC_DATA;
        framebuf[2] = (nrem + 2) - 1; /* accomodate the 2 ARDOP length bytes */
        p = &framebuf[3];
        *p++ = (nrem >> 8) & 0xFF;
        *p++ = nrem & 0xFF;
        s = databuf + sent;
        memcpy(p, s, nrem);
        state = serialthread_send_frame(fd, (unsigned char *)framebuf, nrem + 5);
        if (state == IO_STATE_ERROR) {
            bufq_queue_debug_log("Serial thread: Send data frame to TNC, write to serial port failed");
        } else {
            sent += nrem;
            ardop_data_inc_num_bytes_out(nrem);
        }
        nrem = 0;
        done = 1;
    }
    if (done) {
        /* print trace to monitor view */
        if (arim_test_frame(databuf, len))
            numch = snprintf(buffer, sizeof(buffer), "<< [%c] %s", databuf[1], databuf);
        else if (arim_is_arq_state())
            numch = snprintf(buffer, sizeof(buffer), "<< [@] %s", databuf);
        else
            numch = snprintf(buffer, sizeof(buffer), "<< [U] %s", databuf);
        if (numch >= sizeof(buffer))
            ui_truncate_line(buffer, sizeof(buffer));
        bufq_queue_data_in(buffer);
        bufq_queue_traffic_log(buffer);
        /* send FECSEND command to TNC in FEC mode */
        if (!arim_is_arq_state())
            bufq_queue_cmd_out("FECSEND TRUE");
    }
    return state;
}

int serialthread_enter_host_mode(int fd)
{
    int state, sent;
    char msgbuf[MAX_CMD_SIZE];

    snprintf(msgbuf, sizeof(msgbuf), "JHOST4\r");
    sent = write(fd, msgbuf, strlen(msgbuf));
    if (sent < 0) {
        bufq_queue_debug_log("Serial thread: Enter host mode, write to serial port failed");
        state = IO_STATE_ERROR;
    } else {
        bufq_queue_debug_log("Serial thread: Entering host mode");
        state = IO_STATE_ENTER_HOST_MODE;
        /* set timeout to 1 sec to hasten connection sequence */
        io_timer = 20;
    }
    return state;
}

int serialthread_exit_host_mode(int fd)
{
    int state;
    static char msgbuf[MAX_CMD_SIZE];

    msgbuf[0] = IO_CHAN_CMD;
    msgbuf[1] = HOST_TNC_CMD|0x40; /* set sequence reset flag */
    msgbuf[2] = 0x05;
    msgbuf[3] = 'J';
    msgbuf[4] = 'H';
    msgbuf[5] = 'O';
    msgbuf[6] = 'S';
    msgbuf[7] = 'T';
    msgbuf[8] = '0';
    state = serialthread_send_frame(fd, (unsigned char *)msgbuf, 9);
    if (state == IO_STATE_ERROR) {
        bufq_queue_debug_log("Serial thread: Exit TNC host mode, write to serial port failed");
    } else {
        bufq_queue_debug_log("Serial thread: Exiting TNC host mode");
        state = IO_STATE_EXIT_HOST_MODE; /* override IO_STATE_BUSY */
    }
    /* set timeout to 1 sec to hasten connection sequence */
    io_timer = 20;
    return state;
}

int serialthread_set_ardop_mode(int fd)
{
    int state, sent;
    char msgbuf[MAX_CMD_SIZE];

    snprintf(msgbuf, sizeof(msgbuf), "ARDOP\r");
    sent = write(fd, msgbuf, strlen(msgbuf));
    if (sent < 0) {
        bufq_queue_debug_log("Serial thread: Set TNC ARDOP mode, write to serial port failed");
        state = IO_STATE_ERROR;
    } else {
        bufq_queue_debug_log("Serial thread: Setting TNC ARDOP mode");
        state = IO_STATE_SET_ARDOP_MODE;
        io_timer = IO_TIME_OUT;
    }
    return state;
}

int serialthread_test_cmd_mode_1st(int fd)
{
    int state, sent;
    char msgbuf[MAX_CMD_SIZE];

    msgbuf[0] = 0x1B;
    msgbuf[1] = 0x0D;
    sent = write(fd, msgbuf, 2);
    if (sent < 0) {
        bufq_queue_debug_log("Serial thread: Test TNC cmd mode (1), write to serial port failed");
        state = IO_STATE_ERROR;
    } else {
        snprintf(msgbuf, sizeof(msgbuf), "<< %s", "Probing serial port for TNC...");
        bufq_queue_cmd_in(msgbuf);
        bufq_queue_debug_log("Serial thread: Checking if TNC in cmd mode (1)");
        state = IO_STATE_TEST_CMD_MODE_1ST;
        io_timer = 10; /* 500 msec timeout */
    }
    return state;
}

int serialthread_test_cmd_mode_2nd(int fd)
{
    int state, sent;
    char msgbuf[MAX_CMD_SIZE];

    msgbuf[0] = 0x1B;
    msgbuf[1] = 0x0D;
    sent = write(fd, msgbuf, 2);
    if (sent < 0) {
        bufq_queue_debug_log("Serial thread: Test TNC cmd mode (2), write to serial port failed");
        state = IO_STATE_ERROR;
    } else {
        bufq_queue_debug_log("Serial thread: Checking if TNC in cmd mode (2)");
        state = IO_STATE_TEST_CMD_MODE_2ND;
        io_timer = 10; /* 500 msec timeout */
    }
    return state;
}

int serialthread_gen_poll(int fd)
{
    int state;
    unsigned char msgbuf[MAX_CMD_SIZE];

    msgbuf[0] = IO_CHAN_GEN_POLL;
    msgbuf[1] = HOST_TNC_CMD;
    msgbuf[2] = 0x00;
    msgbuf[3] = 0x47;
    state = serialthread_send_frame(fd, msgbuf, 4);
    if (state == IO_STATE_ERROR)
        bufq_queue_debug_log("Serial thread: General poll to TNC, write to serial port failed");
    return state;
}

int serialthread_unstuff_frame(char *buf, char *data, size_t size)
{
    char *in, *out, *end;

    if (data[0] == 0xAA && data[1] == 0xAA)
        in = data + 2;
    else
        in = data;
    end = data + size;
    out = buf;
    while (in < end) {
        *out = *in;
        if (*in == 0xAA) {
            ++in;
            if (*in && in < end) /* byte stuffing error */
                return -1;
        }
        ++in;
        ++out;
    }
    if (out - buf > MAX_CMD_SIZE+2) {
        /* too much data, force reset */
        return -1;
    }
    return out - buf;
}

int serialthread_handle_gp_resp(char *resp, int size, int fd)
{
    int state, channel;
    unsigned char msgbuf[MAX_CMD_SIZE];

    state = IO_STATE_IDLE;
    if (resp[1] == TNC_HOST_RESP_OK_MSG && resp[2]) {
        channel = resp[2] - 1;
        if (channel == IO_CHAN_STATUS)
            return state; /* ignore status channel 254 */
        msgbuf[0] = channel;
        msgbuf[1] = HOST_TNC_CMD;
        msgbuf[2] = 0x00;
        msgbuf[3] = 0x47;
        state = serialthread_send_frame(fd, msgbuf, 4);
        if (state == IO_STATE_ERROR)
            bufq_queue_debug_log("Serial thread: Send poll frame to TNC, write to serial port failed");
    }
    return state;
}

int serialthread_handle_log_trace(char *resp, int size)
{
    char linebuf[MAX_LOG_LINE_SIZE];

    /*
       Format of TNC-PI9K6 log trace: "TTTTMMMLXXXX..." 
       where TTTT is 32 bit timestamp from Teensy,
             MMM is ASCII encoded milliseconds,
             L is 8 bit "type",
             XXXX... is data of length size - 8.
       Time on Teensy often not in sync with RPi, so
       ignore the timestamp. The local timestamp will
       be inserted when the trace is written to the log.
       This makes it easy to compare the TNC-PI9K6 and
       ARIM logs when troubleshooting.
    */
    snprintf(linebuf, sizeof(linebuf), "[%c] ", resp[7]);
    size = size - 10; /* drop trailing ctrl chars */
    memcpy(&linebuf[4], &resp[8], size);
    linebuf[size + 4] = '\0';
    bufq_queue_tncpi9k6_log(linebuf);
    return 1;
}

int serialthread_dispatch_resp(char *resp, int size, int fd)
{
    int state;
    unsigned int channel;
    char temp[MAX_CMD_SIZE*2];

    state = IO_STATE_IDLE;
    channel = (unsigned int) resp[0];
    switch (channel) { /* dispatch to handlers for each channel */
    case IO_CHAN_GEN_POLL: /* handle general poll channel response */
        state = serialthread_handle_gp_resp(resp, size, fd);
        break;
    case IO_CHAN_CMD:
        switch (resp[1]) { /* opcode - handle command channel response accordingly */
        case TNC_HOST_RESP_OK_MSG:
            snprintf(temp, sizeof(temp), "%s\r", &resp[2]);
            ardop_cmds_proc_resp(temp, strlen(temp));
            break; 
        case TNC_HOST_DATA:
            snprintf(temp, resp[2] + 2, "%s\r", &resp[3]);
            ardop_cmds_proc_resp(temp, strlen(temp));
            break;
        default:
            bufq_queue_debug_log("Serial thread: Received unknown op code in cmd channel response");
            break;
        }
        break;
    case IO_CHAN_DATA:
        switch (resp[1]) { /* opcode - handle data channel response accordingly */
        case TNC_HOST_DATA:
            ardop_data_handle_data((unsigned char *)&resp[3], resp[2] + 1);
            break;
        case TNC_HOST_RESP_OK:
            break;
        case TNC_HOST_RESP_OK_MSG:
            bufq_queue_debug_log("Serial thread: Received 'response success' with NT msg from TNC");
            break;
        case TNC_HOST_RESP_FAIL_MSG:
            bufq_queue_debug_log("Serial thread: Received 'response failure' with NT msg from TNC");
            break;
        default:
            bufq_queue_debug_log("Serial thread: Received unknown op code in data channel response");
            break;
        }
        break;
    case IO_CHAN_LOG:
        switch (resp[1]) { /* opcode - handle log channel response accordingly */
        case TNC_HOST_DATA:
            if (g_tncpi9k6_log_enable) 
                serialthread_handle_log_trace(&resp[3], resp[2] + 1);
            break;
        default:
            bufq_queue_debug_log("Serial thread: Received unknown op code in log channel response");
            break;
        }
        break;
    default:
        bufq_queue_debug_log("Serial thread: Ignoring response from TNC on channel: %d");
        break;
    }
    return state;
}

size_t serialthread_on_rcv(char *data, size_t size, int fd)
{
    int state, datasize;
    unsigned int crc16;
    char temp[MAX_CMD_SIZE*2];

    state = io_state;
    switch (io_state) {
    case IO_STATE_TEST_CMD_MODE_1ST:
    case IO_STATE_TEST_CMD_MODE_2ND:
        /* TNC in cmd mode, set ARDOP mode before switching to host mode */
        snprintf(temp, size + 1, "%s", data);
        if (strstr(temp, ": ")) {
            io_rpts = 3;
            state = serialthread_set_ardop_mode(fd);
        }
        break;
    case IO_STATE_SET_ARDOP_MODE: /* TNC in command mode, change to host mode */
        io_rpts = 0;
        state = serialthread_enter_host_mode(fd);
        break;
    default:
        if (data[0] == 0xAA && data [1] == 0xAA) { /* host mode response from TNC */
            if (size >= 4 && data[2] == 0xAA && data[3] == 0x55) {
                /* repeat request; resend previous frame */
                respsize = 0;
                bufq_queue_debug_log("Serialthread: Received request to repeat last frame sent");
                return serialthread_send_frame(fd, NULL, 0);
            }
            if (size < 6) /* not a whole frame yet */
                return state;
            if ((data[3] & 0x80) != io_seq) {
                /* bad sequence counter, ignore response frame */
                bufq_queue_debug_log("Serialthread: Bad rx sequence counter in frame");
                respsize = 0;
                io_timer = 0;
                return IO_STATE_IDLE;
            }
            io_seq ^= 0x80; /* update sequence bit for next frame received */
            respsize = 0;   /* new frame, clear buffer */
        }
        datasize = serialthread_unstuff_frame(respbuf + respsize, data, size);
        if (datasize == -1) {
            /* error unstuffing frame or too large, abandon data */
            respsize = 0;
            io_timer = 0;
            state = IO_STATE_IDLE;
            bufq_queue_debug_log("Serialthread: Error unstuffing rx frame");
        } else {
            respsize += datasize;
            crc16 = calc_crc16((unsigned char *)respbuf, respsize);
            if (crc16 == 0xF0B8) {
                /* have the entire response payload */
                respbuf[1] &= 0x7F;   /* clear sequence counter bit */
                state = serialthread_dispatch_resp(respbuf, respsize, fd);
                respsize = 0;
                io_timer = 0;
            } /* if crc bad assume frame isn't complete, wait for more data */
        }
    }
    return state;
}

void *serialthread_func(void *data)
{
    char buffer[MAX_CMD_SIZE];
    fd_set readfds, errorfds;
    struct timeval timeout;
    struct termios io_set;
    ssize_t rsize;
    int result, serialfd, arim_timeout, msec_200;
    time_t cur_time;

    bufq_queue_debug_log("Serial thread: initializing");
    /* open serial port */
    snprintf(buffer, sizeof(buffer), "%s", g_tnc_settings[g_cur_tnc].serial_port);
    serialfd = open(buffer, O_RDWR | O_NOCTTY);
    if (serialfd == -1)
    {
        bufq_queue_debug_log("Serial thread: failed to open serial port");
        g_serialthread_stop = 1;
        pthread_exit(data);
    }
    memset(&io_set, 0, sizeof(io_set));
    /* set baud rate for i/o */
    cfsetispeed(&io_set, serialthread_baud_rate(g_tnc_settings[g_cur_tnc].serial_baudrate));
    cfsetospeed(&io_set, serialthread_baud_rate(g_tnc_settings[g_cur_tnc].serial_baudrate));
    /* enable receiver, disable control lines */
    io_set.c_cflag |= (CLOCAL|CREAD);
    /* 8 bits, no parity, 1 stop bit */
    io_set.c_cflag &= ~PARENB;
    io_set.c_cflag &= ~CSTOPB;
    io_set.c_cflag &= ~CSIZE;
    io_set.c_cflag |= CS8;
    /* raw mode */
    io_set.c_lflag &= ~(ICANON|ECHO|ISIG);
    io_set.c_oflag &= ~OPOST;
    /* install termios settings */
    tcsetattr(serialfd, TCSANOW, &io_set);
    g_serialthread_ready = 1;
    /* ARIM protocol timeout specified in secs */
    arim_timeout = atoi(g_arim_settings.frame_timeout);
    arim_reset();
    snprintf(g_tnc_settings[g_cur_tnc].busy,
        sizeof(g_tnc_settings[g_cur_tnc].busy), "%s", "FALSE");
    respsize = 0;
    io_rpts = 3;
    msec_200 = 3;
    io_state = serialthread_test_cmd_mode_1st(serialfd);
    while (1) {
        FD_ZERO(&readfds);
        FD_ZERO(&errorfds);
        FD_SET(serialfd, &readfds);
        FD_SET(serialfd, &errorfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000; /* 50 msec */
        result = select(serialfd + 1, &readfds, (fd_set *)0, &errorfds, &timeout);
        switch (result) {
        case 0:
            if (!msec_200--) { /* generate periodic event every 200 msec */
                msec_200 = 3;
                arim_on_event(EV_PERIODIC, 0);
            }
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
            if (io_timer)
                --io_timer;
            if (io_timer == 0) {
                switch (io_state) {
                case IO_STATE_IDLE:
                    /* if command is queued, send it */
                    io_state = serialthread_next_cmd_out(serialfd);
                    if (io_state != IO_STATE_IDLE)
                        break;
                    /* if data is queued, send it */
                    io_state = serialthread_send_file_out(serialfd);
                    if (io_state != IO_STATE_IDLE)
                        break;
                    io_state = serialthread_send_msg_out(serialfd);
                    if (io_state != IO_STATE_IDLE)
                        break;
                    io_state = serialthread_send_data_out(serialfd);
                    if (io_state != IO_STATE_IDLE)
                        break;
                    /* pump outbound and inbound arq line queues */
                    arim_arq_on_cmd(NULL, 0);
                    arim_arq_on_resp(NULL, 0);
                    if (io_state == IO_STATE_IDLE) /* no command queued, send general poll cmd */
                        io_state = serialthread_gen_poll(serialfd);
                    break;
                case IO_STATE_BUSY:
                    if (io_rpts && --io_rpts == 0) {
                        /* send frame attempt timed out, try to restart TNC */
                        bufq_queue_debug_log("Serial thread: Frame send timeout, restarting TNC");
                        io_rpts = 3;
                        respsize = 0;
                        arim_reset();
                        snprintf(g_tnc_settings[g_cur_tnc].busy,
                            sizeof(g_tnc_settings[g_cur_tnc].busy), "%s", "FALSE");
                        io_state = serialthread_test_cmd_mode_1st(serialfd);
                    } else {
                        /* repeat previous command */
                        bufq_queue_debug_log("Serial thread: Frame ack timeout, repeating");
                        io_state = serialthread_send_frame(serialfd, NULL, 0);
                    }
                    break;
                case IO_STATE_TEST_CMD_MODE_1ST:
                    if (io_rpts && --io_rpts == 0) {
                        /* timed out, must be in host mode, try to exit */
                        bufq_queue_debug_log("Serial thread: Test TNC cmd mode timeout, exiting host mode");
                        io_state = serialthread_exit_host_mode(serialfd);
                    } else {
                        /* repeat previous command */
                        io_state = serialthread_test_cmd_mode_1st(serialfd);
                    }
                    break;
                case IO_STATE_TEST_CMD_MODE_2ND:
                    if (io_rpts && --io_rpts == 0) {
                        /* timed out, abandon attempt to attach to TNC */
                        bufq_queue_debug_log("Serial thread: Test TNC cmd mode timeout, detaching");
                        io_state = IO_STATE_IDLE;
                        g_serialthread_stop = 1; /* detach from TNC */
                    } else {
                        /* repeat previous command */
                        io_state = serialthread_test_cmd_mode_2nd(serialfd);
                    }
                    break;
                case IO_STATE_SET_ARDOP_MODE:
                    if (io_rpts && --io_rpts == 0) {
                        /* timed out, abandon attempt to attach to TNC */
                        bufq_queue_debug_log("Serial thread: Set TNC ARDOP mode wait timeout, detaching");
                        io_state = IO_STATE_IDLE;
                        g_serialthread_stop = 1; /* detach from TNC */
                    } else {
                        /* repeat previous command */
                        io_state = serialthread_set_ardop_mode(serialfd);
                    }
                    break;
                case IO_STATE_EXIT_HOST_MODE:
                    bufq_queue_debug_log("Serial thread: Exited host mode, reinitializing TNC");
                    io_rpts = 3;
                    io_state = serialthread_test_cmd_mode_2nd(serialfd);
                    break;
                case IO_STATE_ENTER_HOST_MODE:
                    bufq_queue_debug_log("Serial thread: Entered host mode, initializing TNC");
                    tnc_init = 1; /* set sequence counter 'not defined' flag' */
                    io_seq = 0;   /* clear sequence counter */
                    io_timer = 0;
                    io_state = IO_STATE_IDLE;
                    ardop_cmds_init();
                    break;
                case IO_STATE_ERROR:
                    bufq_queue_debug_log("Serial thread: I/O error, returning to idle state");
                    io_rpts = 0;
                    io_timer = 0;
                    io_state = IO_STATE_IDLE;
                    break;
                default:
                    bufq_queue_debug_log("Serial thread: Unknown IO state");
                    break;
                }
            }
            break;
        case -1:
            bufq_queue_debug_log("Serial thread: Socket select error (-1)");
            break;
        default:
            if (FD_ISSET(serialfd, &readfds)) {
                rsize = read(serialfd, buffer, sizeof(buffer) - 1);
                if (rsize != -1)
                    io_state = serialthread_on_rcv(buffer, rsize, serialfd);
                else
                    bufq_queue_debug_log("Serial thread: Error on serial port read");
            }
            if (FD_ISSET(serialfd, &errorfds)) {
                bufq_queue_debug_log("Serial thread: Serial port select error (FD_ISSET)");
                break;
            }
        }
        if (g_serialthread_stop)
            break;
    }
    serialthread_exit_host_mode(serialfd);
    snprintf(g_tnc_settings[g_cur_tnc].busy,
        sizeof(g_tnc_settings[g_cur_tnc].busy), "%s", "FALSE");
    bufq_queue_debug_log("Serial thread: terminating");
    sleep(2);
    close(serialfd);
    return data;
}

