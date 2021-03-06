/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Grunt board-specific configuration */

#include "adc.h"
#include "adc_chip.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charge_state_v2.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "driver/accel_kionix.h"
#include "driver/accel_kx022.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/bc12/bq24392.h"
#include "driver/led/lm3630a.h"
#include "driver/ppc/sn5s330.h"
#include "driver/tcpm/anx74xx.h"
#include "driver/tcpm/ps8xxx.h"
#include "driver/temp_sensor/sb_tsi.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "motion_sense.h"
#include "power.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "tcpci.h"
#include "temp_sensor.h"
#include "thermistor.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

/*
 * These GPIOs change pins depending on board version. They are configured
 * in board_init.
 */
static enum gpio_signal gpio_usb_c1_oc_l = GPIO_USB_C1_OC_L_V2;
static enum gpio_signal gpio_usb_c0_pd_rst_l = GPIO_USB_C0_PD_RST_L_V2;

static void tcpc_alert_event(enum gpio_signal signal)
{
	if ((signal == GPIO_USB_C0_PD_INT_ODL) &&
			!gpio_get_level(gpio_usb_c0_pd_rst_l))
		return;

	if ((signal == GPIO_USB_C1_PD_INT_ODL) &&
			!gpio_get_level(GPIO_USB_C1_PD_RST_L))
		return;

#ifdef HAS_TASK_PDCMD
	/* Exchange status with TCPCs */
	host_command_pd_send_status(PD_CHARGE_NO_CHANGE);
#endif
}

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
static void anx74xx_cable_det_handler(void)
{
	int cable_det = gpio_get_level(GPIO_USB_C0_CABLE_DET);
	int reset_n = gpio_get_level(gpio_usb_c0_pd_rst_l);

	/*
	 * A cable_det low->high transition was detected. If following the
	 * debounce time, cable_det is high, and reset_n is low, then ANX3429 is
	 * currently in standby mode and needs to be woken up. Set the
	 * TCPC_RESET event which will bring the ANX3429 out of standby
	 * mode. Setting this event is gated on reset_n being low because the
	 * ANX3429 will always set cable_det when transitioning to normal mode
	 * and if in normal mode, then there is no need to trigger a tcpc reset.
	 */
	if (cable_det && !reset_n)
		task_set_event(TASK_ID_PD_C0, PD_EVENT_TCPC_RESET, 0);
}
DECLARE_DEFERRED(anx74xx_cable_det_handler);

void anx74xx_cable_det_interrupt(enum gpio_signal signal)
{
	/* debounce for 2 msec */
	hook_call_deferred(&anx74xx_cable_det_handler_data, (2 * MSEC));
}
#endif

static void ppc_interrupt(enum gpio_signal signal)
{
	int port = (signal == GPIO_USB_C0_SWCTL_INT_ODL) ? 0 : 1;

	sn5s330_interrupt(port);
}

#include "gpio_list.h"

/* I2C port map. */
const struct i2c_port_t i2c_ports[] = {
	{"power",   I2C_PORT_POWER,   100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"tcpc0",   I2C_PORT_TCPC0,   400, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"tcpc1",   I2C_PORT_TCPC1,   400, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"thermal", I2C_PORT_THERMAL, 400, GPIO_I2C3_SCL, GPIO_I2C3_SDA},
	{"kblight", I2C_PORT_KBLIGHT, 100, GPIO_I2C5_SCL, GPIO_I2C5_SDA},
	{"sensor",  I2C_PORT_SENSOR,  400, GPIO_I2C7_SCL, GPIO_I2C7_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_KBLIGHT] = {
		.channel = 5,
		.flags = 0,
		.freq = 100,
	},
	[PWM_CH_LED1_AMBER] = {
		.channel = 0,
		.flags = (PWM_CONFIG_OPEN_DRAIN | PWM_CONFIG_ACTIVE_LOW
			  | PWM_CONFIG_DSLEEP),
		.freq = 100,
	},
	[PWM_CH_LED2_BLUE] =   {
		.channel = 2,
		.flags = (PWM_CONFIG_OPEN_DRAIN | PWM_CONFIG_ACTIVE_LOW
			  | PWM_CONFIG_DSLEEP),
		.freq = 100,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

static void board_init(void)
{
	if (system_get_board_version() < 2) {
		/*
		 * These GPIOs change pins depending on board version. Change
		 * them here from the V2 pin to the V0 pin.
		 */
		gpio_usb_c1_oc_l = GPIO_USB_C1_OC_L_V0;
		gpio_usb_c0_pd_rst_l = GPIO_USB_C0_PD_RST_L_V0;
	} else {
		/* Alternate functions for board version 2 only. */
		gpio_set_alternate_function(GPIO_F, 0x02, 1); /* ADC8 */
		gpio_set_alternate_function(GPIO_0, 0x10, 0); /* KSO_13 */
		gpio_set_alternate_function(GPIO_8, 0x04, 0); /* KSO_14 */
	}

	/* Now that we know which pin to use, set the correct output mode. */
	gpio_set_flags(gpio_usb_c1_oc_l, GPIO_OUT_HIGH);
	gpio_set_flags(gpio_usb_c0_pd_rst_l, GPIO_OUT_HIGH);

	/* Enable Gyro interrupts */
	gpio_enable_interrupt(GPIO_6AXIS_INT_L);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void board_overcurrent_event(int port)
{
	enum gpio_signal signal = (port == 0) ? GPIO_USB_C0_OC_L
					      : gpio_usb_c1_oc_l;

	gpio_set_level(signal, 0);

	CPRINTS("p%d: overcurrent!", port);
}

void board_tcpc_init(void)
{
	int count = 0;
	int port;

	/* Wait for disconnected battery to wake up */
	while (battery_hw_present() == BP_YES &&
	       battery_is_present() == BP_NO) {
		usleep(100 * MSEC);
		/* Give up waiting after 1 second */
		if (++count > 10)
			break;
	}

	/* Only reset TCPC if not sysjump */
	if (!system_jumped_to_this_image())
		board_reset_pd_mcu();

	/* Enable PPC interrupts. */
	gpio_enable_interrupt(GPIO_USB_C0_SWCTL_INT_ODL);
	if (system_get_board_version() < 2)
		gpio_enable_interrupt(GPIO_USB_C1_SWCTL_INT_ODL_V0);
	else
		gpio_enable_interrupt(GPIO_USB_C1_SWCTL_INT_ODL_V2);

	/* Enable TCPC interrupts. */
	gpio_enable_interrupt(GPIO_USB_C0_PD_INT_ODL);
	gpio_enable_interrupt(GPIO_USB_C1_PD_INT_ODL);

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	/* Enable CABLE_DET interrupt for ANX3429 wake from standby */
	gpio_enable_interrupt(GPIO_USB_C0_CABLE_DET);
#endif
	/*
	 * Initialize HPD to low; after sysjump SOC needs to see
	 * HPD pulse to enable video path
	 */
	for (port = 0; port < CONFIG_USB_PD_PORT_COUNT; port++) {
		const struct usb_mux *mux = &usb_muxes[port];

		mux->hpd_update(port, 0, 0);
	}
}
DECLARE_HOOK(HOOK_INIT, board_tcpc_init, HOOK_PRIO_INIT_I2C + 1);

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_USB_C0_PD_INT_ODL)) {
		if (gpio_get_level(gpio_usb_c0_pd_rst_l))
			status |= PD_STATUS_TCPC_ALERT_0;
	}

	if (!gpio_get_level(GPIO_USB_C1_PD_INT_ODL)) {
		if (gpio_get_level(GPIO_USB_C1_PD_RST_L))
			status |= PD_STATUS_TCPC_ALERT_1;
	}

	return status;
}

/**
 * Power on (or off) a single TCPC.
 * minimum on/off delays are included.
 *
 * @param port	Port number of TCPC.
 * @param mode	0: power off, 1: power on.
 */
void board_set_tcpc_power_mode(int port, int mode)
{
	if (port != USB_PD_PORT_ANX74XX)
		return;

	switch (mode) {
	case ANX74XX_NORMAL_MODE:
		gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 1);
		msleep(ANX74XX_PWR_H_RST_H_DELAY_MS);
		gpio_set_level(gpio_usb_c0_pd_rst_l, 1);
		break;
	case ANX74XX_STANDBY_MODE:
		gpio_set_level(gpio_usb_c0_pd_rst_l, 0);
		msleep(ANX74XX_RST_L_PWR_L_DELAY_MS);
		gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 0);
		msleep(ANX74XX_PWR_L_PWR_H_DELAY_MS);
		break;
	default:
		break;
	}
}

void board_reset_pd_mcu(void)
{
	/* Assert reset to TCPC1 (ps8751) */
	gpio_set_level(GPIO_USB_C1_PD_RST_L, 0);

	/* Assert reset to TCPC0 (anx3429) */
	gpio_set_level(gpio_usb_c0_pd_rst_l, 0);

	/* TCPC1 (ps8751) requires 1ms reset down assertion */
	msleep(MAX(1, ANX74XX_RST_L_PWR_L_DELAY_MS));

	/* Deassert reset to TCPC1 */
	gpio_set_level(GPIO_USB_C1_PD_RST_L, 1);
	/* Disable TCPC0 power */
	gpio_set_level(GPIO_EN_USB_C0_TCPC_PWR, 0);

	/*
	 * anx3429 requires 10ms reset/power down assertion
	 */
	msleep(ANX74XX_PWR_L_PWR_H_DELAY_MS);
	board_set_tcpc_power_mode(USB_PD_PORT_ANX74XX, 1);
}

static void board_kblight_init(void)
{
	/*
	 * Enable keyboard backlight. This needs to be done here because
	 * the chip doesn't have power until PP3300_S0 comes up.
	 */
	gpio_set_level(GPIO_KB_BL_EN, 1);
	lm3630a_poweron();
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_kblight_init, HOOK_PRIO_DEFAULT);
