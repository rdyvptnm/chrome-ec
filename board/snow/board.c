/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Snow board-specific configuration */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "extpower.h"
#include "gaia_power.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "keyboard_raw.h"
#include "pmu_tpschrome.h"
#include "power_led.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define GPIO_KB_INPUT  (GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_BOTH)
#define GPIO_KB_OUTPUT (GPIO_OUTPUT | GPIO_OPEN_DRAIN)

#define INT_BOTH_FLOATING	(GPIO_INPUT | GPIO_INT_BOTH)
#define INT_BOTH_PULL_UP	(GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_BOTH)

#define HARD_RESET_TIMEOUT_MS 5

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[GPIO_COUNT] = {
	/* Inputs with interrupt handlers are first for efficiency */
	{"KB_PWR_ON_L", GPIO_B, (1<<5),  GPIO_INT_BOTH, gaia_power_event},
	{"PP1800_LDO2", GPIO_A, (1<<1),  GPIO_INT_BOTH, gaia_power_event},
	{"XPSHOLD",     GPIO_A, (1<<3),  GPIO_INT_BOTH, gaia_power_event},
	{"CHARGER_INT", GPIO_C, (1<<4),  GPIO_INT_FALLING, pmu_irq_handler},
	{"LID_OPEN",    GPIO_C, (1<<13), GPIO_INT_RISING, gaia_lid_event},
	{"SUSPEND_L",   GPIO_A, (1<<7),  INT_BOTH_FLOATING, gaia_suspend_event},
	{"WP_L",        GPIO_B, (1<<4),  GPIO_INPUT, NULL},
	{"KB_IN00",     GPIO_C, (1<<8),  GPIO_KB_INPUT,
	 keyboard_raw_gpio_interrupt},
	{"KB_IN01",     GPIO_C, (1<<9),  GPIO_KB_INPUT,
	 keyboard_raw_gpio_interrupt},
	{"KB_IN02",     GPIO_C, (1<<10), GPIO_KB_INPUT,
	 keyboard_raw_gpio_interrupt},
	{"KB_IN03",     GPIO_C, (1<<11), GPIO_KB_INPUT,
	 keyboard_raw_gpio_interrupt},
	{"KB_IN04",     GPIO_C, (1<<12), GPIO_KB_INPUT,
	 keyboard_raw_gpio_interrupt},
	{"KB_IN05",     GPIO_C, (1<<14), GPIO_KB_INPUT,
	 keyboard_raw_gpio_interrupt},
	{"KB_IN06",     GPIO_C, (1<<15), GPIO_KB_INPUT,
	 keyboard_raw_gpio_interrupt},
	{"KB_IN07",     GPIO_D, (1<<2),  GPIO_KB_INPUT,
	 keyboard_raw_gpio_interrupt},
	/* Other inputs */
	{"AC_PWRBTN_L", GPIO_A, (1<<0), GPIO_INT_BOTH, NULL},
	{"SPI1_NSS",    GPIO_A, (1<<4), GPIO_DEFAULT, spi_event},
	/*
	 * I2C pins should be configured as inputs until I2C module is
	 * initialized. This will avoid driving the lines unintentionally.
	 */
	{"I2C1_SCL",    GPIO_B, (1<<6),  GPIO_INPUT, NULL},
	{"I2C1_SDA",    GPIO_B, (1<<7),  GPIO_INPUT, NULL},
	{"I2C2_SCL",    GPIO_B, (1<<10), GPIO_INPUT, NULL},
	{"I2C2_SDA",    GPIO_B, (1<<11), GPIO_INPUT, NULL},
	/* Outputs */
	{"AC_STATUS",   GPIO_A, (1<<5), GPIO_DEFAULT, NULL},
	{"SPI1_MISO",   GPIO_A, (1<<6), GPIO_DEFAULT, NULL},
	{"EN_PP1350",   GPIO_A, (1<<2),  GPIO_OUT_LOW, NULL},
	{"EN_PP5000",   GPIO_A, (1<<11),  GPIO_OUT_LOW, NULL},
	{"EN_PP3300",   GPIO_A, (1<<8),  GPIO_OUT_LOW, NULL},
	{"PMIC_PWRON_L",GPIO_A, (1<<12), GPIO_OUT_HIGH, NULL},
	{"PMIC_RESET",  GPIO_A, (1<<15), GPIO_OUT_LOW, NULL},
	{"ENTERING_RW", GPIO_D, (1<<0),  GPIO_OUT_LOW, NULL},
	{"CHARGER_EN",  GPIO_B, (1<<2),  GPIO_OUT_LOW, NULL},
	{"EC_INT",      GPIO_B, (1<<9),  GPIO_HI_Z, NULL},
	{"CODEC_INT",   GPIO_D, (1<<1),  GPIO_HI_Z, NULL},
	{"LED_POWER_L", GPIO_B, (1<<3),  GPIO_INPUT, NULL},
	{"KB_OUT00",    GPIO_B, (1<<0),  GPIO_KB_OUTPUT, NULL},
	{"KB_OUT01",    GPIO_B, (1<<8),  GPIO_KB_OUTPUT, NULL},
	{"KB_OUT02",    GPIO_B, (1<<12), GPIO_KB_OUTPUT, NULL},
	{"KB_OUT03",    GPIO_B, (1<<13), GPIO_KB_OUTPUT, NULL},
	{"KB_OUT04",    GPIO_B, (1<<14), GPIO_KB_OUTPUT, NULL},
	{"KB_OUT05",    GPIO_B, (1<<15), GPIO_KB_OUTPUT, NULL},
	{"KB_OUT06",    GPIO_C, (1<<0),  GPIO_KB_OUTPUT, NULL},
	{"KB_OUT07",    GPIO_C, (1<<1),  GPIO_KB_OUTPUT, NULL},
	{"KB_OUT08",    GPIO_C, (1<<2),  GPIO_KB_OUTPUT, NULL},
	{"KB_OUT09",    GPIO_B, (1<<1),  GPIO_KB_OUTPUT, NULL},
	{"KB_OUT10",    GPIO_C, (1<<5),  GPIO_KB_OUTPUT, NULL},
	{"KB_OUT11",    GPIO_C, (1<<6),  GPIO_KB_OUTPUT, NULL},
	{"KB_OUT12",    GPIO_C, (1<<7),  GPIO_KB_OUTPUT, NULL},
};

void configure_board(void)
{
	uint32_t val;

	dma_init();

	/* Enable all GPIOs clocks
	 * TODO: more fine-grained enabling for power saving
	 */
	STM32_RCC_APB2ENR |= 0x1fd;

#ifdef CONFIG_SPI
	/* Enable SPI */
	STM32_RCC_APB2ENR |= (1<<12);

	/* SPI1 on pins PA4-7 (alt. function push-pull, 10MHz) */
	val = STM32_GPIO_CRL_OFF(GPIO_A) & ~0xffff0000;
	val |= 0x99990000;
	STM32_GPIO_CRL_OFF(GPIO_A) = val;
#endif

	/* remap OSC_IN/OSC_OUT to PD0/PD1 */
	STM32_GPIO_AFIO_MAPR |= 1 << 15;

	/* use PB3 as a GPIO, so disable JTAG and keep only SWD */
	STM32_GPIO_AFIO_MAPR = (STM32_GPIO_AFIO_MAPR & ~(0x7 << 24))
			       | (2 << 24);

	/* remap TIM2_CH2 to PB3 */
	STM32_GPIO_AFIO_MAPR = (STM32_GPIO_AFIO_MAPR & ~(0x3 << 8))
			       | (1 << 8);

	/*
	 * Set alternate function for USART1. For alt. function input
	 * the port is configured in either floating or pull-up/down
	 * input mode (ref. section 7.1.4 in datasheet RM0041):
	 * PA9:  Tx, alt. function output
	 * PA10: Rx, input with pull-down
	 *
	 * note: see crosbug.com/p/12223 for more info
	 */
	val = STM32_GPIO_CRH_OFF(GPIO_A) & ~0x00000ff0;
	val |= 0x00000890;
	STM32_GPIO_CRH_OFF(GPIO_A) = val;

	/* EC_INT is output, open-drain */
	val = STM32_GPIO_CRH_OFF(GPIO_B) & ~0xf0;
	val |= 0x50;
	STM32_GPIO_CRH_OFF(GPIO_B) = val;
	/* put GPIO in Hi-Z state */
	gpio_set_level(GPIO_EC_INT, 1);
}

/* GPIO configuration to be done after I2C module init */
void board_i2c_post_init(int port)
{
	uint32_t val;

	/* enable alt. function (open-drain) */
	if (port == STM32_I2C1_PORT) {
		/* I2C1 is on PB6-7 */
		val = STM32_GPIO_CRL_OFF(GPIO_B) & ~0xff000000;
		val |= 0xdd000000;
		STM32_GPIO_CRL_OFF(GPIO_B) = val;
	} else if (port == STM32_I2C2_PORT) {
		/* I2C2 is on PB10-11 */
		val = STM32_GPIO_CRH_OFF(GPIO_B) & ~0x0000ff00;
		val |= 0x0000dd00;
		STM32_GPIO_CRH_OFF(GPIO_B) = val;
	}
}

void configure_board_late(void)
{
#ifdef CONFIG_AC_POWER_STATUS
	gpio_set_flags(GPIO_AC_STATUS, GPIO_OUT_HIGH);
#endif
#ifdef CONFIG_SPI
	gpio_set_flags(GPIO_SPI1_NSS, GPIO_INT_BOTH);
#endif
}

void keyboard_suppress_noise(void)
{
	/* notify audio codec of keypress for noise suppression */
	gpio_set_level(GPIO_CODEC_INT, 0);
	gpio_set_level(GPIO_CODEC_INT, 1);
}

void board_power_led_config(enum powerled_config config)
{
	uint32_t val;

	switch (config) {
	case POWERLED_CONFIG_PWM:
		val = STM32_GPIO_CRL_OFF(GPIO_B) & ~0x0000f000;
		val |= 0x00009000;	/* alt. function (TIM2/PWM) */
		STM32_GPIO_CRL_OFF(GPIO_B) = val;
		break;
	case POWERLED_CONFIG_MANUAL_OFF:
		/*
		 * Re-configure GPIO as a floating input. Alternatively we could
		 * configure it as an open-drain output and set it to high
		 * impedence, but reconfiguring as an input had better results
		 * in testing.
		 */
		gpio_set_flags(GPIO_LED_POWER_L, GPIO_INPUT);
		gpio_set_level(GPIO_LED_POWER_L, 1);
		break;
	case POWERLED_CONFIG_MANUAL_ON:
		gpio_set_flags(GPIO_LED_POWER_L, GPIO_OUTPUT | GPIO_OPEN_DRAIN);
		gpio_set_level(GPIO_LED_POWER_L, 0);
		break;
	default:
		break;
	}
}

static void board_startup_hook(void)
{
	gpio_set_flags(GPIO_SUSPEND_L, INT_BOTH_PULL_UP);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_startup_hook, HOOK_PRIO_DEFAULT);

static void board_shutdown_hook(void)
{
	/* Disable pull-up on SUSPEND_L during shutdown to prevent leakage */
	gpio_set_flags(GPIO_SUSPEND_L, INT_BOTH_FLOATING);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_shutdown_hook, HOOK_PRIO_DEFAULT);

/*
 * Force the pmic to reset completely.  This forces an entire system reset,
 * and therefore should never return
 */
void board_hard_reset(void)
{
	/* Force a hard reset of tps Chrome */
	gpio_set_level(GPIO_PMIC_RESET, 1);

	/* Delay while the power is cut */
	udelay(HARD_RESET_TIMEOUT_MS * 1000);

	/* Shouldn't get here unless the board doesn't have this capability */
	panic_puts("Hard reset failed! (this board may not be capable)\n");
}

#ifdef CONFIG_PMU_BOARD_INIT
int pmu_board_init(void)
{
	int ver, failure = 0;

	/* Set fast charging timeout to 6 hours*/
	if (!failure)
		failure = pmu_set_fastcharge(TIMEOUT_6HRS);
	/* Enable external gpio CHARGER_EN control */
	if (!failure)
		failure = pmu_enable_ext_control(1);
	/* Disable force charging */
	if (!failure)
		failure = pmu_enable_charger(0);

	/* Set NOITERM bit */
	if (!failure)
		failure = pmu_low_current_charging(1);

	/*
	 * High temperature charging
	 *   termination voltage: 2.1V
	 *   termination current: 100%
	 */
	if (!failure)
		failure = pmu_set_term_voltage(RANGE_T34, TERM_V2100);
	if (!failure)
		failure = pmu_set_term_current(RANGE_T34, TERM_I1000);
	/*
	 * Standard temperature charging
	 *   termination voltage: 2.1V
	 *   termination current: 100%
	 */
	if (!failure)
		failure = pmu_set_term_voltage(RANGE_T23, TERM_V2100);
	if (!failure)
		failure = pmu_set_term_current(RANGE_T23, TERM_I1000);

	/*
	 * Ignore TPSCHROME NTC reading in T40. This is snow board specific
	 * setting. Check:
	 *   http://crosbug.com/p/12221
	 *   http://crosbug.com/p/13171
	 */
	if (!failure)
		failure = pmu_set_term_voltage(RANGE_T40, TERM_V2100);
	if (!failure)
		failure = pmu_set_term_current(RANGE_T40, TERM_I1000);

	/* Workaround init values before ES3 */
	if (pmu_version(&ver) || ver < 3) {
		/* Termination current: 75% */
		if (!failure)
			failure = pmu_set_term_current(RANGE_T34, TERM_I0750);
		if (!failure)
			failure = pmu_set_term_current(RANGE_T23, TERM_I0750);
		if (!failure)
			failure = pmu_set_term_current(RANGE_T40, TERM_I0750);
	}

	return failure ? EC_ERROR_UNKNOWN : EC_SUCCESS;
}
#endif /* CONFIG_BOARD_PMU_INIT */

int extpower_is_present(void)
{
	/*
	 * Detect AC state using combined gpio pins
	 *
	 * On daisy and snow, there's no single gpio signal to detect AC.
	 *   GPIO_AC_PWRBTN_L provides AC on and PWRBTN release.
	 *   GPIO_KB_PWR_ON_L provides PWRBTN release.
	 *
	 * When AC plugged, both GPIOs will be high.
	 *
	 * One drawback of this detection is, when press-and-hold power
	 * button. AC state will be unknown. This function will fallback
	 * to PMU VACG.
	 */

	int ac_good = 1, battery_good;

	if (gpio_get_level(GPIO_KB_PWR_ON_L))
		return gpio_get_level(GPIO_AC_PWRBTN_L);

	/* Check PMU VACG */
	if (!in_interrupt_context())
		pmu_get_power_source(&ac_good, &battery_good);

	/*
	 * Charging task only interacts with AP in discharging state. So
	 * return 1 when AC status can not be detected by GPIO or VACG.
	 */
	return ac_good;
}
