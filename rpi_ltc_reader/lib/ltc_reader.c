/**
 * @file ltc_reader.c
 *
 */
/*
 * Parts of this code is inspired by:
 * http://forum.arduino.cc/index.php/topic,8237.0.html
 *
 * References :
 * https://en.wikipedia.org/wiki/Linear_timecode
 * http://www.philrees.co.uk/articles/timecode.htm
 */
/* Copyright (C) 2016, 2017 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "arm/synchronize.h"

#include "bcm2835.h"
#include "bcm2835_gpio.h"

#include "hardware.h"
#include "console.h"

#include "lcd.h"

#include "midi.h"
#include "midi_send.h"

#include "ltc_reader.h"
#include "ltc_reader_params.h"

#include "util.h"

static volatile char timecode[LCD_MAX_CHARACTERS] ALIGNED;

static const struct _ltc_reader_output *output;

static const char types[5][12] ALIGNED = {"Film 24fps " , "EBU 25fps  " , "DF 29.97fps" , "SMPTE 30fps", "----- -----" };
static timecode_types prev_type = TC_TYPE_INVALID;	///< Invalid type. Force initial update.

static volatile uint32_t irq_us_previous = 0;
static volatile uint32_t irq_us_current = 0;

static volatile uint32_t bit_time = 0;
static volatile uint32_t total_bits = 0;
static volatile bool ones_bit_count = false;
static volatile uint32_t current_bit = 0;
static volatile uint32_t sync_count = 0;
static volatile bool timecode_sync = false;
static volatile bool timecode_valid = false;

static volatile uint8_t timecode_bits[8] ALIGNED;
static volatile char timecode[LCD_MAX_CHARACTERS] ALIGNED;
static volatile bool is_drop_frame_flag_set = false;

static volatile bool timecode_available = false;

static volatile uint32_t ltc_updates_per_seconde= (uint32_t) 0;
static volatile uint32_t ltc_updates_previous = (uint32_t) 0;
static volatile uint32_t ltc_updates = (uint32_t) 0;

static volatile uint32_t led_counter = (uint32_t) 0;

static volatile struct _midi_send_tc midi_timecode = { 0, 0, 0, 0, MIDI_TC_TYPE_EBU };

/**
 *
 */
void __attribute__((interrupt("FIQ"))) c_fiq_handler(void) {
	dmb();

	BCM2835_ST->CS = BCM2835_ST_CS_M1;
	BCM2835_ST->C1 = BCM2835_ST->CLO + (uint32_t) 1000000;

	dmb();
	ltc_updates_per_seconde = ltc_updates - ltc_updates_previous;
	ltc_updates_previous = ltc_updates;

	if ((ltc_updates_per_seconde >= 24) && (ltc_updates_per_seconde <= 30)) {
		hardware_led_set((int) (led_counter++ & 0x01));
	} else {
		hardware_led_set(0);
	}

	dmb();
}

/**
 *
 */
void __attribute__((interrupt("IRQ"))) c_irq_handler(void) {
	dmb();
	irq_us_current = BCM2835_ST->CLO;

	BCM2835_GPIO->GPEDS0 = 1 << GPIO_PIN;

	dmb();
	bit_time = irq_us_current - irq_us_previous;

	if ((bit_time < ONE_TIME_MIN) || (bit_time > ZERO_TIME_MAX)) {
		total_bits = 0;
	} else {
		if (ones_bit_count) {
			ones_bit_count = false;
		} else {
			if (bit_time > ZERO_TIME_MIN) {
				current_bit = 0;
				sync_count = 0;
			} else {
				current_bit = 1;
				ones_bit_count = true;
				sync_count++;

				if (sync_count == 12) {
					sync_count = 0;
					timecode_sync = true;
					total_bits = END_SYNC_POSITION;
				}
			}

			if (total_bits <= END_DATA_POSITION) {
				timecode_bits[0] = timecode_bits[0] >> 1;
				int n;
				for (n = 1; n < 8; n++) {
					if (timecode_bits[n] & 1) {
						timecode_bits[n - 1] |= (uint8_t) 0x80;
					}
					timecode_bits[n] = timecode_bits[n] >> 1;
				}

				if (current_bit == 1) {
					timecode_bits[7] |= (uint8_t) 0x80;
				}
			}

			total_bits++;
		}

		if (total_bits == END_SMPTE_POSITION) {

			total_bits = 0;

			if (timecode_sync) {
				timecode_sync = false;
				timecode_valid = true;
			}
		}

		if (timecode_valid) {
			dmb();
			ltc_updates++;

			timecode_valid = false;

			midi_timecode.frame  = (10 * (timecode_bits[1] & 0x03)) + (timecode_bits[0] & 0x0F);
			midi_timecode.second = (10 * (timecode_bits[3] & 0x07)) + (timecode_bits[2] & 0x0F);
			midi_timecode.minute = (10 * (timecode_bits[5] & 0x07)) + (timecode_bits[4] & 0x0F);
			midi_timecode.hour   = (10 * (timecode_bits[7] & 0x03)) + (timecode_bits[6] & 0x0F);

			timecode[10] = (timecode_bits[0] & 0x0F) + '0';	// frames
			timecode[9]  = (timecode_bits[1] & 0x03) + '0';	// 10's of frames
			timecode[7]  = (timecode_bits[2] & 0x0F) + '0';	// seconds
			timecode[6]  = (timecode_bits[3] & 0x07) + '0';	// 10's of seconds
			timecode[4]  = (timecode_bits[4] & 0x0F) + '0';	// minutes
			timecode[3]  = (timecode_bits[5] & 0x07) + '0';	// 10's of minutes
			timecode[1]  = (timecode_bits[6] & 0x0F) + '0';	// hours
			timecode[0]  = (timecode_bits[7] & 0x03) + '0';	// 10's of hours

			is_drop_frame_flag_set = (timecode_bits[1] & (1 << 2));

			dmb();
			timecode_available = true;
		}
	}

	irq_us_previous = irq_us_current;
	dmb();
}

/**
 *
 */
void ltc_reader(void) {
	uint8_t type = TC_TYPE_UNKNOWN;
	uint32_t limit_us = (uint32_t) 0;
	uint32_t now_us = (uint32_t) 0;
	char limit_warning[16] ALIGNED;

	dmb();
	if (timecode_available) {
		dmb();
		timecode_available = false;

		now_us = BCM2835_ST->CLO;

		type = TC_TYPE_UNKNOWN;

		dmb();
		if (is_drop_frame_flag_set) {
			type = TC_TYPE_DF;
			limit_us = (uint32_t)((double)1000000 / (double)30);
		} else {
			if (ltc_updates_per_seconde == 24) {
				type = TC_TYPE_FILM;
				limit_us = (uint32_t)((double)1000000 / (double)24);
			} else if (ltc_updates_per_seconde == 25) {
				type = TC_TYPE_EBU;
				limit_us = (uint32_t)((double)1000000 / (double)25);
			} else if (ltc_updates_per_seconde == 30) {
				limit_us = (uint32_t)((double)1000000 / (double)30);
				type = TC_TYPE_SMPTE;
			}
		}

		if (output->midi_output) {
			midi_timecode.rate = type;
			midi_send_tc((struct _midi_send_tc *)&midi_timecode);
		}

		if (output->console_output) {
			console_set_cursor(2, 5);
			console_write((char *) timecode, 11);
		}

		if (output->lcd_output) {
			lcd_text_line_1((char *) timecode, LCD_MAX_CHARACTERS);
		}

		if (prev_type != type) {
			prev_type = type;

			if (output->console_output) {
				console_set_cursor(2, 6);
				console_puts(types[type]);
			}

			if (output->lcd_output) {
				lcd_text_line_2(types[type], MIN((sizeof(types[0]) / sizeof(char))-1, LCD_MAX_CHARACTERS));
			}
		}


		const uint32_t delta_us = BCM2835_ST->CLO - now_us;
		if (limit_us == 0) {
			sprintf(limit_warning, "-----:%.5d", (int) delta_us);
			console_status(CONSOLE_CYAN, limit_warning);
		} else {
			sprintf(limit_warning, "%.5d:%.5d", (int) limit_us, (int) delta_us);
			console_status(delta_us < limit_us ? CONSOLE_YELLOW : CONSOLE_RED, limit_warning);
		}
	}
}

/**
 *
 */
void ltc_reader_init(const struct _ltc_reader_output *out) {
	assert(out != NULL);

	output = out;

	bcm2835_gpio_fsel(GPIO_PIN, BCM2835_GPIO_FSEL_INPT);
	// Rising Edge
	BCM2835_GPIO->GPREN0 = 1 << GPIO_PIN;
	// Falling Edge
	BCM2835_GPIO->GPFEN0 = 1 << GPIO_PIN;
	// Clear status bit
	BCM2835_GPIO->GPEDS0 = 1 << GPIO_PIN;
	// Enable GPIO IRQ
	BCM2835_IRQ->IRQ_ENABLE2 = BCM2835_GPIO0_IRQn;

	dmb();
	__enable_irq();

	BCM2835_ST->CS = BCM2835_ST_CS_M1;
	BCM2835_ST->C1 = BCM2835_ST->CLO + (uint32_t) 1000000;
	BCM2835_IRQ->FIQ_CONTROL = BCM2835_FIQ_ENABLE | INTERRUPT_TIMER1;

	dmb();
	__enable_fiq();

	uint8_t i;

	for (i= 0; i < sizeof(timecode) / sizeof(timecode[0]) ; i++) {
		timecode[i] = ' ';
	}

	timecode[2] = ':';
	timecode[5] = ':';
	timecode[8] = '.';

	if (output->lcd_output) {
		lcd_cls();
	}
}