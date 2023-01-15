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
#include <sys/ioctl.h>
#include <fcntl.h>
#include "main.h"
#include "bufq.h"
#include "ini.h"
#include "ardop_cmds.h"
#include "tnc_attach.h"

void cmdthread_next_cmd_out(int sock)
{
    char *cmd, inbuffer[MAX_CMD_SIZE];
    int sent;

    pthread_mutex_lock(&mutex_cmd_out);
    cmd = cmdq_pop(&g_cmd_out_q);
    pthread_mutex_unlock(&mutex_cmd_out);
    if (cmd) {
        snprintf(inbuffer, sizeof(inbuffer), "%s\r", cmd);
        sent = write(sock, inbuffer, strlen(inbuffer));
        if (sent < 0) {
            bufq_queue_debug_log("Cmd thread: write to socket failed");
        } else {
            snprintf(inbuffer, sizeof(inbuffer), "<< %s", cmd);
            bufq_queue_cmd_in(inbuffer);
            bufq_queue_debug_log(inbuffer);
        }
    }
}

void *cmdthread_func(void *data)
{
    int cmdsock;
    char buffer[MAX_CMD_SIZE];
    struct addrinfo hints, *res = NULL;
    fd_set cmdreadfds, cmderrorfds;
    struct timeval timeout;
    ssize_t rsize;
    int result;

    bufq_queue_debug_log("Cmd thread: initializing");
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(g_tnc_settings[g_cur_tnc].ipaddr, g_tnc_settings[g_cur_tnc].port, &hints, &res);
    if (!res)
    {
        bufq_queue_debug_log("Cmd thread: failed to resolve IP address");
        g_cmdthread_stop = 1;
        pthread_exit(data);
    }
    cmdsock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(cmdsock, res->ai_addr, res->ai_addrlen) == -1) {
        bufq_queue_debug_log("Cmd thread: failed to open TCP socket");
        g_cmdthread_stop = 1;
        pthread_exit(data);
    }
    freeaddrinfo(res);
    g_cmdthread_ready = 1;
    snprintf(g_tnc_settings[g_cur_tnc].busy,
        sizeof(g_tnc_settings[g_cur_tnc].busy), "%s", "FALSE");
    ardop_cmds_init();
    while (1) {
        FD_ZERO(&cmdreadfds);
        FD_ZERO(&cmderrorfds);
        FD_SET(cmdsock, &cmdreadfds);
        FD_SET(cmdsock, &cmderrorfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        result = select(cmdsock + 1, &cmdreadfds, (fd_set *)0, &cmderrorfds, &timeout);
        switch (result) {
        case 0:
            cmdthread_next_cmd_out(cmdsock);
            break;
        case -1:
            bufq_queue_debug_log("Cmd thread: Socket select error (-1)");
            break;
        default:
            if (FD_ISSET(cmdsock, &cmdreadfds)) {
                rsize = read(cmdsock, buffer, sizeof(buffer) - 1);
                if (rsize == 0) {
                    bufq_queue_debug_log("Cmd thread: Socket closed by TNC");
                    tnc_detach(); /* close TCP connection to TNC */
                } else if (rsize == -1) {
                    bufq_queue_debug_log("Cmd thread: Socket read error (-1)");
                } else {
                    ardop_cmds_proc_resp(buffer, rsize);
                }
            }
            if (FD_ISSET(cmdsock, &cmderrorfds)) {
                bufq_queue_debug_log("Cmd thread: Socket select error (FD_ISSET)");
                break;
            }
        }
        if (g_cmdthread_stop) {
            break;
        }
    }
    snprintf(g_tnc_settings[g_cur_tnc].busy,
        sizeof(g_tnc_settings[g_cur_tnc].busy), "%s", "FALSE");
    bufq_queue_debug_log("Cmd thread: terminating");
    sleep(2);
    close(cmdsock);
    return data;
}

