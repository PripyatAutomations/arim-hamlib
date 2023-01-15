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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <getopt.h>
#include "main.h"
#include "cmdthread.h"
#include "datathread.h"
#include "ini.h"
#include "ui.h"
#include "log.h"
#include "arim_beacon.h"
#include "mbox.h"
#include "auth.h"

int g_cmdthread_stop;
int g_cmdthread_ready;
int g_datathread_stop;
int g_datathread_ready;
int g_serialthread_stop;
int g_serialthread_ready;
pthread_t g_cmdthread;
pthread_t g_datathread;
pthread_t g_serialthread;

int g_tnc_attached;
int g_win_changed;
int g_new_install;
int g_print_config;

static time_t prev_time;
static int timerthread_stop;

pthread_mutex_t mutex_title = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_status = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cmd_in = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cmd_out = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_data_in = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_data_out = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_heard = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_debug_log = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tncpi9k6_log = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_df_error_log = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_traffic_log = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_recents = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ptable = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ctable = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ftable = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_time = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tnc_set = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_file_out = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_msg_out = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tnc_busy = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_num_bytes = PTHREAD_MUTEX_INITIALIZER;

void sighandler(int sig, siginfo_t *siginfo, void *context)
{
    switch (sig) {
    case SIGWINCH:
        g_win_changed = 1;
        break;
    }
}

void *timerthread_func(void *data)
{
    time_t cur_time;

    do {
        cur_time = time((time_t *)0);
        if (cur_time - prev_time >= ALARM_INTERVAL_SEC) {
            prev_time = cur_time;
            if (g_tnc_attached) {
                arim_beacon_on_alarm();
            }
            log_on_alarm();
        }
        usleep(100000);
    } while (!timerthread_stop);
    return data;
}

int main(int argc, char *argv[])
{
    int result;
    struct sigaction action;
    pthread_t timerthread;
    int option;

    static struct option long_options[] = {
        {"version",      0, 0, 'v'},
        {"config-file",  1, 0, 'f'},
        {"print-conf",   1, 0, 'p'},
        {"help",         0, 0, 'h'},
        {0,              0, 0,  0 }
    };

    while ((option = getopt_long(argc, argv, "vf:p:h", long_options, NULL)) != -1) {
        switch (option) {
        case 'f':
            snprintf(g_config_fname, MAX_PATH_SIZE, "%s", optarg);
            g_config_clo = 1;
            break;
        case 'p':
            snprintf(g_print_config_fname, MAX_PATH_SIZE, "%s", optarg);
            g_print_config = 1;
            break;
        case 'v':
            printf("ARIM %s\nCopyright 2016-2021 Robert Cunnings NW8L\n"
                   "\nLicense GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
                   "This is free software: you are free to change and redistribute it.\n"
                   "There is NO WARRANTY, to the extent permitted by law.\n", ARIM_VERSION);
            return 0;
            break;
        default:
            printf("Usage: %s [OPTION]\n"
                   "  -v, --version            print version information\n"
                   "  -f, --config-file FILE   use configuration file FILE\n"
                   "  -p, --print-conf FILE    print configuration file listing to FILE\n"
                   "  -h, --help               print this option help message\n",
                   argv[0]);
            return 0;
        }
    }
    memset(&action, '\0', sizeof(action));
    action.sa_sigaction = &sighandler;
    action.sa_flags = SA_SIGINFO;
    if (sigaction(SIGWINCH, &action, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    /* read in the settings */
    if (!ini_read_settings()) {
        printf("Error: cannot open .ini file\n");
        return 2;
    }
    /* initialize mailbox files */
    if (!mbox_init()) {
        printf("Error: cannot initialize mailbox files\n");
        return 3;
    }
    /* initialize password file */
    if (!auth_init()) {
        printf("Error: cannot initialize password file\n");
        return 4;
    }
    /* initialize log directory */
    snprintf(g_log_dir_path, MAX_DIR_PATH_SIZE, "%s/%s", g_arim_path, "log");
    /* create the timer thread */
    result = pthread_create(&timerthread, NULL, timerthread_func, NULL);
    if (result) {
        perror("pthread_create");
        return 6;
    }
    /* initialize the ui */
    ui_init();
    sleep(1);
    /* start the ui command loop, will return on "quit" command */
    ui_run();
    if (g_cmdthread) {
        g_cmdthread_stop = 1;
        pthread_join(g_cmdthread, NULL);
    }
    if (g_datathread) {
        g_datathread_stop = 1;
        pthread_join(g_datathread, NULL);
    }
    /* end the ui */
    ui_end();
    /* flush queued events to logs */
    log_close();
    /* kill the timer thread */
    timerthread_stop = 1;
    pthread_join(timerthread, NULL);

    exit(0);
}

