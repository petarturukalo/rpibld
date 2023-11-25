/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 */
#include "error.h"
#include "led.h"
#include "timer.h"

#define SHORT_PAUSE_MS 370
#define LONG_PAUSE_MS 2250

void signal_error(enum error_code error)
{
	led_init();
	do {
		/* 
		 * The LED is turned on while this bootloader program is being loaded.
		 * Do a long pause before the first signal so it can be differentiated
		 * from that load.
		 */
		sleep(LONG_PAUSE_MS);
		for (int i = error; i--;) {
			led_turn_on();
			sleep(SHORT_PAUSE_MS);
			led_turn_off();
			/* Don't short pause when about to long pause. */
			if (i) {
				sleep(SHORT_PAUSE_MS);
			}
		}
	} while (1);
}
