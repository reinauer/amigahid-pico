/* CD32 controller output. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_CD32_H
#define AMIGAHID_CD32_H
#include <stdbool.h>
#include <stdint.h>
// All calls belong to the controller-output core.
void cd32_init(void);
void cd32_enable(unsigned port, bool enabled);
void cd32_update(unsigned port, uint16_t state);
#endif
