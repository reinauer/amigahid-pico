#!/bin/sh
# SPDX-License-Identifier: EPL-2.0
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d "${TMPDIR:-/tmp}/amigahid-firmware.XXXXXX")
trap 'rm -rf "$out"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -DCFG_TUSB_MCU=0 -DBOARD_HIDPICO_REV5 \
    -Itests/keyboard-stubs -Isrc -Ilib/tinyusb/src \
    tests/keyboard_startup_test.c -o "$out/startup"
"$out/startup"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Isrc \
    tests/hid_gamepad_test.c src/hid_gamepad.c src/gamepad_config.c \
    -o "$out/gamepad"
"$out/gamepad"
for bluetooth in '' '-DENABLE_BLUETOOTH_HID=1'; do
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -DCFG_TUSB_MCU=0 -DBOARD_HIDPICO_REV5 $bluetooth \
        -Itests/main-stubs -Isrc -Ilib/tinyusb/src \
        tests/main_startup_test.c -o "$out/main"
    "$out/main"
done
