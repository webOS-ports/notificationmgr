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

#include <Utils.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace
{

//! Write content to a fresh file and return its path.
std::string writeTemp(const std::string &name, const std::string &content)
{
    std::string path = std::string("/tmp/notificationmgr-test-") + name;
    FILE *fp = fopen(path.c_str(), "w");
    if (!fp)
        return std::string();

    fwrite(content.data(), 1, content.size(), fp);
    fclose(fp);
    return path;
}

} // namespace

/*
 * readFile() used to terminate the buffer over the last byte of the content,
 * so the final character of every file it read was thrown away. The json
 * files it is pointed at end in a newline, which is why nobody noticed.
 */
TEST(Utils, readFileKeepsTheLastByte)
{
    std::string path = writeTemp("lastbyte", "{\"a\":1}");
    CHECK(!path.empty());

    char *data = Utils::readFile(path.c_str());
    CHECK(data != NULL);
    if (data)
    {
        CHECK_EQ(std::string(data), std::string("{\"a\":1}"));
        delete[] data;
    }
    unlink(path.c_str());
}

TEST(Utils, readFileTerminatesTheBuffer)
{
    std::string path = writeTemp("terminated", "abc");
    char *data = Utils::readFile(path.c_str());
    CHECK(data != NULL);
    if (data)
    {
        CHECK_EQ(strlen(data), static_cast<size_t>(3));
        delete[] data;
    }
    unlink(path.c_str());
}

TEST(Utils, readFileRejectsNonsense)
{
    CHECK(Utils::readFile(NULL) == NULL);
    CHECK(Utils::readFile("/nonexistent/notificationmgr/nope.json") == NULL);

    std::string empty = writeTemp("empty", "");
    CHECK(Utils::readFile(empty.c_str()) == NULL);
    unlink(empty.c_str());

    // A directory opens but has nothing to read.
    CHECK(Utils::readFile("/tmp") == NULL);
}

TEST(Utils, readFileRoundTripsEveryByteValue)
{
    std::string content;
    for (int i = 1; i < 256; ++i)
    {
        if (i == '\0')
            continue;
        content.push_back(static_cast<char>(i));
    }

    std::string path = writeTemp("allbytes", content);
    char *data = Utils::readFile(path.c_str());
    CHECK(data != NULL);
    if (data)
    {
        CHECK_EQ(std::string(data), content);
        delete[] data;
    }
    unlink(path.c_str());
}

/*
 * A toastId is sourceId + "-" + timestamp and sourceIds have dashes of their
 * own, so the timestamp is what follows the last one.
 */
TEST(Utils, extractTimestampTakesTheLastField)
{
    CHECK_EQ(Utils::extractTimestampFromId("com.webos.app.foo-1757500000000"),
             std::string("1757500000000"));
    CHECK_EQ(Utils::extractTimestampFromId("com.webos.app.notification-11458-1757500000000"),
             std::string("1757500000000"));
    CHECK_EQ(Utils::extractTimestampFromId("no-dashes-here"), std::string("here"));
    CHECK_EQ(Utils::extractTimestampFromId("nodashesatall"), std::string(""));
    CHECK_EQ(Utils::extractTimestampFromId(""), std::string(""));
    CHECK_EQ(Utils::extractTimestampFromId("trailing-"), std::string(""));
}

TEST(Utils, extractSourceIdSplitsOnTheLastSpace)
{
    CHECK_EQ(Utils::extractSourceIdFromCaller("com.webos.app.foo"),
             std::string("com.webos.app.foo"));
    CHECK_EQ(Utils::extractSourceIdFromCaller("com.webos.app.foo 1234"),
             std::string("com.webos.app.foo"));
    CHECK_EQ(Utils::extractSourceIdFromCaller(""), std::string(""));
}

TEST(Utils, isValidURIWantsAScheme)
{
    CHECK(Utils::isValidURI("luna://com.webos.service.x/method"));
    CHECK(Utils::isValidURI("palm://com.palm.bus/signal"));
    CHECK(!Utils::isValidURI("com.webos.service.x/method"));
    CHECK(!Utils::isValidURI(""));
    // Degenerate but accepted: the check is for the separator, and callers
    // that split on it handle an empty method.
    CHECK(Utils::isValidURI("://"));
}

TEST(Utils, isEscapeCharCoversTheWhitespaceWeStrip)
{
    CHECK(Utils::isEscapeChar('\n'));
    CHECK(Utils::isEscapeChar('\t'));
    CHECK(Utils::isEscapeChar('\r'));
    CHECK(Utils::isEscapeChar('\v'));
    CHECK(Utils::isEscapeChar('\f'));
    CHECK(!Utils::isEscapeChar(' '));
    CHECK(!Utils::isEscapeChar('a'));
}

TEST(Utils, createTimestampLooksLikeMilliseconds)
{
    std::string ts;
    Utils::createTimestamp(ts);

    CHECK(ts.size() >= 13);
    for (size_t i = 0; i < ts.size(); ++i)
        CHECK(ts[i] >= '0' && ts[i] <= '9');
}

TEST(Utils, verifyFileExist)
{
    CHECK(!Utils::verifyFileExist(NULL));
    CHECK(!Utils::verifyFileExist("/nonexistent/notificationmgr/nope.png"));
    CHECK(Utils::verifyFileExist("/tmp"));
}
