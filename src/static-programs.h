/* vim:ts=4 sts=4 et tw=80
 *
 *         fnordlicht firmware
 *
 *    for additional information please
 *    see http://lochraster.org/fnordlichtmini
 *
 * (c) by Alexander Neumann <alexander@bumpern.de>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __STATIC_PROGRAMS_H
#define __STATIC_PROGRAMS_H

#include <stdint.h>
#include <stdbool.h>

#define STATIC_PROGRAMS_LEN 3

/* configure maximal static program parameter size */
#define PROGRAM_PARAMETER_SIZE 10

/* parameter structures (max 10 bytes) */
struct colorwheel_params_t {
    uint8_t fade_step;
    uint8_t fade_delay;
    uint8_t fade_sleep;
    uint16_t hue_start;
    int16_t hue_step;
    int8_t add_addr;
    uint8_t saturation;
    uint8_t value;
} __attribute__ ((__packed__));

struct random_params_t {
    uint16_t seed;
    uint8_t use_address:1;
    uint8_t wait_for_fade:1;
    uint8_t reserved:6;
    uint8_t fade_step;
    uint8_t fade_delay;
    uint16_t fade_sleep;
    uint8_t saturation;
    uint8_t value;
    uint8_t min_distance;
} __attribute__ ((__packed__));

struct replay_params_t {
    uint8_t start;
    uint8_t end;
    enum {
        REPEAT_NONE = 0,
        REPEAT_START = 1,
        REPEAT_REVERSE = 2,
    } repeat;
};

union program_params_t {
    /* parameters for static programs */
    uint8_t raw[PROGRAM_PARAMETER_SIZE];
    struct colorwheel_params_t colorwheel;
    struct random_params_t random;
    struct replay_params_t replay;
};

#endif
