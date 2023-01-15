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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "main.h"
#include "bufq.h"
#include "ini.h"
#include "util.h"

#define MAX_LOG_FN_SIZE         256
#define LOG_WRITE_INTERVAL_SEC  30

char g_df_error_fn[MAX_LOG_FN_SIZE];

static int time_interval = 0;
static int time_elapsed = 0;
static char traffic_fn[MAX_LOG_FN_SIZE];
static char debug_fn[MAX_LOG_FN_SIZE];
static char tncpi9k6_fn[MAX_LOG_FN_SIZE];
static int prev_yday;

int g_debug_log_enable;
int g_tncpi9k6_log_enable;
int g_traffic_log_enable;
char g_log_dir_path[MAX_DIR_PATH_SIZE];

void log_write_debug_log()
{
    FILE *debug_fp;
    char *p;

    debug_fp = fopen(debug_fn, "a");
    if (debug_fp != NULL) {
        pthread_mutex_lock(&mutex_debug_log);
        p = cmdq_pop(&g_debug_log_q);
        while (p) {
            fprintf(debug_fp, "%s\n",  p);
            p = cmdq_pop(&g_debug_log_q);
        }
        pthread_mutex_unlock(&mutex_debug_log);
        fclose(debug_fp);
    }
}

void log_write_tncpi9k6_log()
{
    FILE *tncpi9k6_fp;
    char *p;

    tncpi9k6_fp = fopen(tncpi9k6_fn, "a");
    if (tncpi9k6_fp != NULL) {
        pthread_mutex_lock(&mutex_tncpi9k6_log);
        p = cmdq_pop(&g_tncpi9k6_log_q);
        while (p) {
            fprintf(tncpi9k6_fp, "%s\n",  p);
            p = cmdq_pop(&g_tncpi9k6_log_q);
        }
        pthread_mutex_unlock(&mutex_tncpi9k6_log);
        fclose(tncpi9k6_fp);
    }
}

void log_write_traffic_log()
{
    FILE *traffic_fp;
    char *p;
    size_t len;

    traffic_fp = fopen(traffic_fn, "a");
    if (traffic_fp != NULL) {
        pthread_mutex_lock(&mutex_traffic_log);
        p = dataq_pop(&g_traffic_log_q);
        while (p) {
            len = strlen(p);
            if (p[len - 1] == '\n')
                p[len - 1] = '\0';
            if (p[len - 2] == '\r')
                p[len - 2] = '\0';
            fprintf(traffic_fp, "%s\n", p);
            p = dataq_pop(&g_traffic_log_q);
        }
        pthread_mutex_unlock(&mutex_traffic_log);
        fclose(traffic_fp);
    }
}

void log_close()
{
    if (g_traffic_log_enable)
        log_write_traffic_log();
    if (g_debug_log_enable)
        log_write_debug_log();
    if (g_tncpi9k6_log_enable)
        log_write_tncpi9k6_log();
}

void log_on_alarm()
{
    time_t t;
    struct tm *utc;
    char datestamp[MAX_TIMESTAMP_SIZE];
    int numch;

    t = time(NULL);
    utc = gmtime(&t);
    if (utc->tm_yday > prev_yday || (!utc->tm_yday && prev_yday)) {
        prev_yday = utc->tm_yday;
        /* new day, rotate logs */
        numch = snprintf(traffic_fn, sizeof(traffic_fn), "%s/traffic-%s.log",
                         g_log_dir_path, util_datestamp(datestamp, sizeof(datestamp)));
        numch = snprintf(debug_fn, sizeof(debug_fn), "%s/debug-%s.log",
                         g_log_dir_path, util_datestamp(datestamp, sizeof(datestamp)));
        numch = snprintf(tncpi9k6_fn, sizeof(tncpi9k6_fn), "%s/tncpi9k6-%s.log",
                         g_log_dir_path, util_datestamp(datestamp, sizeof(datestamp)));
        pthread_mutex_lock(&mutex_df_error_log);
        numch =snprintf(g_df_error_fn, sizeof(g_df_error_fn), "%s/dyn-file-error-%s.log",
                        g_log_dir_path, util_datestamp(datestamp, sizeof(datestamp)));
        pthread_mutex_unlock(&mutex_df_error_log);
    }
    /* called every 6 secs (1/10 minute granularity) */
    if (time_elapsed && --time_elapsed == 0) {
        /* reset timer */
        time_elapsed = time_interval;
        if (g_traffic_log_enable)
            log_write_traffic_log();
        if (g_debug_log_enable)
            log_write_debug_log();
        if (g_tncpi9k6_log_enable)
            log_write_tncpi9k6_log();
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
}

int log_init(int which_tnc)
{
    FILE *fp;
    DIR *dirp;
    char datestamp[MAX_TIMESTAMP_SIZE], timestamp[MAX_TIMESTAMP_SIZE];
    int numch;

    time_interval = (LOG_WRITE_INTERVAL_SEC / ALARM_INTERVAL_SEC);
    time_elapsed = time_interval;

    /* set up log directory, tnc settings override global settings if enabled */
    if (!strncasecmp(g_tnc_settings[which_tnc].traffic_en, "TRUE", 4) ||
        !strncasecmp(g_tnc_settings[which_tnc].debug_en, "TRUE", 4)   ||
        !strncasecmp(g_tnc_settings[which_tnc].tncpi9k6_en, "TRUE", 4)) {
            snprintf(g_log_dir_path, sizeof(g_log_dir_path), "%s", g_tnc_settings[which_tnc].log_dir);
    } else {
        snprintf(g_log_dir_path, sizeof(g_log_dir_path), "%s/%s", g_arim_path, "log");
    }
    dirp = opendir(g_log_dir_path);
    if (!dirp) {
        if (errno == ENOENT && mkdir(g_log_dir_path, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1)
            return 0;
    } else {
        closedir(dirp);
    }
    /* set up traffic log if enabled, tnc settings override global settings */
    if (!strncasecmp(g_tnc_settings[which_tnc].traffic_en, "TRUE", 4) ||
            !strncasecmp(g_log_settings.traffic_en, "TRUE", 4)) {
        g_traffic_log_enable = 1;
        numch = snprintf(traffic_fn, sizeof(traffic_fn), "%s/traffic-%s.log",
                         g_log_dir_path, util_datestamp(datestamp, sizeof(datestamp)));
        fp = fopen(traffic_fn, "a");
        if (fp == NULL) {
            g_traffic_log_enable = 0;
            return 0;
        } else {
            fprintf(fp, "\n[%s] %s\n",
                    util_timestamp(timestamp, sizeof(timestamp)),
                        "--- Session start, initializing log ---");
            fclose(fp);
        }
    }
    /* set up debug log if enabled, tnc settings override global settings */
    if (!strncasecmp(g_tnc_settings[which_tnc].debug_en, "TRUE", 4) ||
            !strncasecmp(g_log_settings.debug_en, "TRUE", 4)) {
        g_debug_log_enable = 1;
        numch = snprintf(debug_fn, sizeof(debug_fn), "%s/debug-%s.log",
                         g_log_dir_path, util_datestamp(datestamp, sizeof(datestamp)));
        fp = fopen(debug_fn, "a");
        if (fp == NULL) {
            g_debug_log_enable = 0;
            return 0;
        } else {
            fprintf(fp, "\n[%s] %s\n",
                    util_timestamp_usec(timestamp, sizeof(timestamp)),
                        "--- Session start, initializing log ---");
            fclose(fp);
        }
    }
    /* set up tncpi9k6 log if enabled, tnc settings override global settings */
    if (!strncasecmp(g_tnc_settings[which_tnc].tncpi9k6_en, "TRUE", 4) ||
            !strncasecmp(g_log_settings.tncpi9k6_en, "TRUE", 4)) {
        g_tncpi9k6_log_enable = 1;
        numch = snprintf(tncpi9k6_fn, sizeof(tncpi9k6_fn), "%s/tncpi9k6-%s.log",
                         g_log_dir_path, util_datestamp(datestamp, sizeof(datestamp)));
        fp = fopen(tncpi9k6_fn, "a");
        if (fp == NULL) {
            g_tncpi9k6_log_enable = 0;
            return 0;
        } else {
            fprintf(fp, "\n[%s] %s\n",
                    util_timestamp_usec(timestamp, sizeof(timestamp)),
                        "--- Session start, initializing log ---");
            fclose(fp);
        }
    }
    numch = snprintf(g_df_error_fn, sizeof(g_df_error_fn), "%s/dyn-file-error-%s.log",
                     g_log_dir_path, util_datestamp(datestamp, sizeof(datestamp)));
    fp = fopen(g_df_error_fn, "a");
    if (fp == NULL) {
        return 0;
    } else {
        fprintf(fp, "\n[%s] %s\n",
                util_timestamp_usec(timestamp, sizeof(timestamp)),
                    "--- Session start, initializing log ---");
        fclose(fp);
    }
    (void)numch; /* suppress 'assigned but not used' warning for dummy var */
    return 1;
}

