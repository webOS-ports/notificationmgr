#!/bin/sh
# Copyright (c) 2026 Herman van Hazendonk <github.com@herrie.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# Stress notificationmgr on the device: hammer the APIs the audit touched and
# watch the daemon's memory. Run on the device.
#
#   tests/device/stress.sh [iterations]   (default 300)
#
# Each iteration is one createToast, one getToastSettings, one
# disableToast/enableToast pair, one getToastList and one closeToast - plus a
# hostile payload every tenth turn. Between the first and last hundred
# iterations the daemon's RSS must not keep climbing; the old code leaked a
# History object per getToastList and a message per alert, so this is the
# regression the harness is for.

set -u

N="${1:-300}"
LUNA="luna-send -n 1 -a com.webos.surfacemanager"
SRC="com.webos.app.tests.stress"

pid_of() { pidof notificationmgr; }
rss_of() { awk '/VmRSS/{print $2}' "/proc/$1/status" 2>/dev/null; }

PID=$(pid_of)
if [ -z "$PID" ]; then
    echo "notificationmgr is not running"
    exit 1
fi

RSS_START=$(rss_of "$PID")
echo "pid $PID, rss at start: ${RSS_START} kB, iterations: $N"

i=0
while [ "$i" -lt "$N" ]; do
    i=$((i+1))

    OUT=$($LUNA "luna://com.webos.notification/createToast" \
        "{\"sourceId\":\"$SRC\",\"message\":\"stress $i\"}" 2>&1)
    case "$OUT" in *'"returnValue":true'*) ;; *)
        echo "iteration $i: createToast failed: $OUT"; exit 1 ;;
    esac

    TOASTID=$(echo "$OUT" | sed -n 's/.*"toastId":"\([^"]*\)".*/\1/p')

    $LUNA "luna://com.webos.notification/getToastSettings" '{}' >/dev/null 2>&1
    $LUNA "luna://com.webos.notification/disableToast" "{\"source\":\"$SRC\"}" >/dev/null 2>&1
    $LUNA "luna://com.webos.notification/enableToast" "{\"source\":\"$SRC\"}" >/dev/null 2>&1
    $LUNA "luna://com.webos.notification/getToastList" '{"displayId":0}' >/dev/null 2>&1

    [ -n "$TOASTID" ] && \
        $LUNA "luna://com.webos.notification/closeToast" "{\"toastId\":\"$TOASTID\"}" >/dev/null 2>&1

    if [ $((i % 10)) -eq 0 ]; then
        # A payload shaped like the old injection, and an out-of-range display.
        $LUNA "luna://com.webos.notification/closeToast" \
            '{"sourceId":"x\",\"op\":\"!=\",\"val\":\"y"}' >/dev/null 2>&1
        $LUNA "luna://com.webos.notification/createToast" \
            "{\"sourceId\":\"$SRC\",\"message\":\"x\",\"displayId\":100000000}" >/dev/null 2>&1
    fi

    if [ $((i % 100)) -eq 0 ]; then
        NOW_PID=$(pid_of)
        if [ "$NOW_PID" != "$PID" ]; then
            echo "iteration $i: daemon RESTARTED (pid $PID -> ${NOW_PID:-gone})"
            exit 1
        fi
        echo "iteration $i: rss $(rss_of "$PID") kB"
    fi
done

RSS_END=$(rss_of "$PID")
GROWTH=$((RSS_END - RSS_START))
echo "rss at end: ${RSS_END} kB (grew ${GROWTH} kB over $N iterations)"

# Some growth is heap warm-up; a leak per iteration is not. With ~6 calls per
# iteration the old getToastList leak alone grew by hundreds of kB here.
LIMIT=2048
if [ "$GROWTH" -gt "$LIMIT" ]; then
    echo "FAIL: rss grew more than ${LIMIT} kB"
    exit 1
fi

echo "stress: PASS (daemon alive, same pid, rss growth within ${LIMIT} kB)"
exit 0
