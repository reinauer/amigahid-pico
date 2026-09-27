/**
 * ssd1306 display handling
 *
 * please see disp_ssd.c for a more comprehensive readme.
 */

#ifndef _DISPLAY_DISP_SSD_H
#define _DISPLAY_DISP_SSD_H

#include <stdint.h>
#include <stdbool.h>

void disp_ssd_init(void);
void disp_ssd_task(void);
void disp_ssd_version(char *version);
bool disp_ssd_available(void);
void disp_ssd_set_enabled(bool enabled);
void disp_ssd_set_overlay(bool active);
void disp_ssd_menu(char const *heading, char const *item, char const *value, char const *hint);

extern void (*disp_write)(uint8_t x, uint8_t y, char *message);

#endif // _DISPLAY_DISP_SSD_H
