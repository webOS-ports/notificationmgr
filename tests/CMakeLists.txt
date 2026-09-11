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
# Unit tests for the parts of notificationmgr that can be exercised without a
# bus. They are cross-compiled with the component and meant to be run on the
# target - the LuneOS sysroot is where luna-service2, pbnjson and PmLogLib
# live, and a phone is where the behaviour has to hold.
#
# Off by default so ordinary image builds are unaffected:
#   cmake -DBUILD_TESTING=ON ...

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp)

# The component's own sources, minus the daemon entry point.
set(COMPONENT_SOURCES ${SOURCES})
list(REMOVE_ITEM COMPONENT_SOURCES ${PROJECT_SOURCE_DIR}/src/main.cpp)

add_executable(notificationmgr-tests ${TEST_SOURCES} ${COMPONENT_SOURCES})

target_include_directories(notificationmgr-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/Configured/src
)

target_link_libraries(notificationmgr-tests
    ${GLIB2_LDFLAGS}
    ${GLIBMM_LDFLAGS}
    ${LUNASERVICE_LDFLAGS}
    ${PBNJSON_CPP_LDFLAGS}
    ${PMLOG_LDFLAGS}
    ${LIBXMLXX_LDFLAGS}
    ${OPENSSL_LDFLAGS}
)

add_test(NAME notificationmgr-unit COMMAND notificationmgr-tests)
