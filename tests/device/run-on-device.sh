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
# Functional test of a running notificationmgr, over the bus, on the device
# itself. Run it there (or via adb shell). Needs luna-send, which every LuneOS
# image has. Exercises the paths the herrie/fixes audit touched:
#
#   - createToast round trip
#   - the per-app block list: disableToast, getToastSettings, the block
#     actually blocking, enableToast lifting it
#   - closeToast by id
#   - displayId bounds on createToast/getToastCount/setToastStatus
#   - a db8-injection shaped sourceId staying inert
#
# Prints PASS/FAIL per case and exits non-zero if anything failed.

set -u

LUNA="luna-send -n 1 -a com.webos.surfacemanager"
SRC="com.webos.app.tests.notification"
FAILED=0

# disableToast/enableToast only accept a source that is an installed
# application - "Unknown Source ID" otherwise - so the block-list cases use a
# real one from this device.
APP=$($LUNA "luna://com.webos.applicationManager/listApps" '{}' 2>&1     | sed -n 's/.*"id":"\(com\.palm\.app\.[a-z]*\)".*/\1/p' | sed -n 1p)
if [ -z "$APP" ]; then
    echo "could not find an installed application to block; is SAM up?"
    exit 1
fi
echo "using installed app: $APP"

say() { echo "== $1"; }

check() {
    # check <name> <expected-substring> <method> <payload>
    # method "check_file_contains" reads the preferences file instead.
    name="$1"; want="$2"; method="$3"; payload="${4:-}"
    if [ "$method" = "check_file_contains" ]; then
        out=$(cat /var/luna/preferences/notification-blocked-apps.json 2>&1)
    else
        out=$($LUNA "luna://com.webos.notification/$method" "$payload" 2>&1)
    fi
    case "$out" in
        *"$want"*) echo "PASS: $name" ;;
        *) echo "FAIL: $name"
           echo "      wanted: $want"
           echo "      got:    $out"
           FAILED=$((FAILED+1)) ;;
    esac
}

say "createToast round trip"
check "createToast accepts a message" '"returnValue":true' \
    createToast "{\"sourceId\":\"$SRC\",\"message\":\"harness: hello\"}"

say "the per-app block list"
check "disableToast for an installed app" '"returnValue":true' \
    disableToast "{\"source\":\"$APP\"}"
check "getToastSettings lists it" "\"$APP\"" \
    getToastSettings "{}"
check "a blocked app's toast is refused" "Toast is blocked for $APP" \
    createToast "{\"sourceId\":\"$APP\",\"message\":\"harness: should not appear\"}"
check "the block survives in the file" "$APP" \
    check_file_contains
check "enableToast lifts it" '"returnValue":true' \
    enableToast "{\"source\":\"$APP\"}"
check "getToastSettings no longer lists it" '"blockedApps":[]' \
    getToastSettings "{}"
check "the app's toast goes through again" '"returnValue":true' \
    createToast "{\"sourceId\":\"$APP\",\"message\":\"harness: hello again\"}"

say "closeToast"
TOASTID=$($LUNA "luna://com.webos.notification/createToast" \
    "{\"sourceId\":\"$SRC\",\"message\":\"harness: to close\"}" 2>&1 \
    | sed -n 's/.*"toastId":"\([^"]*\)".*/\1/p')
if [ -n "$TOASTID" ]; then
    check "closeToast by id" '"returnValue":true' \
        closeToast "{\"toastId\":\"$TOASTID\"}"
else
    echo "FAIL: closeToast by id (no toastId to close)"
    FAILED=$((FAILED+1))
fi

say "displayId stays inside the device"
check "createToast rejects displayId 2" 'Invalid displayId' \
    createToast "{\"sourceId\":\"$SRC\",\"message\":\"x\",\"displayId\":2}"
check "createToast rejects displayId 100000000" 'Invalid displayId' \
    createToast "{\"sourceId\":\"$SRC\",\"message\":\"x\",\"displayId\":100000000}"
check "createToast rejects a negative displayId" 'Invalid displayId' \
    createToast "{\"sourceId\":\"$SRC\",\"message\":\"x\",\"displayId\":-1}"
check "getToastCount rejects displayId 7" 'Invalid displayId' \
    getToastCount "{\"displayId\":7}"
check "setToastStatus rejects displayId 9" 'Invalid displayId' \
    setToastStatus "{\"toastId\":\"$SRC-1\",\"readStatus\":true,\"displayId\":9}"
check "getToastList rejects displayId 5" 'Invalid displayId' \
    getToastList "{\"displayId\":5}"

say "a hostile sourceId stays a value"
# The shape that used to rewrite a db8 purge. It must be treated as an odd
# name, never as query syntax; the call itself may succeed or fail politely,
# but the service has to answer and survive.
HOSTILE='x\",\"op\":\"!=\",\"val\":\"y'
check "closeToast with a hostile sourceId answers" '"returnValue"' \
    closeToast "{\"sourceId\":\"$HOSTILE\"}"
check "the service is still there" '"returnValue":true' \
    createToast "{\"sourceId\":\"$SRC\",\"message\":\"harness: alive\"}"

echo
if [ "$FAILED" -eq 0 ]; then
    echo "all device checks passed"
    exit 0
fi
echo "$FAILED device check(s) FAILED"
exit 1
