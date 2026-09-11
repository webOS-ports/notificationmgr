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

#include <History.h>
#include <JUtil.h>

#include <pbnjson.hpp>

namespace
{

std::string serialize(const pbnjson::JValue &v)
{
    return JUtil::jsonToString(v);
}

//! Parse a query back and walk to its single where clause.
pbnjson::JValue firstClause(const pbnjson::JValue &query)
{
    return query["query"]["where"][0];
}

} // namespace

TEST(Db8Query, buildsTheShapeDb8Expects)
{
    pbnjson::JValue q = History::findQuery(
        History::whereClause("sourceId", pbnjson::JValue("com.webos.app.foo")));

    CHECK_EQ(q["query"]["from"].asString(), std::string("com.webos.notificationhistory:1"));
    CHECK(q["query"]["where"].isArray());
    CHECK_EQ(firstClause(q)["prop"].asString(), std::string("sourceId"));
    CHECK_EQ(firstClause(q)["op"].asString(), std::string("="));
    CHECK_EQ(firstClause(q)["val"].asString(), std::string("com.webos.app.foo"));
    CHECK(!q.hasKey("purge"));
}

TEST(Db8Query, purgeSetsThePurgeFlag)
{
    pbnjson::JValue q = History::purgeQuery(
        History::whereClause("timestamp", pbnjson::JValue("1757500000000")));

    CHECK(q["purge"].asBool());
}

TEST(Db8Query, carriesTheOperatorThrough)
{
    pbnjson::JValue q = History::purgeQuery(
        History::whereClause("schedule.expire", pbnjson::JValue(static_cast<int64_t>(1757500000)), "<"));

    CHECK_EQ(firstClause(q)["op"].asString(), std::string("<"));
    CHECK_EQ(firstClause(q)["val"].asNumber<int64_t>(), static_cast<int64_t>(1757500000));
}

TEST(Db8Query, keepsNonStringValuesTyped)
{
    pbnjson::JValue q = History::findQuery(
        History::whereClause("displayId", pbnjson::JValue(1)));
    CHECK(firstClause(q)["val"].isNumber());

    q = History::findQuery(
        History::whereClause("saveRemoteNotification", pbnjson::JValue(false)));
    CHECK(firstClause(q)["val"].isBoolean());
    CHECK_EQ(firstClause(q)["val"].asBool(), false);
}

/*
 * These queries used to be assembled with g_strdup_printf and operator+, and
 * everything that reaches them is caller supplied. A value with a quote in it
 * did not produce a malformed query, it produced a different one.
 */
TEST(Db8Query, aQuoteInTheValueStaysInTheValue)
{
    const std::string hostile = "x\",\"op\":\"!=\",\"val\":\"y";

    pbnjson::JValue q = History::purgeQuery(
        History::whereClause("sourceId", pbnjson::JValue(hostile)));

    // The clause still says "=", and the whole payload is still one clause.
    CHECK_EQ(firstClause(q)["op"].asString(), std::string("="));
    CHECK_EQ(firstClause(q)["val"].asString(), hostile);
    CHECK_EQ(q["query"]["where"].arraySize(), static_cast<ssize_t>(1));

    // And it survives a round trip through the generator unchanged.
    std::string text = serialize(q);
    CHECK_STR_LACKS(text, "\"op\":\"!=\"");

    pbnjson::JValue reparsed = JUtil::parse(text.c_str(), "", NULL);
    CHECK(!reparsed.isNull());
    CHECK_EQ(firstClause(reparsed)["val"].asString(), hostile);
    CHECK_EQ(firstClause(reparsed)["op"].asString(), std::string("="));
}

TEST(Db8Query, survivesBackslashesAndControlCharacters)
{
    const std::string hostile = "a\\\"b\nc\td\\";

    pbnjson::JValue q = History::purgeQuery(
        History::whereClause("notiId", pbnjson::JValue(hostile)));

    std::string text = serialize(q);
    pbnjson::JValue reparsed = JUtil::parse(text.c_str(), "", NULL);
    CHECK(!reparsed.isNull());
    CHECK_EQ(firstClause(reparsed)["val"].asString(), hostile);
}

TEST(Db8Query, aQuoteInThePropertyNameStaysThereToo)
{
    const std::string hostile = "sourceId\",\"op\":\"!=\",\"x\":\"";

    pbnjson::JValue q = History::purgeQuery(
        History::whereClause(hostile, pbnjson::JValue("v")));

    CHECK_EQ(firstClause(q)["prop"].asString(), hostile);
    CHECK_EQ(firstClause(q)["op"].asString(), std::string("="));
}
