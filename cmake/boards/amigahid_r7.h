/* AmigaHID r7: RP2350A, 4 MiB Winbond flash and RM2 wireless.
 * SPDX-License-Identifier: EPL-2.0
 * This header is also read by the assembler and Pico SDK CMake parser. */
#ifndef AMIGAHID_R7
#define AMIGAHID_R7 1

/* RM2 uses the same CYW43 interface pins as Pico 2 W: 23/24/25/29.
 * UART0 is on GPIO0/1; the OLED uses I2C1 on GPIO2/3. */
#define PICO_DEFAULT_I2C 1
#define PICO_DEFAULT_I2C_SDA_PIN 2
#define PICO_DEFAULT_I2C_SCL_PIN 3
#include "boards/pico2_w.h"

/* These Pico 2 W board functions are not wired on r7. */
#undef PICO_VSYS_PIN
#undef CYW43_USES_VSYS_PIN
#undef CYW43_WL_GPIO_SMPS_PIN
#undef CYW43_WL_GPIO_VBUS_PIN
#undef PICO_DEFAULT_SPI
#undef PICO_DEFAULT_SPI_SCK_PIN
#undef PICO_DEFAULT_SPI_TX_PIN
#undef PICO_DEFAULT_SPI_RX_PIN
#undef PICO_DEFAULT_SPI_CSN_PIN

/* Match the SDK's A2-compatible pairing offset on every r7 build. */
#define PICO_FLASH_BANK_STORAGE_OFFSET (PICO_FLASH_SIZE_BYTES - 3u * 4096u)
#endif
