/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Grunt family-specific configuration */

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

const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_LID_OPEN,
	GPIO_AC_PRESENT,
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used =  ARRAY_SIZE(hibernate_wake_pins);

const struct adc_t adc_channels[] = {
	[ADC_TEMP_SENSOR_CHARGER] = {
		"CHARGER", NPCX_ADC_CH0, ADC_MAX_VOLT, ADC_READ_MAX+1, 0
	},
	[ADC_TEMP_SENSOR_SOC] = {
		"SOC", NPCX_ADC_CH1, ADC_MAX_VOLT, ADC_READ_MAX+1, 0
	},
	[ADC_VBUS] = {
		"VBUS", NPCX_ADC_CH8, ADC_MAX_VOLT*10, ADC_READ_MAX+1, 0
	},
	[ADC_SKU_ID1] = {
		"SKU1", NPCX_ADC_CH9, ADC_MAX_VOLT, ADC_READ_MAX+1, 0
	},
	[ADC_SKU_ID2] = {
		"SKU2", NPCX_ADC_CH4, ADC_MAX_VOLT, ADC_READ_MAX+1, 0
	},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* Power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_PCH_SLP_S3_L,	POWER_SIGNAL_ACTIVE_HIGH, "SLP_S3_DEASSERTED"},
	{GPIO_PCH_SLP_S5_L,	POWER_SIGNAL_ACTIVE_HIGH, "SLP_S5_DEASSERTED"},
	{GPIO_S0_PGOOD,		POWER_SIGNAL_ACTIVE_HIGH, "S0_PGOOD"},
	{GPIO_S5_PGOOD,		POWER_SIGNAL_ACTIVE_HIGH, "S5_PGOOD"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	[USB_PD_PORT_ANX74XX] = {
		.i2c_host_port = I2C_PORT_TCPC0,
		.i2c_slave_addr = ANX74XX_I2C_ADDR1,
		.drv = &anx74xx_tcpm_drv,
		.pol = TCPC_ALERT_ACTIVE_LOW,
	},
	[USB_PD_PORT_PS8751] = {
		.i2c_host_port = I2C_PORT_TCPC1,
		.i2c_slave_addr = PS8751_I2C_ADDR1,
		.drv = &ps8xxx_tcpm_drv,
		.pol = TCPC_ALERT_ACTIVE_LOW,
	},
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	[USB_PD_PORT_ANX74XX] = {
		.port_addr = USB_PD_PORT_ANX74XX,
		.driver = &anx74xx_tcpm_usb_mux_driver,
		.hpd_update = &anx74xx_tcpc_update_hpd_status,
	},
	[USB_PD_PORT_PS8751] = {
		.port_addr = USB_PD_PORT_PS8751,
		.driver = &tcpci_tcpm_usb_mux_driver,
		.hpd_update = &ps8xxx_tcpc_update_hpd_status,
		/* TODO(ecgh): ps8751_tune_mux needed? */
	}
};

struct ppc_config_t ppc_chips[] = {
	{
		.i2c_port = I2C_PORT_TCPC0,
		.i2c_addr = SN5S330_ADDR0,
		.drv = &sn5s330_drv
	},
	{
		.i2c_port = I2C_PORT_TCPC1,
		.i2c_addr = SN5S330_ADDR0,
		.drv = &sn5s330_drv
	},
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

/* BC 1.2 chip Configuration */
const struct bq24392_config_t bq24392_config[CONFIG_USB_PD_PORT_COUNT] = {
	[USB_PD_PORT_ANX74XX] = {
		.chip_enable_pin = GPIO_USB_C0_BC12_VBUS_ON_L,
		.chg_det_pin = GPIO_USB_C0_BC12_CHG_DET,
		.flags = BQ24392_FLAGS_ENABLE_ACTIVE_LOW,
	},
	[USB_PD_PORT_PS8751] = {
		.chip_enable_pin = GPIO_USB_C1_BC12_VBUS_ON_L,
		.chg_det_pin = GPIO_USB_C1_BC12_CHG_DET,
		.flags = BQ24392_FLAGS_ENABLE_ACTIVE_LOW,
	},
};

const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_USB_A0_5V,
	GPIO_EN_USB_A1_5V,
};

static void baseboard_chipset_suspend(void)
{
	/*
	 * Turn off display backlight. This ensures that the backlight stays off
	 * in S3, no matter what the AP has it set to. The AP also controls it.
	 * This is here more for legacy reasons.
	 */
	gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, baseboard_chipset_suspend,
	     HOOK_PRIO_DEFAULT);

static void baseboard_chipset_resume(void)
{
	/* Allow display backlight to turn on. See above backlight comment */
	gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 0);

}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, baseboard_chipset_resume, HOOK_PRIO_DEFAULT);

static void baseboard_chipset_startup(void)
{
	/*
	 * Enable sensor power (lid accel, gyro) in S3 for calculating the lid
	 * angle (needed on convertibles to disable resume from keyboard in
	 * tablet mode).
	 */
	gpio_set_level(GPIO_EN_PP1800_SENSOR, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, baseboard_chipset_startup,
	     HOOK_PRIO_DEFAULT);

static void baseboard_chipset_shutdown(void)
{
	/* Disable sensor power (lid accel, gyro) in S5. */
	gpio_set_level(GPIO_EN_PP1800_SENSOR, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, baseboard_chipset_shutdown,
	     HOOK_PRIO_DEFAULT);

int board_set_active_charge_port(int port)
{
	int i;

	CPRINTS("New chg p%d", port);

	if (port == CHARGE_PORT_NONE) {
		/* Disable all ports. */
		for (i = 0; i < ppc_cnt; i++) {
			if (ppc_vbus_sink_enable(i, 0))
				CPRINTS("p%d: sink disable failed.", i);
		}

		return EC_SUCCESS;
	}

	/* Check if the port is sourcing VBUS. */
	if (ppc_is_sourcing_vbus(port)) {
		CPRINTF("Skip enable p%d", port);
		return EC_ERROR_INVAL;
	}

	/*
	 * Turn off the other ports' sink path FETs, before enabling the
	 * requested charge port.
	 */
	for (i = 0; i < ppc_cnt; i++) {
		if (i == port)
			continue;

		if (ppc_vbus_sink_enable(i, 0))
			CPRINTS("p%d: sink disable failed.", i);
	}

	/* Enable requested charge port. */
	if (ppc_vbus_sink_enable(port, 1)) {
		CPRINTS("p%d: sink enable failed.");
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	charge_set_input_current_limit(MAX(charge_ma,
					   CONFIG_CHARGER_INPUT_CURRENT),
				       charge_mv);
}

/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	/* Extra delay when KSO2 is tied to Cr50. */
	.output_settle_us = 60,
	.debounce_down_us = 6 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 1500,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = SECOND,
	.actual_key_mask = {
		0x3c, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};

/*
 * We use 11 as the scaling factor so that the maximum mV value below (2761)
 * can be compressed to fit in a uint8_t.
 */
#define THERMISTOR_SCALING_FACTOR 11

/*
 * Values are calculated from the "Resistance VS. Temperature" table on the
 * Murata page for part NCP15WB473F03RC. Vdd=3.3V, R=30.9Kohm.
 */
static const struct thermistor_data_pair thermistor_data[] = {
	{ 2761 / THERMISTOR_SCALING_FACTOR, 0},
	{ 2492 / THERMISTOR_SCALING_FACTOR, 10},
	{ 2167 / THERMISTOR_SCALING_FACTOR, 20},
	{ 1812 / THERMISTOR_SCALING_FACTOR, 30},
	{ 1462 / THERMISTOR_SCALING_FACTOR, 40},
	{ 1146 / THERMISTOR_SCALING_FACTOR, 50},
	{ 878 / THERMISTOR_SCALING_FACTOR, 60},
	{ 665 / THERMISTOR_SCALING_FACTOR, 70},
	{ 500 / THERMISTOR_SCALING_FACTOR, 80},
	{ 434 / THERMISTOR_SCALING_FACTOR, 85},
	{ 376 / THERMISTOR_SCALING_FACTOR, 90},
	{ 326 / THERMISTOR_SCALING_FACTOR, 95},
	{ 283 / THERMISTOR_SCALING_FACTOR, 100}
};

static const struct thermistor_info thermistor_info = {
	.scaling_factor = THERMISTOR_SCALING_FACTOR,
	.num_pairs = ARRAY_SIZE(thermistor_data),
	.data = thermistor_data,
};

static int board_get_temp(int idx, int *temp_k)
{
	/* idx is the sensor index set below in temp_sensors[] */
	int mv = adc_read_channel(
		idx ? ADC_TEMP_SENSOR_SOC : ADC_TEMP_SENSOR_CHARGER);
	int temp_c;

	if (mv < 0)
		return -1;

	temp_c = thermistor_linear_interpolate(mv, &thermistor_info);
	*temp_k = C_TO_K(temp_c);
	return 0;
}

const struct temp_sensor_t temp_sensors[] = {
	{"Charger", TEMP_SENSOR_TYPE_BOARD, board_get_temp, 0, 1},
	{"SOC", TEMP_SENSOR_TYPE_BOARD, board_get_temp, 1, 5},
	{"CPU", TEMP_SENSOR_TYPE_CPU, sb_tsi_get_val, 0, 4},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/* Motion sensors */
static struct mutex g_lid_mutex;
static struct mutex g_base_mutex;

/*
 * Matrix to rotate accelerator into standard reference frame
 *
 * TODO(teravest): Update this when we can physically test a Grunt.
 */
const matrix_3x3_t base_standard_ref = {
	{ 0, FLOAT_TO_FP(-1), 0},
	{ FLOAT_TO_FP(1), 0,  0},
	{ 0, 0,  FLOAT_TO_FP(1)}
};

/* sensor private data */
static struct kionix_accel_data g_kx022_data;
static struct bmi160_drv_data_t g_bmi160_data;

struct motion_sensor_t motion_sensors[] = {
	[LID_ACCEL] = {
	 .name = "Lid Accel",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_KX022,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &kionix_accel_drv,
	 .mutex = &g_lid_mutex,
	 .drv_data = &g_kx022_data,
	 .port = I2C_PORT_SENSOR,
	 .addr = KX022_ADDR1,
	 .rot_standard_ref = NULL, /* Identity matrix. */
	 .default_range = 2, /* g, enough for laptop. */
	 .min_frequency = KX022_ACCEL_MIN_FREQ,
	 .max_frequency = KX022_ACCEL_MAX_FREQ,
	 .config = {
		/* EC use accel for angle detection */
		[SENSOR_CONFIG_EC_S3] = {
			.odr = 10000 | ROUND_UP_FLAG,
		},
	 },
	},

	[BASE_ACCEL] = {
	 .name = "Base Accel",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &bmi160_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_SENSOR,
	 .addr = BMI160_ADDR0,
	 .default_range = 2, /* g, enough for laptop */
	 .rot_standard_ref = &base_standard_ref,
	 .min_frequency = BMI160_ACCEL_MIN_FREQ,
	 .max_frequency = BMI160_ACCEL_MAX_FREQ,
	 .config = {
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			.odr = 10000 | ROUND_UP_FLAG,
			.ec_rate = 100,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S3] = {
			.odr = 10000 | ROUND_UP_FLAG,
		 },
	 },
	},

	[BASE_GYRO] = {
	 .name = "Base Gyro",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &bmi160_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_SENSOR,
	 .addr = BMI160_ADDR0,
	 .default_range = 1000, /* dps */
	 .rot_standard_ref = &base_standard_ref,
	 .min_frequency = BMI160_GYRO_MIN_FREQ,
	 .max_frequency = BMI160_GYRO_MAX_FREQ,
	},
};

const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

#ifndef TEST_BUILD
void lid_angle_peripheral_enable(int enable)
{
	keyboard_scan_enable(enable, KB_SCAN_DISABLE_LID_ANGLE);
}
#endif

static const int sku_thresh_mv[] = {
	/* Vin = 3.3V, Ideal voltage, R2 values listed below */
	/* R1 = 51.1 kOhm */
	200,  /* 124 mV, 2.0 Kohm */
	366,  /* 278 mV, 4.7 Kohm */
	550,  /* 456 mV, 8.2  Kohm */
	752,  /* 644 mV, 12.4 Kohm */
	927,  /* 860 mV, 18.0 Kohm */
	1073, /* 993 mV, 22.0 Kohm */
	1235, /* 1152 mV, 27.4 Kohm */
	1386, /* 1318 mV, 34.0 Kohm */
	1552, /* 1453 mV, 40.2 Kohm */
	/* R1 = 10.0 kOhm */
	1739, /* 1650 mV, 10.0 Kohm */
	1976, /* 1827 mV, 12.4 Kohm */
	2197, /* 2121 mV, 18.0 Kohm */
	2344, /* 2269 mV, 22.0 Kohm */
	2484, /* 2418 mV, 27.4 Kohm */
	2636, /* 2550 mV, 34.0 Kohm */
	2823, /* 2721 mV, 47.0 Kohm */
};

static int board_read_sku_adc(enum adc_channel chan)
{
	int mv;
	int i;

	mv = adc_read_channel(chan);

	if (mv == ADC_READ_ERROR)
		return -1;

	for (i = 0; i < ARRAY_SIZE(sku_thresh_mv); i++)
		if (mv < sku_thresh_mv[i])
			return i;

	return -1;
}

uint32_t system_get_sku_id(void)
{
	static uint32_t sku_id = -1;
	int sku_id1, sku_id2;

	if (sku_id != -1)
		return sku_id;

	sku_id1 = board_read_sku_adc(ADC_SKU_ID1);
	sku_id2 = board_read_sku_adc(ADC_SKU_ID2);

	if (sku_id1 < 0 || sku_id2 < 0)
		return 0;

	sku_id = (sku_id2 << 4) | sku_id1;
	return sku_id;
}
