/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PWM_H
#define __CROS_EC_PWM_H

/* The values are defined in board.h */
enum pwm_channel;

/**
 * Enable/disable a PWM channel.
 */
void pwm_enable(enum pwm_channel ch, int enabled);

/**
 * Get PWM channel enabled status.
 */
int pwm_get_enabled(enum pwm_channel ch);

/**
 * Set PWM channel frequency (Hz).
 * PWM will be disabled until the duty is set.
 */
void pwm_set_freq(enum pwm_channel ch, uint32_t freq);

/**
 * Set PWM channel duty cycle (0-65535).
 */
void pwm_set_raw_duty(enum pwm_channel ch, uint16_t duty);

/**
 * Get PWM channel duty cycle.
 */
uint16_t pwm_get_raw_duty(enum pwm_channel ch);

/**
 * Set PWM channel duty cycle (0-100).
 */
void pwm_set_duty(enum pwm_channel ch, int percent);

/**
 * Get PWM channel duty cycle.
 */
int pwm_get_duty(enum pwm_channel ch);


/* Flags for PWM config table */

/**
 * PWM output signal is inverted, so 100% duty means always low
 */
#define PWM_CONFIG_ACTIVE_LOW		(1 << 0)
/**
 * PWM channel has a fan controller with a tach input and can auto-adjust
 * its duty cycle to produce a given fan RPM.
 */
#define PWM_CONFIG_HAS_RPM_MODE		(1 << 1)
/**
 * PWM clock select alternate source.  The actual clock and alternate
 * source are chip dependent.
 */
#define PWM_CONFIG_ALT_CLOCK		(1 << 2)
/**
 * PWM channel has a complementary output signal which should be enabled in
 * addition to the primary output.
 */
#define PWM_CONFIG_COMPLEMENTARY_OUTPUT	(1 << 3)
/**
 * PWM channel must stay active in low-power idle, if enabled.
 */
#define PWM_CONFIG_DSLEEP		(1 << 4)
/**
 * PWM channel's IO type is open-drain, if enabled. (default IO is push-pull.)
 */
#define PWM_CONFIG_OPEN_DRAIN		(1 << 5)
#endif  /* __CROS_EC_PWM_H */
