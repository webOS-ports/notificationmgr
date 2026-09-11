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

#include <Settings.h>

/*
 * Every privilege decision in this service used to be find() != npos, which
 * answers "does this string appear anywhere" rather than "is this id in this
 * namespace". These cases are the difference between the two.
 */

TEST(Prefix, acceptsIdsInsideTheNamespace)
{
    CHECK(Settings::idHasPrefix("com.webos.app.foo", "com.webos."));
    CHECK(Settings::idHasPrefix("com.webos.service.bar", "com.webos."));
    CHECK(Settings::idHasPrefix("com.palm.app.x", "com.palm."));
    CHECK(Settings::idHasPrefix("com.lge.service.y", "com.lge."));
    CHECK(Settings::idHasPrefix("org.webosports.app.z", "org.webosports."));
}

TEST(Prefix, rejectsIdsThatMerelyMentionIt)
{
    CHECK(!Settings::idHasPrefix("org.example.com.webos.app.foo", "com.webos."));
    CHECK(!Settings::idHasPrefix("evil-com.webos.app.foo", "com.webos."));
    CHECK(!Settings::idHasPrefix("xcom.webos.app.foo", "com.webos."));
    CHECK(!Settings::idHasPrefix("notcom.palm.x", "com.palm."));
}

TEST(Prefix, stopsAtANameBoundaryForDotlessPrefixes)
{
    // The whole id.
    CHECK(Settings::idHasPrefix("com.webos.surfacemanager", "com.webos.surfacemanager"));
    // The id with an instance suffix, which is how LS2 names app callers.
    CHECK(Settings::idHasPrefix("com.webos.app.notification-11458", "com.webos.app.notification"));
    CHECK(Settings::idHasPrefix("com.webos.app.notification 4242", "com.webos.app.notification"));
    CHECK(Settings::idHasPrefix("com.webos.surfacemanager.sub", "com.webos.surfacemanager"));

    // A longer name that merely starts the same way is a different service.
    CHECK(!Settings::idHasPrefix("com.webos.app.notifications", "com.webos.app.notification"));
    CHECK(!Settings::idHasPrefix("com.webos.surfacemanagerX", "com.webos.surfacemanager"));
}

TEST(Prefix, handlesEmptyAndShortInput)
{
    CHECK(!Settings::idHasPrefix("", "com.webos."));
    CHECK(!Settings::idHasPrefix("com.webos.app.foo", ""));
    CHECK(!Settings::idHasPrefix("", ""));
    CHECK(!Settings::idHasPrefix("com", "com.webos."));
}

/*
 * createToast lets a non-privileged caller name a sourceId only if the caller
 * sits under it. The old test was find(sourceId) != npos, so any inner
 * fragment - "example", "app", "ker" - passed for every caller that contained
 * it. Dotted ancestors ("com", "com.example") still pass by design: a
 * webapp's bus name extends its application id with further dotted segments,
 * so the id has to be creditable to the name that carries it.
 */
TEST(Prefix, aCallerCannotClaimAFragmentOfItself)
{
    CHECK(!Settings::idHasPrefix("com.example.app.attacker", "example"));
    CHECK(!Settings::idHasPrefix("com.example.app.attacker", "app"));
    CHECK(!Settings::idHasPrefix("com.example.app.attacker", "ker"));
    CHECK(!Settings::idHasPrefix("com.example.app.attacker", "attacker"));
    CHECK(!Settings::idHasPrefix("com.example.app.attacker", "exam"));

    CHECK(Settings::idHasPrefix("com.example.app.attacker", "com.example.app.attacker"));
    CHECK(Settings::idHasPrefix("com.example.app.attacker-991", "com.example.app.attacker"));
    CHECK(Settings::idHasPrefix("com.example.app.attacker", "com"));
    CHECK(Settings::idHasPrefix("com.example.app.attacker", "com.example"));
}
