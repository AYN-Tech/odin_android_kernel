/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/irqreturn.h>
#include <linux/kd.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/errno.h>
#include <linux/serio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <asm/irq.h>

#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/regmap.h>

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/switch.h>

#ifdef CONFIG_FB
#include <linux/msm_drm_notify.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define MIPI_1080P
#define LT8912_HDP_WORK

#define HDMI_PULL_OUT	0
#define HDMI_PULL_IN	1
#define HDMI_UNUSED	2

//int dp_mode = 0;
//extern int dp_display_connected;
//static struct task_struct *hdmi_tast = NULL;
//#define LT8912_HDP_IRQ


/**
 * struct lt8912_private - Cached chip configuration data
 * @client: I2C client
 * @dev: device structure
 */
struct lt8912_private {
	struct i2c_client *lt8912_client;
	struct switch_dev switch_dev;
	struct regmap *regmap;
	int pwren_gpio;
	int reset_gpio;
	int hdmien_gpio;
	int hdmidet_gpio;
	int hpd_gpio;
	#ifdef LT8912_HDP_IRQ
	int hpd_irq;
	#endif
	bool audio_enable;
	int mConnect;
	//	int main_i2c_addr;
	//	int cec_dsi_i2c_addr;
#ifdef LT8912_HDP_WORK
	struct delayed_work hpd_work;
#endif
#ifdef CONFIG_FB
	struct notifier_block fb_notifier;
	bool fb_ready;
#endif
};

struct kobject It8912_kobj;

static int lt8912_i2c_write_byte(struct lt8912_private *data,
		unsigned int reg, unsigned int val)
{
	int rc = 0;

	rc = regmap_write(data->regmap, reg, val);
	if (rc) {
		dev_err(&data->lt8912_client->dev,
				"write 0x%02x register failed\n", reg);
		return rc;
	}

	return 0;
}
//#ifdef  LT8912_HDP_WORK
static int lt8912_i2c_read_byte(struct lt8912_private *data,
		unsigned int reg, unsigned int* val)
{
	int rc = 0;

	rc = regmap_read(data->regmap, reg, val);
	if (rc) {
		dev_err(&data->lt8912_client->dev,
				"read 0x%02x register failed\n", reg);
		return rc;
	}

	return 0;
}
//#endif


static int digital_clock_enable(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x08, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x09, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x0a, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x0b, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x0c, 0xff);
	if (rc)
		return rc;

	return 0;
}

static int tx_analog(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x31, 0xa1);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x32, 0xb9);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x33, 0x17);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x37, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x38, 0x22);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x60, 0x82);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3a, 0x00);
	if (rc)
		return rc;

	return 0;
}

static int cbus_analog(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x39, 0x45);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3b, 0x00);
	if (rc)
		return rc;

	return 0;
}

static int hdmi_pll_analog(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x44, 0x31);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x55, 0x44);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x57, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5a, 0x02);
	if (rc)
		return rc;
	return 0;
}

static int mipi_rx_logic_res(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x48;
	rc = lt8912_i2c_write_byte(data, 0x03, 0x7f);
	if (rc)
		return rc;

	mdelay(100);

	rc = lt8912_i2c_write_byte(data, 0x03, 0xff);
	if (rc)
		return rc;

	return 0;
}

static int mipi_basic_set(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;

	/* term en  To analog phy for trans lp mode to hs mode */
	rc = lt8912_i2c_write_byte(data, 0x10, 0x20);
	if (rc)
		return rc;

	/* settle Set timing for dphy trans state from PRPR to SOT state */
	rc = lt8912_i2c_write_byte(data, 0x11, 0x04);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x12, 0x04);
	if (rc)
		return rc;

	/* 4 lane, 01 lane, 02 2 lane, 03 3lane */
	rc = lt8912_i2c_write_byte(data, 0x13, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x14, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x15, 0x00);
	if (rc)
		return rc;

	/* hshift 3 */
	rc = lt8912_i2c_write_byte(data, 0x1a, 0x03);
	if (rc)
		return rc;

	/* vshift 3 */
	rc = lt8912_i2c_write_byte(data, 0x1b, 0x03);
	if (rc)
		return rc;
	return 0;
}

#ifdef MIPI_1080P
static int mipi_dig_1080p(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;
	rc = lt8912_i2c_write_byte(data, 0x18, 0x2c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x19, 0x05);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1c, 0x80);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1d, 0x07);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2f, 0x0c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x34, 0x98);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x35, 0x08);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x36, 0x65);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x37, 0x04);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x38, 0x24);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x39, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3a, 0x04);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3b, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3c, 0x94);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3d, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3e, 0x58);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3f, 0x00);
	if (rc)
		return rc;
	return 0;
}
#endif

#ifdef MIPI_720P
static int mipi_dig_720p(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;
	rc = lt8912_i2c_write_byte(data, 0x18, 0x28);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x19, 0x05);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1c, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1d, 0x05);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1e, 0x67);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2f, 0x0c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x34, 0x72);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x35, 0x06);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x36, 0xee);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x37, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x38, 0x14);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x39, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3a, 0x05);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3b, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3c, 0xdc);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3d, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3e, 0x6e);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3f, 0x00);
	if (rc)
		return rc;
	return 0;
}
#endif

#ifdef MIPI_480P
static int mipi_dig_480p(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;
	rc = lt8912_i2c_write_byte(data, 0x18, 0x60);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x19, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1c, 0x80);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1d, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1e, 0x67);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2f, 0x0c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x34, 0x20);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x35, 0x03);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x36, 0x0d);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x37, 0x02);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x38, 0x20);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x39, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3a, 0x0a);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3b, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3c, 0x30);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3d, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3e, 0x10);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x3f, 0x00);
	if (rc)
		return rc;
	return 0;
}
#endif

static int dds_config(struct lt8912_private *data)
{
	int rc = 0;

	data->lt8912_client->addr = 0x49;

	/* strm_sw_freq_word[ 7: 0] */
	rc = lt8912_i2c_write_byte(data, 0x4e, 0x6A);
	if (rc)
		return rc;

	/* strm_sw_freq_word[15: 8] */
	rc = lt8912_i2c_write_byte(data, 0x4f, 0x4D);
	if (rc)
		return rc;

	/* strm_sw_freq_word[23:16] */
	rc = lt8912_i2c_write_byte(data, 0x50, 0xF3);
	if (rc)
		return rc;

	/* [0]=strm_sw_freq_word[24]//[7]=strm_sw_freq_word_en=0,
	   [6]=strm_err_clr=0 */
	rc = lt8912_i2c_write_byte(data, 0x51, 0x80);
	if (rc)
		return rc;

	/* full_value  464 */
	rc = lt8912_i2c_write_byte(data, 0x1f, 0x90);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x20, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x21, 0x68);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x22, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x23, 0x5E);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x24, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x25, 0x54);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x26, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x27, 0x90);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x28, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x29, 0x68);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2a, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2b, 0x5E);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2c, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2d, 0x54);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x2e, 0x01);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x42, 0x64);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x43, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x44, 0x04);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x45, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x46, 0x59);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x47, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x48, 0xf2);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x49, 0x06);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x4a, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x4b, 0x72);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x4c, 0x45);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x4d, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x52, 0x08);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x53, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x54, 0xb2);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x55, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x56, 0xe4);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x57, 0x0d);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x58, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x59, 0xe4);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5a, 0x8a);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5b, 0x00);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5c, 0x34);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x1e, 0x4f);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x51, 0x00);
	if (rc)
		return rc;
	return 0;
}

static void audio_config(struct lt8912_private *data, bool on) 
{
	//pr_err("tianyx, lt8912, %s, audio stats = %s\n", __func__, on ? "on" : "off");
	// audio i2s
	data->lt8912_client->addr = 0x48;  // I2C_ADDR_MAIN
	lt8912_i2c_write_byte(data, 0xb2, on ? 0x01 : 0x00);

	data->lt8912_client->addr = 0x4a;  //I2C_ADDR_I2S
	lt8912_i2c_write_byte(data, 0x06, 0x08);
	lt8912_i2c_write_byte(data, 0x07, 0xF0);
	lt8912_i2c_write_byte(data, 0x34, 0xe2); //16bit 0xe2 32bit 0xd2
	lt8912_i2c_write_byte(data, 0x3c, 0x41);// Null Packet enable

	// audio rxlogicres
	data->lt8912_client->addr = 0x48;  //I2C_ADDR_MAIN
	//lt8912_i2c_write_byte(data, 0xb2, 0x01);
	//usleep_range(5000, 10000);
	lt8912_i2c_write_byte(data, 0x03, 0x7f);
	usleep_range(5000, 10000);
	lt8912_i2c_write_byte(data, 0x03, 0xff);

	data->lt8912_client->addr = 0x49;  //I2C_ADDR_CEC_DSI
	lt8912_i2c_write_byte(data, 0x51, 0x80);
	usleep_range(5000, 10000);
	lt8912_i2c_write_byte(data, 0x51, 0x00);	
}
#if 0
static int low_power_suspend(struct lt8912_private *data)
{
	int rc = 0;
	
	if (!data)
	return -1;
	
	data->lt8912_client->addr = 0x48;
	//rc = lt8912_i2c_write_byte(data, 0x08, 0xff);
	//if (rc)
	//	return rc;
	rc = lt8912_i2c_write_byte(data, 0x09, 0xff);
	if (rc)
		return rc;

	rc = lt8912_i2c_write_byte(data, 0x54, 0x1d);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x51, 0x15);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x44, 0x31);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x41, 0xbd);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5c, 0x11);
	if (rc)
		return rc;
	
	rc = lt8912_i2c_write_byte(data, 0x09, 0x80);
	if (rc)
		return rc;
	
	rc = lt8912_i2c_write_byte(data, 0x33, 0x0c);
		if (rc)
			return rc;
	
	return 0;
}

static int low_power_resume(struct lt8912_private *data)
{
	int rc = 0;
	if (!data)
		return -1;
	data->lt8912_client->addr = 0x48;

	rc = lt8912_i2c_write_byte(data, 0x09, 0x80);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x5c, 0x10);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x54, 0x1c);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x51, 0x2d);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x44, 0x30);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x41, 0xbc);
	if (rc)
		return rc;
	#if 0
	mdelay(10);
	rc = lt8912_i2c_write_byte(data, 0x03, 0x7f);
	if (rc)
		return rc;
	mdelay(10);
	rc = lt8912_i2c_write_byte(data, 0x03, 0xff);
	if (rc)
		return rc;
	rc = lt8912_i2c_write_byte(data, 0x05, 0xfb);
	if (rc)
		return rc;
	mdelay(10);
	rc = lt8912_i2c_write_byte(data, 0x05, 0xff);
	if (rc)
		return rc;
	#endif
	return 0;
}
#endif

#ifdef LT8912_HDP_IRQ
static irqreturn_t hpd_isr(int irq, void *data)
{
	struct lt8912_private *pdata = data;

	if (gpio_is_valid(pdata->hpd_gpio)) {
		int hpd = gpio_get_value(pdata->hpd_gpio);
		/* Fixme: do something while hdmi plugin */
		pr_debug("lt8912, hpd isr, hpd=%d\n", hpd);
	}

	return IRQ_HANDLED;
}
#endif

static int lt8912_parse_dt(struct device *dev,
		struct lt8912_private *pdata)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	pdata->audio_enable = of_property_read_bool(np, "lt,enable-audio");
	if (pdata->audio_enable < 0) {
		pr_err("lt,audio_enable not defined.\n");
	}
#ifdef LT8912_HDP_IRQ
	pdata->hpd_gpio = of_get_named_gpio(np, "lt,hpd-gpio", 0);
	if (pdata->hpd_gpio < 0) {
		pr_err("lt,hpd-gpio not defined.\n");
	}
#endif
	pdata->pwren_gpio = of_get_named_gpio(np, "lt,pwren-gpio", 0);
	if (pdata->pwren_gpio < 0) {
		pr_err("lt,pwren-gpio not defined.\n");
	}

	pdata->reset_gpio = of_get_named_gpio(np, "lt,reset-gpio", 0);
	if (pdata->reset_gpio < 0) {
		pr_err("lt,reset-gpio not defined.\n");
	}
	
	pdata->hdmien_gpio = of_get_named_gpio(np, "lt,hdmien-gpio", 0);
	if (pdata->hdmien_gpio < 0) {
		pr_err("lt,hdmien-gpio not defined.\n");
	}
	
	pdata->hdmidet_gpio = of_get_named_gpio(np, "lt,hdmidet-gpio", 0);
	if (pdata->hdmidet_gpio < 0) {
		pr_err("lt,hdmidet-gpio not defined.\n");
	}

	return ret;
}

static int lt8912_request_gpio(struct lt8912_private *data)
{
	int ret = 0;

	if (gpio_is_valid(data->pwren_gpio)) {
		ret = gpio_request(data->pwren_gpio, "lt8912_pwren_gpio");
		if (ret) {
			pr_err("pwren_gpio request failed");
			ret = -1;
			return ret;
		}
	}


	if (gpio_is_valid(data->reset_gpio)) {
		ret = gpio_request(data->reset_gpio, "lt8912_reset_gpio");
		if (ret) {
			pr_err("reset gpio request failed");
			ret = -1;
			return ret;
		}
	}
	
	if (gpio_is_valid(data->hdmien_gpio)) {
		ret = gpio_request(data->hdmien_gpio, "lt8912_hdmien_gpio");
		if (ret) {
			pr_err("hdmien_gpio request failed");
			ret = -1;
			return ret;
		}
	}
	
	if (gpio_is_valid(data->hdmidet_gpio)) {
		ret = gpio_request(data->hdmidet_gpio, "lt8912_hdmidet_gpio");
		if (ret) {
			pr_err("hdmidet_gpio request failed");
			ret = -1;
			return ret;
		}
	}

	return ret;
}
#ifdef LT8912_HDP_IRQ
static int lt8912_request_irq(struct lt8912_private *data)
{
	int ret = 0;

	if (gpio_is_valid(data->hpd_gpio)) {
		data->hpd_irq = gpio_to_irq(data->hpd_gpio);
		if (data->hpd_irq < 0) {
			pr_err("failed to get gpio irq\n");
			ret = -1;
			return ret;
		}

		ret = request_threaded_irq(data->hpd_irq, NULL, hpd_isr,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING
				| IRQF_ONESHOT, "lt8912-hpd-isr", data);		
		if (ret < 0) {
			pr_err( "failed to request irq\n");
			data->hpd_irq = -1;
			ret = -1;
			return ret;
		}
	}

	return ret;
}
#endif
static struct regmap_config lt8912_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};


static int lt8912_power_set(struct lt8912_private *pdata, bool on)
{
	int ret = 0;

	if (!gpio_is_valid(pdata->pwren_gpio) ||
			!gpio_is_valid(pdata->reset_gpio)) {

		pr_err("%s cannot set gpio\n", __FUNCTION__);
		ret = -1;
		return ret;
	}

	if (on) {
		gpio_direction_output(pdata->pwren_gpio, 1);
		mdelay(100);

		gpio_direction_output(pdata->reset_gpio, 1);
		mdelay(20);
		gpio_direction_output(pdata->reset_gpio, 0);
		mdelay(50);
		gpio_direction_output(pdata->reset_gpio, 1);
		mdelay(20);

	} else {
		gpio_direction_output(pdata->reset_gpio, 0);
		gpio_direction_output(pdata->pwren_gpio, 0);
	}

	return ret;
}

static void lt8912_init(struct lt8912_private *data)
{
	if (!data)
		return;

	if (digital_clock_enable(data))
		return;

	if (tx_analog(data))
		return;

	if (cbus_analog(data))
		return;

	if (hdmi_pll_analog(data))
		return;

	if (mipi_basic_set(data))
		return;

#ifdef MIPI_1080P
	if (mipi_dig_1080p(data))
		return;
#elif defined MIPI_720P
	if (mipi_dig_720p(data))
		return;
#elif defined MIPI_480P
	if (mipi_dig_480p(data))
		return;
#endif

	if (dds_config(data))
		return;

	if (mipi_rx_logic_res(data))
		return;

	audio_config(data, data->audio_enable);

	//	pr_info("%s: init lt8912 -\n", __func__);
}

int default_display_connected = HDMI_PULL_OUT;
EXPORT_SYMBOL(default_display_connected);

int /*g_power_set = 0, */g_suspend = 0,low_power_suspend_flag = 0,low_power_resume_flag = 0;
#ifdef LT8912_HDP_WORK
static void lt8912_hpd_work_fn(struct work_struct *work)
{
	struct lt8912_private *data;
	int connected = 0;
	//unsigned int reg_val;
	//int ret = 100;
	data = container_of((struct delayed_work *)work, struct lt8912_private, hpd_work);
#if 0
	if(g_power_set == 1)
	{
		lt8912_power_set(data, true);
		//lt8912_init(data);
		data->lt8912_client->addr = 0x48;
		lt8912_i2c_write_byte(data, 0x09, 0x80);
		g_power_set = 0;
		//g_reinit = 1;
		printk("[kevin] lt8912_power_set true\n");
	}
	
	data->lt8912_client->addr = 0x48;
	lt8912_i2c_read_byte(data, 0xC1, &reg_val);
	connected  = (reg_val & BIT(7));
	printk("[kevin]%s: reg[0x%x]=0x%x connected = %d,low_power_suspend_flag = %d,low_power_resume_flag = %d\n", 
	__func__, 0xc1, reg_val, connected,low_power_suspend_flag,low_power_resume_flag);
	
	if(low_power_suspend_flag == 1 || low_power_resume_flag == 1){
		if(connected == 128 && low_power_resume_flag == 1)
		{
			lt8912_power_set(data, false);
			lt8912_power_set(data, true);
			//low_power_resume(data);
			lt8912_init(data);
			low_power_resume_flag = 0;
			low_power_suspend_flag = 1;
			printk("[kevin] low_power_resume\n");
		}else if(connected == 0 && low_power_suspend_flag == 1){
			lt8912_power_set(data, false);
			lt8912_power_set(data, true);
			low_power_suspend(data);
			low_power_suspend_flag = 0;
			low_power_resume_flag = 1;
			printk("[kevin] low_power_suspend\n");
		}
		
	}
#else
	connected = gpio_get_value(data->hdmidet_gpio);
	//printk("[kevin]%s: connected = %d,low_power_suspend_flag = %d,low_power_resume_flag = %d\n", __func__, connected,low_power_suspend_flag,low_power_resume_flag);
	if(low_power_suspend_flag == 1 || low_power_resume_flag == 1){
		if(connected == 1 && low_power_resume_flag == 1)
		{
			//lt8912_power_set(data, false);
			lt8912_power_set(data, true);
			//low_power_resume(data);
			lt8912_init(data);
			low_power_resume_flag = 0;
			low_power_suspend_flag = 1;
			printk("[kevin] low_power_resume\n");
		}else if(connected == 0 && low_power_suspend_flag == 1){
			lt8912_power_set(data, false);
			//lt8912_power_set(data, true);
			//low_power_suspend(data);
			low_power_suspend_flag = 0;
			low_power_resume_flag = 1;
			printk("[kevin] low_power_suspend\n");
		}
	}
#endif
	//pr_err("%s: reg[0x%x]=0x%x connected = %d\n", __func__, 0xc1, reg_val, connected);
#if 0
	//video check
	lt8912_i2c_read_byte(data, 0x9C, &reg_val);
	pr_err("[kevin]%s: 0x9c = 0x%x\n", __func__, reg_val);

	lt8912_i2c_read_byte(data, 0x9D, &reg_val);
	pr_err("[kevin]%s: 0x9d = 0x%x\n", __func__, reg_val);

	lt8912_i2c_read_byte(data, 0x9E, &reg_val);
	pr_err("[kevin]%s: 0x9e = 0x%x\n", __func__, reg_val);

	lt8912_i2c_read_byte(data, 0x9F, &reg_val);
	pr_err("[kevin]%s: 0x9f = 0x%x\n", __func__, reg_val);
	//pixel check
	lt8912_i2c_read_byte(data, 0x09, &reg_val);
	pr_err("[kevin]%s: 0x09 = 0x%x\n", __func__, reg_val);
	lt8912_i2c_read_byte(data, 0x0A, &reg_val);
	pr_err("[kevin]%s: 0x0a = 0x%x\n", __func__, reg_val);
	lt8912_i2c_read_byte(data, 0x0B, &reg_val);
	pr_err("[kevin]%s: 0x0b = 0x%x\n", __func__, reg_val);
	lt8912_i2c_read_byte(data, 0x0C, &reg_val);
	pr_err("[kevin]%s: 0x0c = 0x%x\n", __func__, reg_val);

	lt8912_i2c_read_byte(data, 0x0D, &reg_val);
	pr_err("[kevin]%s: 0x0d = 0x%x\n", __func__, reg_val);

	lt8912_i2c_read_byte(data, 0x0E, &reg_val);
	pr_err("[kevin]%s: 0x0e = 0x%x\n", __func__, reg_val);

	lt8912_i2c_read_byte(data, 0x0F, &reg_val);
	pr_err("[kevin]%s: 0x0f = 0x%x\n", __func__, reg_val);

	//pixel check 0x92
	data->lt8912_client->addr = 0x49;
	lt8912_i2c_read_byte(data, 0x0C, &reg_val);
	pr_err("[kevin-0x49]%s: 0x0c = 0x%x\n", __func__, reg_val);

	lt8912_i2c_read_byte(data, 0x0D, &reg_val);
	pr_err("[kevin-0x49]%s: 0x0d = 0x%x\n", __func__, reg_val);

	lt8912_i2c_read_byte(data, 0x0E, &reg_val);
	pr_err("[kevin-0x49]%s: 0x0e = 0x%x\n", __func__, reg_val);
#endif
#if 0
{
	static int cnt = 10;  // for debug
	cnt += 1;
	if(cnt > 10)
	{
		cnt = 0;
		data->mConnect = (data->mConnect == 0 ? 1 : 0);
		pr_err("tianyx, lt8912, switch_set_state(&data->switch_dev, %d);\n", data->mConnect);
		switch_set_state(&data->switch_dev, data->mConnect);
	}
}
#else
    if(data->mConnect != connected)
    {
		data->mConnect = connected;
		// if(data->mConnect!=0){
		// 	ret = kobject_uevent_env(It8912_kobj.parent, KOBJ_CHANGE, def_hdim_on);
		// }else{
		// 	ret = kobject_uevent_env(It8912_kobj.parent, KOBJ_CHANGE, def_hdim_off);
		// }
		//printk("<3>""oncethings new connected:%d",connected);
		if(data->mConnect!=0){
			//default_display_connected = 1;
			default_display_connected = HDMI_PULL_IN;
			printk("<3>""oncethings hdmi driver connected:%d",default_display_connected);
		}else{
			//default_display_connected = 0;
			default_display_connected = HDMI_PULL_OUT;
			printk("<3>""oncethings hdmi driver connected:%d",default_display_connected);
		}
		switch_set_state(&data->switch_dev, !!data->mConnect);	
    }

	// printk("<3>""oncethings dp_display send dp_display_connected:%d",dp_display_connected);
	// if(dp_mode == 1){
	// 	kobject_uevent_env(It8912_kobj.parent, KOBJ_CHANGE, dp_hdim_on);
	// 	printk("<3>""oncethings dp_display send dp_hdim_on");
	// }else if(dp_mode == 0){
	// 	kobject_uevent_env(It8912_kobj.parent, KOBJ_CHANGE, dp_hdim_off);
	// 	printk("<3>""oncethings dp_display send dp_hdim_off");
	// }

#endif
	//pr_err("tianyx, lt8912, schedule_delayed_work \n");

	schedule_delayed_work(&data->hpd_work, msecs_to_jiffies(1000));
}
#endif

#ifdef CONFIG_FB
static int lt8912_suspend(struct lt8912_private *pdata)
{
	pr_err("[kevin]lt8912_suspend +++\n");
	g_suspend = 1;
	low_power_suspend_flag = 0;
	low_power_resume_flag = 0;
#ifdef LT8912_HDP_IRQ
	disable_irq(pdata->hpd_irq);
#endif
#ifdef LT8912_HDP_WORK
	cancel_delayed_work_sync(&pdata->hpd_work);
#endif
	// report hdmi disconnect
	pdata->mConnect = 0;
	if(gpio_is_valid(pdata->hdmidet_gpio)){
		pdata->mConnect = gpio_get_value(pdata->hdmidet_gpio);
	}
	switch_set_state(&pdata->switch_dev, !!pdata->mConnect);

	// power off
	lt8912_power_set(pdata, false);
	//printk("[kevin]lt8912_suspend ---\n");
	gpio_direction_output(pdata->hdmien_gpio, 0);
	return 0;
}

static int lt8912_resume(struct lt8912_private *pdata)
{
	pr_err("[kevin]lt8912_resume +++\n");
	gpio_direction_output(pdata->hdmien_gpio, 1);
	mdelay(10);
	//lt8912_power_set(pdata, true);
	//lt8912_init(pdata);
	if(g_suspend == 1){
		//g_power_set = 1;
		low_power_suspend_flag = 1;
		low_power_resume_flag = 1;
		g_suspend = 0;
	}
#ifdef LT8912_HDP_IRQ
	enable_irq(pdata->hpd_irq);
#endif
#ifdef LT8912_HDP_WORK
	schedule_delayed_work(&pdata->hpd_work, msecs_to_jiffies(1000));
#endif
	//printk("[kevin]lt8912_resume ---\n");

	return 0;
}

static int lt8912_fb_notifier_cb(struct notifier_block *self,
		unsigned long event, void *data)
{
	int *transition;
	struct fb_event *evdata = data;
	struct lt8912_private *lt8912_data =
		container_of(self, struct lt8912_private,
				fb_notifier);

	if (evdata && evdata->data && lt8912_data) {
		transition = evdata->data;
		//pr_debug("[kevin]%s-%d,transition = %d,event = %d\n",__func__,__LINE__,*transition,event);
		if ((event == MSM_DRM_EARLY_EVENT_BLANK) && (lt8912_data->fb_ready == false)) {
			/* Resume */
			if (*transition == MSM_DRM_BLANK_UNBLANK) {
				//pr_debug("[kevin]%s-%d\n",__func__,__LINE__);
				lt8912_resume(lt8912_data);
				lt8912_data->fb_ready = true;
			}
		} else if ((event == MSM_DRM_EVENT_BLANK)&& (lt8912_data->fb_ready == true)) {
			/* Suspend */
			if (*transition == MSM_DRM_BLANK_POWERDOWN) {
				//pr_debug("[kevin]%s-%d\n",__func__,__LINE__);
				lt8912_suspend(lt8912_data);
				lt8912_data->fb_ready = false;
			}
		}
	}

	return 0;
}
#endif

static int lt8912_regist_switch(struct lt8912_private *data)
{
	struct switch_dev *switch_dev = &data->switch_dev;
	int ret = 0;

	switch_dev->name = "hdmi_audio";
	//	switch_data->gpio = pdata->gpio;
	//	switch_data->name_on = pdata->name_on;
	//	switch_data->name_off = pdata->name_off;
	//	switch_data->state_on = pdata->state_on;
	//	switch_data->state_off = pdata->state_off;
	//	switch_data->sdev.print_state = switch_gpio_print_state;

	ret = switch_dev_register(switch_dev);
	if (ret < 0)
	{
		pr_err("switch_dev_register failed\n");
		return ret;
	}

	pr_debug("lt8912_regist_switch success\n");

	return ret;
}

static int lt8912_read_device_rev(struct lt8912_private *pdata)
{
	unsigned int rev = 0;
	int ret;

	pdata->lt8912_client->addr = 0x48;

	ret = lt8912_i2c_read_byte(pdata, 0x00, &rev);
	if(rev != 0x12)
	{
		pr_err("%s, check revsion error, read reg[%x]=0x%x, should be 0x12\n", __func__, 0x00, rev);
		ret = -1;
	}

	return ret;
}

// static int report_hdmi_status(void* data){
// 	int dp_status = 0;
// 	int default_status = 0;
// 	char *def_hdmi_on[2] = {"default_hdmi_on", NULL};
// 	char *def_hdmi_off[2] = {"default_hdmi_off", NULL};
// 	char *dp_hdmi_on[2] = {"dp_hdmi_on", NULL};
// 	char *dp_hdmi_off[2] = {"dp_hdmi_off", NULL};

// 	while(1){
// 		//printk("<3>""oncethings new dp_display_connected:%d default_display_connected:%d",dp_display_connected,default_display_connected);
// 		if(dp_display_connected != DP_UNUSED){
// 			if(dp_display_connected == DP_PULL_IN){
// 				dp_status = 1;
// 			}else if(dp_display_connected == DP_PULL_OUT){
// 				dp_status = 0;
// 			}
// 		}

// 		if(default_display_connected != 2){
// 			if(default_display_connected != 0){
// 				default_status = 1;
// 			}else{
// 				default_status = 0;
// 			}
// 		}

// 		if(dp_status){
// 			kobject_uevent_env(It8912_kobj.parent, KOBJ_CHANGE, dp_hdmi_on);
// 			//printk("<3>""oncethings new dp_hdmi_on");
// 		}else{
// 			kobject_uevent_env(It8912_kobj.parent, KOBJ_CHANGE, dp_hdmi_off);
// 			//printk("<3>""oncethings new dp_hdmi_off");
// 		}

// 		if(default_status){
// 			kobject_uevent_env(It8912_kobj.parent, KOBJ_CHANGE, def_hdmi_on);
// 			//printk("<3>""oncethings new def_hdmi_on");
// 		}else{
// 			kobject_uevent_env(It8912_kobj.parent, KOBJ_CHANGE, def_hdmi_off);
// 			//printk("<3>""oncethings new def_hdmi_off");
// 		}

// 		msleep(1000);
// 	}
// 	return 0;
// }

static int lt8912_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct lt8912_private *data;
	int ret;
	//	pr_err("[kevin]%s-%d\n",__func__,__LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "lt8912 i2c check failed.\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct lt8912_private), GFP_KERNEL);
	if(!data)
	{
		return -ENOMEM;		
	}

	data->lt8912_client = client;
	data->mConnect = 0;

	It8912_kobj = client->dev.kobj;

	if (client->dev.of_node) {
		ret = lt8912_parse_dt(&client->dev, data);
		if (ret) {
			dev_err(&client->dev,
					"unable to parse device tree.(%d)\n", ret);
			goto err_parse_dt;
		}
	} else {
		dev_err(&client->dev, "device tree not found.\n");
		ret = -ENODEV;
		goto err_parse_dt;
	}

	dev_set_drvdata(&client->dev, data);

	lt8912_request_gpio(data);
	gpio_direction_output(data->hdmien_gpio, 1);
	mdelay(10);
	lt8912_power_set(data, true);

	data->regmap = devm_regmap_init_i2c(client, &lt8912_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev, "init regmap failed.(%ld)\n",
				PTR_ERR(data->regmap));
		ret = PTR_ERR(data->regmap);
		goto free_reset_gpio;
	}

	ret = lt8912_read_device_rev(data);
	if (ret < 0) {
		pr_err("%s: Failed to read chip rev\n", __func__);
		goto err_read_rev;
	}

	//printk("<3>""oncethings start kernel pthread");
	//hdmi_tast = kthread_run(report_hdmi_status,NULL,"hdmi_status");
	// if(!hdmi_tast){
	// 	pr_err(" report_hdmi_status run error");
	// }else{
	// 	pr_err(" report_hdmi_status run success");
	// }

#ifdef CONFIG_FB
	data->fb_notifier.notifier_call = lt8912_fb_notifier_cb;
	//ret = fb_register_client(&data->fb_notifier);
	ret = msm_drm_register_client(&data->fb_notifier);
	pr_debug("[kevin]lt8912_fb_notifier_cb %d\n",ret);
	if (ret < 0) {
		pr_err("[kevin]Failed to register lt8912_fb_notifier_cb client\n");
		goto err_fb_cb;
	}
#endif
#ifdef LT8912_HDP_IRQ
	lt8912_request_irq(data);
#endif
#ifdef LT8912_HDP_WORK
	INIT_DELAYED_WORK(&data->hpd_work, lt8912_hpd_work_fn);
	schedule_delayed_work(&data->hpd_work, msecs_to_jiffies(5000));
#endif
	lt8912_regist_switch(data);

	//lt8912_init(data);
	//g_reinit = 1;
	low_power_suspend_flag = 1;
	low_power_resume_flag = 1;
	pr_info("%s: lt8912 probe success\n", __func__);

	return 0;

	//err_init_work:
#ifdef LT8912_HDP_IRQ	
	disable_irq(data->hpd_irq);
	free_irq(data->hpd_irq, data);
#endif
#ifdef CONFIG_FB
	msm_drm_unregister_client(&data->fb_notifier);
#endif
err_fb_cb:
err_read_rev:
free_reset_gpio:
	lt8912_power_set(data, true);
err_parse_dt:
	devm_kfree(&client->dev, data);

	return ret;
}

static int lt8912_i2c_remove(struct i2c_client *client)
{
	struct lt8912_private *data = dev_get_drvdata(&client->dev);

	if (!data)
		return -1;

	switch_dev_unregister(&data->switch_dev);
#ifdef LT8912_HDP_IRQ
	disable_irq(data->hpd_irq);
	free_irq(data->hpd_irq, data);
#endif	
#ifdef CONFIG_FB
	msm_drm_unregister_client(&data->fb_notifier);
#endif
	lt8912_power_set(data, true);
	devm_kfree(&client->dev, data);

	return 0;
}

static const struct of_device_id of_lt8912_match[] = {
	{ .compatible = "lontium,lt8912"},
	{  },
};

static const struct i2c_device_id lt8912_id[] = {
	{"lt8912", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lt8912_id);


static struct i2c_driver lt8912_i2c_driver = {
	.driver = {
		.name = "lt8912",
		.owner = THIS_MODULE,
		.of_match_table = of_lt8912_match,
	},
	.probe = lt8912_i2c_probe,
	.remove = lt8912_i2c_remove,
	.id_table = lt8912_id,
};

static int __init lontium_i2c_test_init(void)
{
	return i2c_add_driver(&lt8912_i2c_driver);
}

static void __exit lontium_i2c_test_exit(void)
{
	i2c_del_driver(&lt8912_i2c_driver);
}

module_init(lontium_i2c_test_init);
module_exit(lontium_i2c_test_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("LT8912 Driver");
