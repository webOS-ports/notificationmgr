// Copyright (c) 2013-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <glib.h>
#include <glib-unix.h>
#include <errno.h>

#include "Logging.h"
#include "NotificationService.h"

static GMainLoop * s_main_loop = NULL;

/*
 * Runs from the main loop, not from the signal handler. The previous version
 * called g_main_loop_quit() straight out of a SIGTERM handler; nothing in glib
 * is async-signal-safe, so a signal arriving while the loop held its own lock
 * deadlocked or corrupted it. g_unix_signal_add() catches the signal and
 * dispatches this as an ordinary source.
 */
static gboolean
term_handler(gpointer user_data)
{
    LOG_DEBUG("shutting down on signal");
    g_main_loop_quit(s_main_loop);
    return G_SOURCE_REMOVE;
}

int
main(int argc, char **argv)
{
    LOG_DEBUG("entering %s in %s", __func__, __FILE__ );

    /* A bus peer going away while a reply is being written would otherwise
     * take the daemon with it. */
    signal(SIGPIPE, SIG_IGN);

    s_main_loop = g_main_loop_new(NULL, FALSE);

    g_unix_signal_add(SIGTERM, term_handler, NULL);
    g_unix_signal_add(SIGINT, term_handler, NULL);

    /* Registering on the bus is the whole job. Carrying on without it left a
     * process sitting in a main loop with nothing attached to it, which
     * systemd has no way to tell apart from a working service. */
    if (!NotificationService::instance()->attach(s_main_loop))
    {
        LOG_ERROR(MSGID_SERVICE_REG_ERR, 0, "Could not attach to the bus, giving up");
        g_main_loop_unref(s_main_loop);
        return EXIT_FAILURE;
    }

    g_main_loop_run(s_main_loop);

    NotificationService::instance()->detach();

    g_main_loop_unref(s_main_loop);

    LOG_DEBUG("exiting %s in %s", __func__, __FILE__ );

    return EXIT_SUCCESS;
}
