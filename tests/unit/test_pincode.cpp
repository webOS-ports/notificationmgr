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

#include <PincodeValidator.h>

TEST(Pincode, acceptsTheCodeItWasGiven)
{
    PincodeValidator v("1234");
    CHECK(v.check("1234"));
}

TEST(Pincode, refusesEverythingElse)
{
    PincodeValidator v("1234");
    CHECK(!v.check("1235"));
    CHECK(!v.check("123"));
    CHECK(!v.check("12345"));
    CHECK(!v.check("0000"));
    CHECK(!v.check(""));
}

//! An empty input is refused before the stored code is even consulted.
TEST(Pincode, refusesAnEmptyInput)
{
    PincodeValidator v("");
    CHECK(!v.check(""));
}
