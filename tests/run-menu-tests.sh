#!/bin/sh
# SPDX-License-Identifier: EPL-2.0
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d "${TMPDIR:-/tmp}/amigahid-menu.XXXXXX")
trap 'rm -rf "$out"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Isrc \
    tests/menu_lever_test.c src/menu_lever.c -o "$out/contacts"
"$out/contacts"
for bluetooth in '' '-DENABLE_BLUETOOTH_HID=1'; do
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/stubs -Isrc -Icommon \
        -DCFG_TUSB_MCU=0 -DBOARD_HIDPICO_REV7 -DAMIGAHID_R7=1 -DPICO_RP2350=1 \
        $bluetooth tests/runtime_menu_lever_test.c src/menu_lever.c -o "$out/menu"
    "$out/menu"
done
