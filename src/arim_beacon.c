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
#include "arim_beacon.h"
#include "ini.h"
#include "ui.h"
#include "ui_heard_list.h"
#include "ui_tnc_data_win.h"
#include "bufq.h"
#include "util.h"

int g_btime;

pthread_mutex_t mutex_beacon;
static int beacon_time_interval = 0;
static int beacon_time_elapsed = 0;
static int reset_btimer_on_tx = 0;
static char beaconstr[MIN_BEACON_BUF_SIZE];

int arim_beacon_send()
{
    size_t len;

    if (!arim_is_idle() || !arim_tnc_is_idle())
        return 0;
    bufq_queue_data_out(beaconstr);
    len = strlen(beaconstr);
    /* prime buffer count because update from TNC not immediate */
    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(g_tnc_settings[g_cur_tnc].buffer,
        sizeof(g_tnc_settings[g_cur_tnc].buffer), "%zu", len);
    pthread_mutex_unlock(&mutex_tnc_set);
    arim_on_event(EV_SEND_BCN, 0);
    return 1;
}

void arim_beacon_recv(const char *fm_call, const char *gridsq, const char *msg)
{
    char buffer[MAX_HEARD_SIZE];

    snprintf(buffer, sizeof(buffer), "5[B] %-10s ", fm_call);
    bufq_queue_heard(buffer);
}

void arim_beacon_on_alarm()
{
    static int try_again = 0;
    int send_beacon = 0;

    /* called every ALARM_INTERVAL_SEC secs (1/10 minute granularity) */
    if (!g_tnc_attached)
        return;
    pthread_mutex_lock(&mutex_beacon);
    if (beacon_time_elapsed && --beacon_time_elapsed == 0) {
        beacon_time_elapsed = beacon_time_interval;
        send_beacon = 1;
    }
    pthread_mutex_unlock(&mutex_beacon);
    if (send_beacon) {
        if (!arim_beacon_send()) {
            bufq_queue_debug_log("Automatic beacon: can't send, TNC busy.");
            if (!try_again) {
                /* try again once, two minutes from now */
                try_again = 1;
                beacon_time_elapsed = 2 * (60 / ALARM_INTERVAL_SEC);
            } else {
                try_again = 0;
            }
        }
    }
}

void arim_beacon_set(int minutes)
{
    char mycall[TNC_MYCALL_SIZE], gridsq[TNC_GRIDSQ_SIZE], name[TNC_NAME_SIZE];
    size_t len = 0;

    pthread_mutex_lock(&mutex_tnc_set);
    snprintf(mycall, sizeof(mycall), "%s", g_tnc_settings[g_cur_tnc].mycall);
    snprintf(gridsq, sizeof(gridsq), "%s", g_tnc_settings[g_cur_tnc].gridsq);
    snprintf(name, sizeof(name), "%s", g_tnc_settings[g_cur_tnc].name);
    if (!strncasecmp(g_tnc_settings[g_cur_tnc].reset_btime_tx, "TRUE", 4))
        reset_btimer_on_tx = 1;
    else
        reset_btimer_on_tx = 0;
    pthread_mutex_unlock(&mutex_tnc_set);

    pthread_mutex_lock(&mutex_beacon);
    if (minutes >= 0)
        g_btime = minutes;
    beacon_time_interval = g_btime * (60 / ALARM_INTERVAL_SEC);
    beacon_time_elapsed = beacon_time_interval;
    snprintf(beaconstr, sizeof(beaconstr), "|B%02d|%s|%04zX|%s|%s",
                    ARIM_PROTO_VERSION,
                    mycall,
                    len,
                    gridsq,
                    name);
    len = strlen(beaconstr);
    snprintf(beaconstr, sizeof(beaconstr), "|B%02d|%s|%04zX|%s|%s",
                    ARIM_PROTO_VERSION,
                    mycall,
                    len,
                    gridsq,
                    name);
    pthread_mutex_unlock(&mutex_beacon);
}

void arim_beacon_reset_btimer()
{
    if (reset_btimer_on_tx) {
        pthread_mutex_lock(&mutex_beacon);
        beacon_time_elapsed = beacon_time_interval;
        pthread_mutex_unlock(&mutex_beacon);
    }
}

int arim_beacon_cancel()
{
    char buffer[MAX_LOG_LINE_SIZE];

    /* operator has canceled the beacon by pressing ESC key,
       print to monitor view and traffic log */
    snprintf(buffer, sizeof(buffer), ">> [X] (Beacon canceled by operator)");
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    return 1;
}

int arim_beacon_timeout()
{
    char buffer[MAX_LOG_LINE_SIZE];

    /* beacon send attempt timed out, print to monitor view and traffic log */
    snprintf(buffer, sizeof(buffer), ">> [X] (Beacon send attempt timed out)");
    bufq_queue_traffic_log(buffer);
    bufq_queue_data_in(buffer);
    return 1;
}

