/* Two settings sectors followed by two BTstack pairing sectors. */
ASSERT(__flash_binary_end <= ORIGIN(FLASH) + LENGTH(FLASH) - 16384,
       "Firmware overlaps reserved settings/Bluetooth flash storage")
