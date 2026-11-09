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

/*
 * A toast's groupId - which of an application's toasts the shell shows as one
 * entry - on its way from createToast to the shell, and back out of the toast
 * history through getToastList.
 */

#include "check.h"

#include <History.h>
#include <JUtil.h>
#include <NotificationService.h>

#include <pbnjson.hpp>

namespace
{

pbnjson::JValue parse(const char *json)
{
    return pbnjson::JDomParser::fromString(json);
}

pbnjson::JValue toastFrom(const char *request)
{
    pbnjson::JValue toast = pbnjson::Object();
    NotificationService::putGroupId(parse(request), toast);
    return toast;
}

} // namespace

TEST(ToastGroup, createToastPassesGroupIdOn)
{
    pbnjson::JValue toast = toastFrom(R"({"message":"m","groupId":"account-work"})");
    CHECK_EQ(toast["groupId"].asString(), std::string("account-work"));
}

TEST(ToastGroup, noGroupIdMeansNone)
{
    CHECK(!toastFrom(R"({"message":"m"})").hasKey("groupId"));
}

// An empty one would put the toast in a group of its own, apart from the
// application's other toasts without one.
TEST(ToastGroup, emptyGroupIdIsLeftOut)
{
    CHECK(!toastFrom(R"({"message":"m","groupId":""})").hasKey("groupId"));
}

TEST(ToastGroup, groupIdThatIsNotAStringIsLeftOut)
{
    CHECK(!toastFrom(R"({"message":"m","groupId":7})").hasKey("groupId"));
    CHECK(!toastFrom(R"({"message":"m","groupId":{"a":1}})").hasKey("groupId"));
}

TEST(ToastHistory, keepsTheGroupId)
{
    pbnjson::JValue info = History::toastInfo(parse(
        R"({"sourceId":"com.palm.app.email","timestamp":"1794154685801","groupId":"account-work"})"));
    CHECK_EQ(info["groupId"].asString(), std::string("account-work"));
}

TEST(ToastHistory, aToastWithoutAGroupIdHasNone)
{
    pbnjson::JValue info = History::toastInfo(parse(
        R"({"sourceId":"com.palm.app.email","timestamp":"1794154685801"})"));
    CHECK(!info.hasKey("groupId"));
}

// createToast saves no toastId; it answers with sourceId-timestamp, and the
// history gives the same one back.
TEST(ToastHistory, toastIdIsTheOneCreateToastGave)
{
    pbnjson::JValue info = History::toastInfo(parse(
        R"({"sourceId":"com.palm.app.email","timestamp":"1794154685801"})"));
    CHECK_EQ(info["toastId"].asString(), std::string("com.palm.app.email-1794154685801"));
}

// A record that has one keeps its own, not its notiId.
TEST(ToastHistory, savedToastIdIsKept)
{
    pbnjson::JValue info = History::toastInfo(parse(
        R"({"sourceId":"s","timestamp":"1","toastId":"s-own","notiId":"s-noti"})"));
    CHECK_EQ(info["toastId"].asString(), std::string("s-own"));
}

TEST(ToastHistory, copiesTheFieldsTheShellReads)
{
    pbnjson::JValue info = History::toastInfo(parse(R"({
        "sourceId":"s","timestamp":"1","iconUrl":"u","iconPath":"p","title":"t",
        "message":"m","isSysReq":true,"displayId":0,"user":"guest",
        "schedule":{"expire":2},"type":"standard","action":{"launchParams":{}},
        "readStatus":false})"));
    CHECK_EQ(info["sourceId"].asString(), std::string("s"));
    CHECK_EQ(info["timestamp"].asString(), std::string("1"));
    CHECK_EQ(info["iconUrl"].asString(), std::string("u"));
    CHECK_EQ(info["iconPath"].asString(), std::string("p"));
    CHECK_EQ(info["title"].asString(), std::string("t"));
    CHECK_EQ(info["message"].asString(), std::string("m"));
    CHECK(info["isSysReq"].asBool());
    CHECK_EQ(info["displayId"].asNumber<int>(), 0);
    CHECK_EQ(info["user"].asString(), std::string("guest"));
    CHECK_EQ(info["schedule"]["expire"].asNumber<int>(), 2);
    CHECK_EQ(info["type"].asString(), std::string("standard"));
    CHECK(info["action"].isObject());
    CHECK(info.hasKey("readStatus"));
    CHECK(!info["readStatus"].asBool());
}

// Only what the record has: no nulls for the shell to trip over.
TEST(ToastHistory, leavesOutWhatTheRecordLacks)
{
    pbnjson::JValue info = History::toastInfo(parse(R"({"sourceId":"s","timestamp":"1"})"));
    CHECK(!info.hasKey("title"));
    CHECK(!info.hasKey("iconUrl"));
    CHECK(!info.hasKey("schedule"));
    CHECK(!info.hasKey("readStatus"));
}

// db8's own fields stay in the database.
TEST(ToastHistory, leavesOutDb8Fields)
{
    pbnjson::JValue info = History::toastInfo(parse(
        R"({"_id":"x","_kind":"k","_rev":3,"sourceId":"s","timestamp":"1"})"));
    CHECK(!info.hasKey("_id"));
    CHECK(!info.hasKey("_kind"));
    CHECK(!info.hasKey("_rev"));
}
