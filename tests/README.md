# notificationmgr test and hardening harness

Three layers, smallest first.

## Unit tests (`tests/unit/`)

Cross-compiled with the component; no bus needed. They pin the behaviour the
herrie/fixes audit corrected: `Utils::readFile` keeping the last byte,
`Settings::idHasPrefix` refusing ids that merely mention a namespace,
`History`'s db8 query builders keeping hostile values inert, the displayId
bound, `Singleton::destroy`, `JsonParser::createActionInfo` on unsplittable
uris, and the pincode validator.

Build them with the component:

    cmake -DBUILD_TESTING=ON ...
    ninja notificationmgr-tests

The runner in `check.h` is deliberately tiny - gtest is not in the LuneOS
sysroot and a few dozen assertions do not justify adding it. Run the binary
with no arguments for everything, or with a suite name to filter.

On the build host the corei7-64 build runs against its recipe sysroot:

    <sysroot>/usr/lib/ld-linux-x86-64.so.2 --library-path <sysroot>/usr/lib \
        build/tests/notificationmgr-tests

## Hardening (`tests/hardening/`)

`run-valgrind.sh` runs that same x86-64 test binary under valgrind and fails
on any memory error or definite leak. The image toolchain ships no sanitizer
runtimes, so valgrind on the corei7-64 build is the memory checker that works
everywhere; `-DENABLE_SANITIZERS=address` is wired in CMakeLists.txt for
toolchains that do carry one.

## On the device (`tests/device/`)

`run-on-device.sh` talks to the running service with luna-send: the toast
round trip, the per-app block list end to end (including that the block
actually blocks and survives in the preferences file), closeToast, every
displayId bound, and an injection-shaped sourceId that has to stay a value.

`stress.sh [iterations]` hammers createToast / getToastSettings /
disableToast / enableToast / getToastList / closeToast in a loop with hostile
payloads mixed in, and fails if the daemon crashes, restarts, or its RSS
keeps climbing. The leaks fixed on this branch (a History per getToastList, a
message per alert) are exactly what it would catch coming back.

Copy both scripts and the aarch64 `notificationmgr-tests` binary to the
device and run them there.
