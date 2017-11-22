/* -*- mode:c -*-
 *
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(USB_C0_PD_INT_ODL,	PIN(6, 1), GPIO_INT_FALLING,			tcpc_alert_event)
GPIO_INT(USB_C1_PD_INT_ODL,	PIN(F, 5), GPIO_INT_FALLING,			tcpc_alert_event)
GPIO_INT(PCH_SLP_S3_L,		PIN(A, 6), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(PCH_SLP_S5_L,		PIN(A, 3), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(VGATE,			PIN(F, 4), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(SPOK,			PIN(6, 3), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(POWER_BUTTON_L,	PIN(0, 1), GPIO_INT_BOTH,			power_button_interrupt)
GPIO_INT(LID_OPEN,		PIN(D, 2), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH,	lid_interrupt)
GPIO_INT(AC_PRESENT,		PIN(0, 0), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH,	extpower_interrupt)
GPIO_INT(WP_L,			PIN(A, 1), GPIO_INT_BOTH,			switch_interrupt)
GPIO_INT(VOLDN_BTN_ODL,		PIN(7, 0), GPIO_INT_BOTH | GPIO_PULL_UP,	button_interrupt)
GPIO_INT(VOLUP_BTN_ODL,		PIN(7, 5), GPIO_INT_BOTH | GPIO_PULL_UP,	button_interrupt)

GPIO(EN_PWR_A,			PIN(E, 2), GPIO_OUT_HIGH)			/* Enable Power */
GPIO(EN_PP1800_SENSOR,		PIN(6, 7), GPIO_OUT_HIGH)			/* Enable Power */
GPIO(ENABLE_BACKLIGHT,		PIN(D, 3), GPIO_OUT_LOW)			/* Enable Backlight */
GPIO(PCH_RSMRST_L,		PIN(C, 2), GPIO_OUT_LOW)			/* RSMRST# to SOC */
GPIO(PCH_PWRBTN_L,		PIN(C, 1), GPIO_OUT_HIGH)			/* Power Button to SOC */
GPIO(PCH_WAKE_L,		PIN(7, 4), GPIO_OUT_HIGH)			/* Wake SOC */
GPIO(PCH_RCIN_L,		PIN(0, 2), GPIO_OUT_HIGH)			/* Cold Reset to SOC */
GPIO(CCD_MODE_ODL,		PIN(E, 3), GPIO_INPUT)				/* Case Closed Debug Mode */
GPIO(ENTERING_RW,		PIN(E, 1), GPIO_OUT_LOW)			/* EC Entering RW */
GPIO(EC_BATT_PRES_ODL,		PIN(E, 5), GPIO_INPUT)				/* Battery Present */
GPIO(PCH_SYS_PWROK,		PIN(3, 7), GPIO_OUT_LOW)			/* Power OK to SOC */
GPIO(EC_APU_RST,		PIN(E, 4), GPIO_INPUT)				/* Reset to SOC */
GPIO(CPU_PROCHOT,		PIN(3, 4), GPIO_INPUT | GPIO_PULL_UP)		/* PROCHOT to SOC */
GPIO(EC_APU_ALERT_L,		PIN(4, 0), GPIO_INPUT | GPIO_PULL_UP)		/* Alert to SOC */

/* I2C pins - these will be reconfigured for alternate function below */
GPIO(I2C0_SCL,			PIN(B, 5), GPIO_INPUT)				/* EC_I2C_POWER_SCL */
GPIO(I2C0_SDA,			PIN(B, 4), GPIO_INPUT)				/* EC_I2C_POWER_SDA */
GPIO(I2C1_SCL,			PIN(9, 0), GPIO_INPUT)				/* EC_I2C_USB_C0_PD_SCL */
GPIO(I2C1_SDA,			PIN(8, 7), GPIO_INPUT)				/* EC_I2C_USB_C0_PD_SDA */
GPIO(I2C2_SCL,			PIN(9, 2), GPIO_INPUT)				/* EC_I2C_USB_C1_PD_SCL */
GPIO(I2C2_SDA,			PIN(9, 1), GPIO_INPUT)				/* EC_I2C_USB_C1_PD_SDA */
GPIO(I2C3_SCL,			PIN(D, 1), GPIO_INPUT)				/* APU_SIC */
GPIO(I2C3_SDA,			PIN(D, 0), GPIO_INPUT)				/* APU_SID */
GPIO(I2C7_SCL,			PIN(B, 3), GPIO_INPUT)				/* EC_I2C_SENSOR_SCL */
GPIO(I2C7_SDA,			PIN(B, 2), GPIO_INPUT)				/* EC_I2C_SENSOR_SDA */

GPIO(PCH_SCI_ODL,		PIN(7, 6), GPIO_ODR_HIGH)
GPIO(PCH_SMI_ODL,		PIN(C, 6), GPIO_ODR_HIGH)
GPIO(PCH_SLP_S0_L,		PIN(A, 4), GPIO_INPUT)

GPIO(EN_USB_A0_5V,		PIN(A, 0), GPIO_OUT_LOW)			/* Enable A0 5V Charging */
GPIO(EN_USB_A1_5V,		PIN(C, 0), GPIO_OUT_LOW)			/* Enable A1 5V Charging */
GPIO(USB_A0_CHARGE_EN_L,	PIN(A, 2), GPIO_OUT_HIGH)			/* Enable A0 1.5A Charging */
GPIO(USB_A1_CHARGE_EN_L,	PIN(D, 4), GPIO_OUT_HIGH)			/* Enable A1 1.5A Charging */
GPIO(EN_USB_C0_TCPC_PWR,	PIN(6, 0), GPIO_OUT_LOW)			/* Enable C0 TCPC Power */
GPIO(USB_C0_OC_L,		PIN(7, 3), GPIO_OUT_HIGH)			/* C0 Over Current */
GPIO(USB_C1_OC_L,		PIN(D, 7), GPIO_OUT_HIGH)			/* C1 Over Current */
GPIO(USB_C0_PD_RST_L,		PIN(F, 1), GPIO_OUT_HIGH)			/* C0 PD Reset */
GPIO(USB_C1_PD_RST_L,		PIN(D, 5), GPIO_OUT_HIGH)			/* C1 PD Reset */
GPIO(USB_C0_SWCTL_INT_ODL,	PIN(0, 3), GPIO_INPUT)				/* C0 Switch Control Interrupt */
GPIO(USB_C1_SWCTL_INT_ODL,	PIN(0, 4), GPIO_INPUT)				/* C1 Switch Control Interrupt */
GPIO(USB_C0_BC12_VBUS_ON_ODL,	PIN(8, 2), GPIO_ODR_HIGH)			/* C0 BC1.2 Power */
GPIO(USB_C1_BC12_VBUS_ON_ODL,	PIN(B, 1), GPIO_ODR_HIGH)			/* C1 BC1.2 Power */
GPIO(USB_C0_BC12_CHG_MAX,	PIN(6, 2), GPIO_INPUT)				/* C0 BC1.2 Detect */
GPIO(USB_C1_BC12_CHG_MAX,	PIN(8, 3), GPIO_INPUT)				/* C1 BC1.2 Detect */

/* Board ID */
GPIO(BOARD_VERSION1,		PIN(9, 6), GPIO_INPUT)
GPIO(BOARD_VERSION2,		PIN(9, 3), GPIO_INPUT)
GPIO(BOARD_VERSION3,		PIN(8, 0), GPIO_INPUT)
GPIO(SKU_ID1,			PIN(F, 0), GPIO_INPUT)
GPIO(SKU_ID2,			PIN(4, 1), GPIO_INPUT)

/* Alternate functions GPIO definitions */
ALTERNATE(PIN_MASK(6, 0x30), 0, MODULE_UART, GPIO_PULL_UP)			/* UART from EC to Servo */
ALTERNATE(PIN_MASK(B, 0x30), 1, MODULE_I2C, GPIO_PULL_UP)			/* I2C0 */
ALTERNATE(PIN_MASK(9, 0x07), 1, MODULE_I2C, GPIO_PULL_UP)			/* I2C1 SCL / I2C2 */
ALTERNATE(PIN_MASK(8, 0x80), 1, MODULE_I2C, GPIO_PULL_UP)			/* I2C1 SDA */
ALTERNATE(PIN_MASK(D, 0x03), 1, MODULE_I2C, GPIO_PULL_UP)			/* I2C3 */
ALTERNATE(PIN_MASK(B, 0x0C), 1, MODULE_I2C, GPIO_PULL_UP)			/* I2C7 */
ALTERNATE(PIN_MASK(4, 0x30), 1, MODULE_ADC, 0)					/* ADC0-1 */

/* Keyboard Pins */
ALTERNATE(PIN_MASK(3, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_INPUT)		/* KSI_00-01 */
ALTERNATE(PIN_MASK(2, 0xFC), 0, MODULE_KEYBOARD_SCAN, GPIO_INPUT)		/* KSI_02-07 */
ALTERNATE(PIN_MASK(2, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		/* KSO_00-01 */
GPIO(KBD_KSO2, PIN(1, 7), GPIO_OUT_LOW)						/* KSO_02 inverted */
ALTERNATE(PIN_MASK(1, 0x7F), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		/* KSO_03-09 */
ALTERNATE(PIN_MASK(0, 0xE0), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		/* KSO_10-12 */

/* Power Switch Logic (PSL) inputs */
ALTERNATE(PIN_MASK(0, 0x03), 1, MODULE_PMU, 0)					/* GPIO00, GPIO01 */
ALTERNATE(PIN_MASK(D, 0x04), 1, MODULE_PMU, 0)					/* GPIOD2 */