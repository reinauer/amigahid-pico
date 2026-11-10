/* Two settings sectors, two BTstack sectors, and RP2350's E10 sector. */
ASSERT(__flash_binary_end <= ORIGIN(FLASH) + LENGTH(FLASH) - __amigahid_flash_storage_bytes,
       "Firmware overlaps reserved settings/Bluetooth flash storage")
