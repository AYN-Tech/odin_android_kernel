#ifndef __LT6911UXC_FLASH_UPDATA_H__
#define __LT6911UXC_FLASH_UPDATA_H__
#include <linux/delay.h>
#include "cam_sensor_core.h"

#define FIRMWARE_PATCH "/sdcard"

#define WRITE_SELECT_AD_HALF_PIX_CLK 0x40
#define WRITE_SELECT_AD_LMTX_WRITE_CLK 0x40

#define REG_CTRL 0xFF

#define WRITE_CHIP_ID 0x81
#define REG_CHIP_ID 0x00

#define WRITE_FIRMWARE_VERSION 0x86
#define REG_FIRMWARE_VERSION 0xA7

#define WRITE_INT_EVENT 0x86
#define REG_HDMI_INT_EVENT 0xA3
#define REG_AUDIO_INT_EVENT 0xA5

#define WRITE_TOTAL 0xD4
#define REG_H_TOTAL 0x26
#define REG_V_TOTAL 0x32

#define WRITE_ACTIVE 0x85
#define REG_H_ACTIVE 0xEA
#define REG_V_ACTIVE 0xF0

#define WRITE_MIPI_LANE 0x86
#define REG_MIPI_LANE 0xA2

#define WRITE_HALF_PIXEL_CLOCK 0x85
#define REG_HALF_PIXEL_CLOCK 0x48

#define WRITE_BYTE_CLOCK 0x85
#define REG_BYTE_CLOCK 0x48

#define WRITE_AUDIO_SAMPLE_RATE 0xB0
#define REG_AUDIO_SAMPLE_RATE 0xAB

#define WRITE_MIPI_OUTPUT 0x81
#define REG_MIPI_OUTPUT 0x1D

#define WRITE_I2C_CTRL 0x80
#define READ_I2C_CTRL 0x81
#define REG_I2C_CTRL 0xEE

#define REG_WATCHDOG 0x10

#define CHIP_ID 0x1704

#define LT6911_MAX_FLASH_SIZE 1024*40
#define LT6911_BLOCK_SIZE 256
#define LT6911_PAGE_SIZE 32
#define LT6911_SPI_SIZE (LT6911_PAGE_SIZE - 1)
#define MAX_BUF_SIZE 256

enum HDMI_STATUS{
	UNKNOW_STATUS,
	HDMI_POWRE_UP,
	HDMI_POWRE_DOWN,
	HDMI_SIGNAL_DISAPPEAR,
	HDMI_SIGNAL_STABLE,
	AUDIO_SIGNAL_DISAPPEAR,
	AUDIO_SAMPLE_RATE_HIGHER,
	AUDIO_SAMPLE_RATE_LOWER,
};

enum HDMI_LANE{
	UNKNOW_LANE,
	MIPI_4LANE_2PORT,
	MIPI_4LANE_PORT,
	MIPI_2LANE_PORT,
	MIPI_1LANE_PORT,
};

struct hdmi_resolution{
    uint32_t pclk;
	uint32_t byte_clk;
    uint32_t audio_rate;
    enum HDMI_LANE lane;
    uint16_t h_total;
    uint16_t v_total;
    uint16_t h_active;
    uint16_t v_active;
};

struct hdmi_info{
	uint32_t version;
	struct hdmi_resolution hdmi_resolution;
	enum HDMI_STATUS hdmi_status;
	enum HDMI_STATUS audio_status;
};

struct lt6911_data{
	struct cam_sensor_ctrl_t *s_ctrl;
	struct cam_sensor_i2c_reg_setting write_setting;
	struct cam_sensor_i2c_reg_array reg_setting;
	struct work_struct lt6911_work;
	struct hdmi_info hdmi_info;
	enum HDMI_STATUS power_status;
	spinlock_t irq_lock;
	uint8_t* write_data;
	uint8_t* read_data;
	int32_t data_size;
	int32_t is_probe;
	int32_t int_gpio;
	int32_t irq;
	int32_t irq_status;
};
#endif