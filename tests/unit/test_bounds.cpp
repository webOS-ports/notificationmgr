// Copyright (c) 2026 Herman van Hazendonk <github.com@herrie.org>
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

#include "check.h"

#include <NotificationService.h>
#include <JsonParser.h>
#include <Utils.h>

#include <limits.h>

/*
 * toastCountVector has NUM_DISPLAYS entries and createToast,
 * removeAllNotification, setToastStatus and getToastList index it with a
 * number out of the request payload. The old guard was "displayId >= 0".
 */
TEST(DisplayId, acceptsOnlyDisplaysTheDeviceHas)
{
    CHECK(NotificationService::isValidDisplayId(0));
    CHECK(NotificationService::isValidDisplayId(NUM_DISPLAYS - 1));
}

TEST(DisplayId, rejectsEverythingOutsideTheArray)
{
    CHECK(!NotificationService::isValidDisplayId(NUM_DISPLAYS));
    CHECK(!NotificationService::isValidDisplayId(-1));
    CHECK(!NotificationService::isValidDisplayId(INT_MIN));
    CHECK(!NotificationService::isValidDisplayId(INT_MAX));
    CHECK(!NotificationService::isValidDisplayId(100000000));
}

/*
 * createActionInfo() split the uri on find_last_of("/") into an unsigned, so
 * npos truncated to 0xffffffff and substr(0, found + 1) wrapped to an empty
 * string.
 */
TEST(ActionInfo, splitsAServiceUri)
{
    pbnjson::JValue src = pbnjson::Object();
    src.put("uri", "luna://com.webos.service.x/doThing");

    pbnjson::JValue action = JsonParser::createActionInfo(src);
    CHECK_EQ(action["serviceURI"].asString(), std::string("luna://com.webos.service.x/"));
    CHECK_EQ(action["serviceMethod"].asString(), std::string("doThing"));
}

TEST(ActionInfo, refusesAUriItCannotSplit)
{
    pbnjson::JValue src = pbnjson::Object();

    // No scheme.
    src.put("uri", "com.webos.service.x/doThing");
    CHECK(!JsonParser::createActionInfo(src).hasKey("serviceURI"));

    // Nothing at all.
    src.put("uri", "");
    CHECK(!JsonParser::createActionInfo(src).hasKey("serviceURI"));

    // A null source is the "no onclose given" case and must not crash.
    CHECK(!JsonParser::createActionInfo(pbnjson::JValue()).hasKey("serviceURI"));
}

TEST(ActionInfo, carriesParamsWhenThereAreSome)
{
    pbnjson::JValue params = pbnjson::Object();
    params.put("key", "value");

    pbnjson::JValue src = pbnjson::Object();
    src.put("uri", "luna://com.webos.service.x/doThing");
    src.put("params", params);

    pbnjson::JValue action = JsonParser::createActionInfo(src);
    CHECK_EQ(action["launchParams"]["key"].asString(), std::string("value"));
}
