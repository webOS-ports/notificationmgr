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

#include <Singleton.hpp>

namespace
{

int g_liveAlpha = 0;
int g_liveBeta = 0;

class Alpha : public Singleton<Alpha>
{
public:
    Alpha() : value(1) { ++g_liveAlpha; }
    ~Alpha() override { --g_liveAlpha; }
    int value;
};

class Beta : public Singleton<Beta>
{
public:
    Beta() : value(2) { ++g_liveBeta; }
    ~Beta() override { --g_liveBeta; }
    int value;
};

} // namespace

TEST(Singleton, handsBackTheSameObject)
{
    Alpha &a = Alpha::instance();
    Alpha &b = Alpha::instance();
    CHECK(&a == &b);
    CHECK_EQ(a.value, 1);
}

/*
 * untrack() erased the list entry and then deleted *it, dereferencing an
 * iterator erase() had already invalidated. With two singletons registered the
 * list has real neighbours to free into, which is what turns the read into a
 * crash or a double free rather than a quiet no-op.
 */
TEST(Singleton, destroyReleasesOnlyItsOwnInstance)
{
    Alpha::instance();
    Beta::instance();

    CHECK_EQ(g_liveAlpha, 1);
    CHECK_EQ(g_liveBeta, 1);

    Alpha::destroy();

    CHECK_EQ(g_liveAlpha, 0);
    CHECK_EQ(g_liveBeta, 1);

    // Asking again builds a fresh one rather than handing back the dead one.
    CHECK_EQ(Alpha::instance().value, 1);
    CHECK_EQ(g_liveAlpha, 1);

    Beta::destroy();
    CHECK_EQ(g_liveBeta, 0);
}

TEST(Singleton, destroyIsIdempotent)
{
    Alpha::instance();
    Alpha::destroy();
    Alpha::destroy();
    CHECK_EQ(g_liveAlpha, 0);
}

TEST(Singleton, survivesRepeatedChurn)
{
    for (int i = 0; i < 1000; ++i)
    {
        Alpha::instance();
        Beta::instance();
        Alpha::destroy();
        Beta::destroy();
    }

    CHECK_EQ(g_liveAlpha, 0);
    CHECK_EQ(g_liveBeta, 0);
}
