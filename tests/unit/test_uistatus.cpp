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

#include <UiStatus.h>

TEST(UiStatus, startsWithTheUiOff)
{
    UiStatus::instance().enable(UiStatus::ENABLE_SYSTEM | UiStatus::ENABLE_EXTERNAL);
    UiStatus::instance().disable(UiStatus::ENABLE_UI);

    CHECK(UiStatus::instance().toast() != NULL);
    CHECK(UiStatus::instance().alert() != NULL);
    CHECK(UiStatus::instance().input() != NULL);
    CHECK(UiStatus::instance().prompt() != NULL);

    CHECK(!UiStatus::instance().isEnabled(UiStatus::ENABLE_UI));
    CHECK(UiStatus::instance().isEnabled(UiStatus::ENABLE_SYSTEM));
}

TEST(UiStatus, masksCombine)
{
    UiStatus::instance().enable(UiStatus::ENABLE_ALL);
    CHECK(UiStatus::instance().isEnabled(UiStatus::ENABLE_ALL));

    UiStatus::instance().disable(UiStatus::ENABLE_EXTERNAL, "test");
    CHECK(!UiStatus::instance().isEnabled(UiStatus::ENABLE_ALL));
    CHECK(UiStatus::instance().isEnabled(UiStatus::ENABLE_SYSTEM | UiStatus::ENABLE_UI));

    UiStatus::instance().enable(UiStatus::ENABLE_EXTERNAL, "test");
    CHECK(UiStatus::instance().isEnabled(UiStatus::ENABLE_ALL));
}

TEST(UiStatus, silenceIsRemembered)
{
    CHECK(!UiStatus::instance().toast()->isSilence());
    UiStatus::instance().toast()->setSilence(true);
    CHECK(UiStatus::instance().toast()->isSilence());
    UiStatus::instance().toast()->setSilence(false);
    CHECK(!UiStatus::instance().toast()->isSilence());
}

//! The accessors used to do a find() and then an operator[] that would have
//! inserted a null entry; asking many times must not grow or corrupt the map.
TEST(UiStatus, accessorsAreStable)
{
    void *first = UiStatus::instance().toast();
    for (int i = 0; i < 10000; ++i)
        CHECK(UiStatus::instance().toast() == first);
}
