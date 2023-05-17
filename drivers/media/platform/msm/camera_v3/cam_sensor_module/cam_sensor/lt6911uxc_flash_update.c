#include "lt6911uxc_flash_update.h"

struct lt6911_data lt6911_data;
static int read_lt6911_byte(struct lt6911_data *lt6911, uint32_t addr, uint8_t *data)
{
    uint32_t value;
    int32_t rc;
    rc = camera_io_dev_read(&lt6911->s_ctrl->io_master_info, addr, &value, CAMERA_SENSOR_I2C_TYPE_BYTE, CAMERA_SENSOR_I2C_TYPE_BYTE);
    *data = value&0xff;
    CAM_DBG(CAM_SENSOR, "addr:0x%x data:0x%x", addr, *data);
    return rc;
}

static int HDMI_ReadI2C_ByteN(struct lt6911_data *lt6911, uint32_t addr, uint8_t *data, int32_t num_bytes)
{
    return camera_io_dev_read_seq(&lt6911->s_ctrl->io_master_info, addr, data, CAMERA_SENSOR_I2C_TYPE_BYTE, 
        CAMERA_SENSOR_I2C_TYPE_BYTE, num_bytes);
}

static int write_lt6911_byte(struct lt6911_data *lt6911, uint32_t addr, uint8_t data)
{
    lt6911->reg_setting.reg_addr = addr;
    lt6911->reg_setting.reg_data = data;
    CAM_DBG(CAM_SENSOR, "addr:0x%x data:0x%x", addr, data);
    return camera_io_dev_write(&lt6911->s_ctrl->io_master_info, &lt6911->write_setting);
}

static int HDMI_WriteI2C_ByteN(struct lt6911_data *lt6911, uint32_t addr, uint8_t* data, int32_t num_bytes)
{
    int i, ret;
    for(i=0; i<num_bytes; i++){
        ret = write_lt6911_byte(lt6911, addr, data[i]);
        if(ret)
            return ret;
    }
    return 0;
}

static int enabled_I2C_control(struct lt6911_data *lt6911, bool enable)
{
    if(write_lt6911_byte(lt6911, REG_CTRL, WRITE_I2C_CTRL) < 0)
        return -1;
    if(enable){
        write_lt6911_byte(lt6911, REG_I2C_CTRL, 0x01);
        return write_lt6911_byte(lt6911, REG_WATCHDOG, 0x00);
    }else{
        return write_lt6911_byte(lt6911, REG_I2C_CTRL, 0x00);
    }
}

static int write_control_reg(struct lt6911_data *lt6911, uint8_t data)
{
    if(enabled_I2C_control(lt6911, 1))
        return -1;
    return write_lt6911_byte(lt6911, REG_CTRL, data);
}

static int config(struct lt6911_data *lt6911)
{
    CAM_DBG(CAM_SENSOR, "Enter");
    if(enabled_I2C_control(lt6911, 1))
        return -1;

    write_lt6911_byte(lt6911, 0x5E, 0xC0 | 0x1F);
    write_lt6911_byte(lt6911, 0x59, 0x50);
    write_lt6911_byte(lt6911, 0x5A, 0x10);
    write_lt6911_byte(lt6911, 0x5A, 0x00);
    write_lt6911_byte(lt6911, 0x58, 0x21);
    return 0;
}

static int reset_fifo(struct lt6911_data *lt6911)
{
    uint8_t data;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(write_lt6911_byte(lt6911, REG_CTRL, READ_I2C_CTRL))
        return -1;

    read_lt6911_byte(lt6911, 0x08, &data);
    data &= 0xbF;
    write_lt6911_byte(lt6911, 0x08, data);
    data |= 0x40;
    write_lt6911_byte(lt6911, 0x08, data);
    return write_lt6911_byte(lt6911, REG_CTRL, WRITE_I2C_CTRL);
}

static int block_erase(struct lt6911_data *lt6911)
{
    uint32_t addr = 0;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(enabled_I2C_control(lt6911, 1))
        return -1;

    write_lt6911_byte(lt6911, 0x5A, 0x04);
    write_lt6911_byte(lt6911, 0x5A, 0x00);

    write_lt6911_byte(lt6911, 0x5B, (addr>>16) & 0xFF);
    write_lt6911_byte(lt6911, 0x5C, (addr>>8) & 0xFF);
    write_lt6911_byte(lt6911, 0x5D, addr & 0xFF);

    write_lt6911_byte(lt6911, 0x5A, 0x01);
    write_lt6911_byte(lt6911, 0x5A, 0x00);
    msleep(1000);
    return 0;
}

static int read(struct lt6911_data *lt6911, uint8_t* data, uint32_t length)
{
    int32_t i, count, size = LT6911_PAGE_SIZE;
    uint32_t addr = 0;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(enabled_I2C_control(lt6911, 1))
        return -1;

    write_lt6911_byte(lt6911, 0x5A, 0x84);
    write_lt6911_byte(lt6911, 0x5A, 0x80);

    count = (length+LT6911_PAGE_SIZE-1) / LT6911_PAGE_SIZE;
    for(i=0; i<count; i++){
        write_lt6911_byte(lt6911, 0x5E, 0x40 | LT6911_SPI_SIZE);
        write_lt6911_byte(lt6911, 0x5A, 0xA0);
        write_lt6911_byte(lt6911, 0x5A, 0x80);

        write_lt6911_byte(lt6911, 0x5B, (addr>>16) & 0xFF);
        write_lt6911_byte(lt6911, 0x5C, (addr>>8) & 0xFF);
        write_lt6911_byte(lt6911, 0x5D, addr & 0xFF);

        write_lt6911_byte(lt6911, 0x5A, 0x90);
        write_lt6911_byte(lt6911, 0x5A, 0x80);
        write_lt6911_byte(lt6911, 0x58, 0x21);

        if(count == (i+1))
            size = length - i*LT6911_PAGE_SIZE;
        HDMI_ReadI2C_ByteN(lt6911, 0x5F, data+addr, size);
        addr += size;
    }
    return 0;
}

static int write(struct lt6911_data *lt6911, uint8_t* data, uint32_t length)
{
    int32_t i, count, size = LT6911_PAGE_SIZE;
    uint32_t addr = 0;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(enabled_I2C_control(lt6911, 1))
        return -1;

    write_lt6911_byte(lt6911, 0x5A, 0x84);
    write_lt6911_byte(lt6911, 0x5A, 0x80);

    count = (length+LT6911_PAGE_SIZE-1) / LT6911_PAGE_SIZE;
    for(i=0; i<count; i++){
        write_lt6911_byte(lt6911, 0x5A, 0x84);
        write_lt6911_byte(lt6911, 0x5A, 0x80);
        write_lt6911_byte(lt6911, 0x5E, 0xC0 | LT6911_SPI_SIZE);
        write_lt6911_byte(lt6911, 0x5A, 0xA0);
        write_lt6911_byte(lt6911, 0x5A, 0x80);
        write_lt6911_byte(lt6911, 0x58, 0x21);

        if(count == (i+1))
            size = length - i*LT6911_PAGE_SIZE;
        HDMI_WriteI2C_ByteN(lt6911, 0x59, data+addr, size);

        write_lt6911_byte(lt6911, 0x5B, (addr>>16) & 0xFF);
        write_lt6911_byte(lt6911, 0x5C, (addr>>8) & 0xFF);
        write_lt6911_byte(lt6911, 0x5D, addr & 0xFF);

        write_lt6911_byte(lt6911, 0x5E, 0xC0);
        write_lt6911_byte(lt6911, 0x5A, 0x90);
        write_lt6911_byte(lt6911, 0x5A, 0x80);

        addr += size;
    }
    write_lt6911_byte(lt6911, 0x5A, 0x88);
    return write_lt6911_byte(lt6911, 0x5A, 0x80);
}

static int read_flash(struct lt6911_data *lt6911)
{
    lt6911->read_data = (uint8_t*)kzalloc(lt6911->data_size, GFP_KERNEL);
    if(!lt6911->read_data){
        CAM_INFO(CAM_SENSOR, "kzalloc read data failed");
        return -1;
    }
    memset(lt6911->read_data, 0, lt6911->data_size);
    reset_fifo(lt6911);
    return read(lt6911, lt6911->read_data, lt6911->data_size);
}

static int write_flash(struct lt6911_data *lt6911)
{
    if(!lt6911->write_data){
        CAM_INFO(CAM_SENSOR, "write data null");
        return -1;
    }
    reset_fifo(lt6911);
    return write(lt6911, lt6911->write_data, lt6911->data_size);
}

static int firware_upgrade(struct lt6911_data *lt6911)
{
    int i, rc=0;

    CAM_DBG(CAM_SENSOR, "Enter");
    config(lt6911);
    block_erase(lt6911);
    write_flash(lt6911);
    read_flash(lt6911);
    for(i=0; i<lt6911->data_size; i++){
        if(lt6911->write_data[i] != lt6911->read_data[i]){
            CAM_INFO(CAM_SENSOR, "failed [0x%x] write:0x%x read:0x%x", i, lt6911->write_data[i], lt6911->read_data[i]);
            rc = -1;
            break;
        }
    }
    kfree(lt6911->read_data);
    kfree(lt6911->write_data);
    lt6911->read_data = NULL;
    lt6911->write_data = NULL;
    return rc;
}

static int check_chip_id(struct lt6911_data *lt6911)
{
    uint32_t chip_id;
    uint8_t data;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(write_control_reg(lt6911, WRITE_CHIP_ID))
        return -1;

    read_lt6911_byte(lt6911, REG_CHIP_ID, &data);
    chip_id = data<<8;
    read_lt6911_byte(lt6911, REG_CHIP_ID+1, &data);
    chip_id = chip_id | data;
    if(chip_id != CHIP_ID){
        CAM_INFO(CAM_SENSOR, "read id:0x%x chip id:0x%x", chip_id, CHIP_ID);
        return -2;
    }
    CAM_DBG(CAM_SENSOR, "read id: 0x%x expected id 0x%x:", chip_id, CHIP_ID);
    return 0;
}

static int get_firmware_version(struct lt6911_data *lt6911, struct hdmi_info *info)
{
    uint8_t data;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(write_control_reg(lt6911, WRITE_FIRMWARE_VERSION))
        return -1;

    read_lt6911_byte(lt6911, REG_FIRMWARE_VERSION, &data);
    info->version = data<<24;
    read_lt6911_byte(lt6911, REG_FIRMWARE_VERSION+1, &data);
    info->version = info->version | data<<16;
    read_lt6911_byte(lt6911, REG_FIRMWARE_VERSION+2, &data);
    info->version = info->version | data<<8;
    read_lt6911_byte(lt6911, REG_FIRMWARE_VERSION+3, &data);
    info->version = info->version | data;
    CAM_DBG(CAM_SENSOR, "version:0x%x", info->version);
    return 0;
}

static int get_interrupt_event(struct lt6911_data *lt6911, struct hdmi_info *info)
{
    uint8_t data;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(write_control_reg(lt6911, WRITE_INT_EVENT))
        return -1;

    read_lt6911_byte(lt6911, REG_HDMI_INT_EVENT, &data);
    if(data == 0x88)
        info->hdmi_status = HDMI_SIGNAL_DISAPPEAR;
    else if(data == 0x55)
        info->hdmi_status = HDMI_SIGNAL_STABLE;
    else
        info->hdmi_status = UNKNOW_STATUS;

    read_lt6911_byte(lt6911, REG_AUDIO_INT_EVENT, &data);
    if(data == 0x88)
        info->audio_status = AUDIO_SIGNAL_DISAPPEAR;
    else if(data == 0x55)
        info->audio_status = AUDIO_SAMPLE_RATE_HIGHER;
    else if(data == 0xAA)
        info->audio_status = AUDIO_SAMPLE_RATE_LOWER;
    else
        info->audio_status = UNKNOW_STATUS;

    CAM_DBG(CAM_SENSOR, "HDMI:%d AUDIO:%d", info->hdmi_status, info->audio_status);
    return 0;
}

static int get_MIPI_resolution(struct lt6911_data *lt6911, struct hdmi_info *info)
{
    uint8_t data;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(write_control_reg(lt6911, WRITE_TOTAL))
        return -1;

    read_lt6911_byte(lt6911, REG_H_TOTAL, &data);
    info->hdmi_resolution.h_total = data<<8;
    read_lt6911_byte(lt6911, REG_H_TOTAL+1, &data);
    info->hdmi_resolution.h_total = info->hdmi_resolution.h_total | data;
    read_lt6911_byte(lt6911, REG_V_TOTAL, &data);
    info->hdmi_resolution.v_total = data<<8;
    read_lt6911_byte(lt6911, REG_V_TOTAL+1, &data);
    info->hdmi_resolution.v_total = info->hdmi_resolution.v_total | data;
    CAM_DBG(CAM_SENSOR, "mipi h_total:%d v_total:%d", info->hdmi_resolution.h_total, info->hdmi_resolution.v_total);

    if(write_control_reg(lt6911, WRITE_ACTIVE))
        return -1;

    read_lt6911_byte(lt6911, REG_H_ACTIVE, &data);
    info->hdmi_resolution.h_active = data<<8;
    read_lt6911_byte(lt6911, REG_H_ACTIVE+1, &data);
    info->hdmi_resolution.h_active = info->hdmi_resolution.h_active | data;
    read_lt6911_byte(lt6911, REG_V_ACTIVE, &data);
    info->hdmi_resolution.v_active = data<<8;
    read_lt6911_byte(lt6911, REG_V_ACTIVE+1, &data);
    info->hdmi_resolution.v_active = info->hdmi_resolution.v_active | data;
    CAM_DBG(CAM_SENSOR, "mipi h_active:%d v_active:%d", info->hdmi_resolution.h_active, info->hdmi_resolution.v_active);

    if(write_control_reg(lt6911, WRITE_MIPI_LANE))
        return -1;

    read_lt6911_byte(lt6911, REG_MIPI_LANE, &data);
    switch(data){
        case 8 : info->hdmi_resolution.lane = MIPI_4LANE_2PORT;
        case 4 : info->hdmi_resolution.lane = MIPI_4LANE_PORT;
        case 2 : info->hdmi_resolution.lane = MIPI_2LANE_PORT;
        case 1 : info->hdmi_resolution.lane = MIPI_1LANE_PORT;
        default : info->hdmi_resolution.lane = UNKNOW_LANE;
    }
    CAM_DBG(CAM_SENSOR, "mipi lane:%d", info->hdmi_resolution.lane);

    if(write_control_reg(lt6911, WRITE_HALF_PIXEL_CLOCK))
        return -1;
    write_lt6911_byte(lt6911, WRITE_SELECT_AD_HALF_PIX_CLK, 0x21);
    msleep(10);
    read_lt6911_byte(lt6911, REG_HALF_PIXEL_CLOCK, &data);
    info->hdmi_resolution.pclk = (data&0xF)<<16;
    read_lt6911_byte(lt6911, REG_HALF_PIXEL_CLOCK+1, &data);
    info->hdmi_resolution.pclk = info->hdmi_resolution.pclk | (data<<8);
    read_lt6911_byte(lt6911, REG_HALF_PIXEL_CLOCK+1, &data);
    info->hdmi_resolution.pclk = info->hdmi_resolution.pclk | data;
    CAM_DBG(CAM_SENSOR, "mipi pclk:%d", info->hdmi_resolution.pclk);

    if(write_control_reg(lt6911, WRITE_BYTE_CLOCK))
        return -1;
    write_lt6911_byte(lt6911, WRITE_SELECT_AD_LMTX_WRITE_CLK, 0x1B);
    msleep(10);
    read_lt6911_byte(lt6911, REG_BYTE_CLOCK, &data);
    info->hdmi_resolution.byte_clk = (data&0xF)<<16;
    read_lt6911_byte(lt6911, REG_BYTE_CLOCK+1, &data);
    info->hdmi_resolution.byte_clk = info->hdmi_resolution.byte_clk | (data<<8);
    read_lt6911_byte(lt6911, REG_BYTE_CLOCK+1, &data);
    info->hdmi_resolution.byte_clk = info->hdmi_resolution.byte_clk | data;
    CAM_DBG(CAM_SENSOR, "mipi byte_clk:%d", info->hdmi_resolution.byte_clk);

    if(write_control_reg(lt6911, WRITE_AUDIO_SAMPLE_RATE))
        return -1;

    read_lt6911_byte(lt6911, REG_AUDIO_SAMPLE_RATE, &data);
    info->hdmi_resolution.audio_rate = data;
    CAM_DBG(CAM_SENSOR, "mipi audio_rate:%d", info->hdmi_resolution.audio_rate);
    return 0;
}

static int enabled_mipi_stream(struct lt6911_data *lt6911)
{
    CAM_DBG(CAM_SENSOR, "Enter");
    if(write_lt6911_byte(lt6911, REG_CTRL, WRITE_MIPI_OUTPUT) < 0)
        return -1;

    return write_lt6911_byte(lt6911, REG_MIPI_OUTPUT, 0xFB);
}

static int get_hdmi_info(struct lt6911_data *lt6911, struct hdmi_info *info)
{
    CAM_DBG(CAM_SENSOR, "Enter");
    get_firmware_version(lt6911, info);
    return get_MIPI_resolution(lt6911, info);
}

static int get_chip_status(struct lt6911_data *lt6911)
{
    int count, rc = 0;

    CAM_DBG(CAM_SENSOR, "Enter");
    msleep(2000);
    gpio_direction_input(lt6911->int_gpio);
    for(count=0; count<4; count++){
        rc = check_chip_id(lt6911);
        if(rc == -1)
            return -1;
        if(!rc || !gpio_get_value(lt6911->int_gpio)){
            CAM_DBG(CAM_SENSOR, "INT gpio:%d count:%d", lt6911->int_gpio, count);
            return 0;
        }
        else{
            msleep(50);
        }
    }
    return -1;
}

static void enabled_hdmi_power(struct lt6911_data *lt6911, bool enable)
{
    CAM_DBG(CAM_SENSOR, "Enter");
    if(enable == lt6911->s_ctrl->sensordata->power_info.cam_pinctrl_status)
        return;

    if(enable)
        cam_sensor_power_up(lt6911->s_ctrl);
    else
        cam_sensor_power_down(lt6911->s_ctrl);
}

static void enabled_hdmi_reset(struct lt6911_data *lt6911)
{
    CAM_DBG(CAM_SENSOR, "Enter");
    enabled_hdmi_power(lt6911, 0);
    msleep(10);
    enabled_hdmi_power(lt6911, 1);
}

static int get_lt6911_status(struct lt6911_data *lt6911, int stream_on)
{
    int rc=0;

    rc = get_chip_status(lt6911);
    if(rc){
        CAM_INFO(CAM_SENSOR, "get_chip_status failed");
        return -1;
    }

    rc = check_chip_id(lt6911);
    if(rc){
        CAM_INFO(CAM_SENSOR, "check_chip_id failed");
    }
    if(stream_on){
        enabled_mipi_stream(lt6911);
        enabled_I2C_control(lt6911, 0);
    }
    return rc;
}

static int read_firware_bin(struct lt6911_data *lt6911, const char *filename)
{
    struct file* fp;
    mm_segment_t old_fs;
    loff_t pos;
    char path_name[100]={0};
    int len;

    CAM_DBG(CAM_SENSOR, "Enter");
    len = strlen(filename);
    if(len >= 100){
        CAM_INFO(CAM_SENSOR, "file path name too long");
        return -1;
    }

    if(filename[len-1] == 10)
        memcpy(path_name, filename, len-1);
    else
        memcpy(path_name, filename, len);
    CAM_DBG(CAM_SENSOR, "open file name %s", path_name);
    fp = filp_open(path_name, O_RDONLY, 0);
    if(IS_ERR(fp)){
        CAM_INFO(CAM_SENSOR, "open file %s failed", path_name);
        return -1;
    }

    lt6911->write_data = (uint8_t*)kzalloc(LT6911_MAX_FLASH_SIZE, GFP_KERNEL);
    if(!lt6911->write_data){
        CAM_INFO(CAM_SENSOR, "kzalloc write data failed");
        goto Exit;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
    lt6911->data_size = vfs_read(fp, lt6911->write_data, LT6911_MAX_FLASH_SIZE, &pos);
    if(lt6911->data_size < 0){
        CAM_INFO(CAM_SENSOR, "vfs_read failed rc:%d", lt6911->data_size);
        kfree(lt6911->write_data);
        lt6911->write_data = NULL;
    }else{
        CAM_DBG(CAM_SENSOR, "read data length 0x%x", lt6911->data_size);
    }
    set_fs(old_fs);

Exit:
    filp_close(fp, NULL);
    return lt6911->data_size;
}

static void lt6911_irq_enable(struct lt6911_data *lt6911)
{
    unsigned long spin_lock_flags;

    spin_lock_irqsave(&lt6911->irq_lock, spin_lock_flags);
    if(lt6911->irq_status) {
        enable_irq(lt6911->irq);
        lt6911->irq_status = 0;
    }
    spin_unlock_irqrestore(&lt6911->irq_lock, spin_lock_flags);
}

static void lt6911_irq_disable(struct lt6911_data *lt6911)
{
    unsigned long spin_lock_flags;

    spin_lock_irqsave(&lt6911->irq_lock, spin_lock_flags);
    if (!lt6911->irq_status) {
        lt6911->irq_status = 1;
        disable_irq_nosync(lt6911->irq);
    }
    spin_unlock_irqrestore(&lt6911->irq_lock, spin_lock_flags);
}

static int lt6911_hdmi_info(struct lt6911_data *lt6911, char *envp_ext[])
{    
    int rc=0, length = 0;
    struct hdmi_info info;
    static char buf[MAX_BUF_SIZE];

    CAM_DBG(CAM_SENSOR, "Enter");
    rc = get_lt6911_status(lt6911, 0);
    if(rc){
        CAM_INFO(CAM_SENSOR, "get_lt6911_status failed");
        goto Exit;
    }
    
    rc = get_interrupt_event(lt6911, &info);
    if(rc){
        CAM_INFO(CAM_SENSOR, "get_hdmi_info failed");
        goto Exit;
    }

    if(info.hdmi_status != lt6911->hdmi_info.hdmi_status){
        switch(info.hdmi_status){
            case HDMI_SIGNAL_STABLE :
                length += snprintf(&buf[length], MAX_BUF_SIZE-length, "HDMI_STATE=%s", "CONNECTED"); break;
            case HDMI_SIGNAL_DISAPPEAR :
                length += snprintf(&buf[length], MAX_BUF_SIZE-length, "HDMI_STATE=%s", "DISCONNECTED"); break;
            default:break;
        }
        switch(info.audio_status){
            case AUDIO_SIGNAL_DISAPPEAR :
                length += snprintf(&buf[length], MAX_BUF_SIZE-length, ", AUDIO_STATE=%s", "DISAPPEAR"); break;
            case AUDIO_SAMPLE_RATE_HIGHER :
                length += snprintf(&buf[length], MAX_BUF_SIZE-length, ", AUDIO_STATE=%s", "SAMPLE_RATE_HIGHER"); break;
            case AUDIO_SAMPLE_RATE_LOWER:
                length += snprintf(&buf[length], MAX_BUF_SIZE-length, ", AUDIO_STATE=%s", "SAMPLE_RATE_LOWER"); break;
            default:break;
        }
    }
    if(length){
        rc = get_hdmi_info(lt6911, &info);
        if(rc){
            CAM_INFO(CAM_SENSOR, "get_hdmi_info failed\n");
            goto Exit;
        }

        if(info.hdmi_status == HDMI_SIGNAL_STABLE){
             length += snprintf(&buf[length], MAX_BUF_SIZE-length, ", HDMI_SIZE=%dx%d", info.hdmi_resolution.h_active, info.hdmi_resolution.v_active);
             length += snprintf(&buf[length], MAX_BUF_SIZE-length, ", AUDIO_RATE=%d", info.hdmi_resolution.audio_rate);
        }

        buf[MAX_BUF_SIZE-1] = '\0';
        lt6911->hdmi_info = info;
        envp_ext[0] = buf;
    }

Exit:
    enabled_I2C_control(lt6911, 0);
    CAM_DBG(CAM_SENSOR, "%s\n", rc? "failed":"succeed");
    return rc == 0 ? length : rc;
}

static void lt6911_schedule_work(struct work_struct *work)
{
    char *envp_ext[2] = {0, 0};
    struct lt6911_data *lt6911 = container_of(work, struct lt6911_data, lt6911_work);

    CAM_DBG(CAM_SENSOR, "Enter");
    if(lt6911_hdmi_info(lt6911, envp_ext) > 0){
        CAM_INFO(CAM_SENSOR, "%s", envp_ext[0]);
        kobject_uevent_env(&lt6911->s_ctrl->pdev->dev.kobj, KOBJ_CHANGE, envp_ext);
    }
    lt6911_irq_enable(lt6911);
}

irqreturn_t lt6911_irq_handler(int irq, void *data)
{
    struct lt6911_data *lt6911 = (struct lt6911_data *)data;

    if(lt6911->power_status == HDMI_POWRE_UP){
        lt6911_irq_disable(lt6911);
        schedule_work(&lt6911->lt6911_work);
    }
    return IRQ_HANDLED;
}

static int start_updata(struct lt6911_data *lt6911, const char *buf)
{
    int rc=0;

    CAM_DBG(CAM_SENSOR, "Enter");
    if(read_firware_bin(lt6911, buf)<=0){
        CAM_INFO(CAM_SENSOR, "read_firware_bin failed");
        return -1;
    }

    rc = get_lt6911_status(lt6911, 0);
    if(rc){
        CAM_INFO(CAM_SENSOR, "get_lt6911_status failed");
        goto Exit;
    }

    rc = firware_upgrade(lt6911);
    if(rc){
        CAM_INFO(CAM_SENSOR, "firware_upgrade failed");
    }

Exit:
    enabled_I2C_control(lt6911, 0);
    CAM_INFO(CAM_SENSOR, " %s", rc? "failed":"succeed");
    return rc;
}

ssize_t lt6911uxc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct lt6911_data *lt6911 = &lt6911_data;
    if(lt6911->is_probe){
        enabled_hdmi_reset(lt6911);
        start_updata(lt6911, buf);
        enabled_hdmi_power(lt6911, 0);
    }
    return count;
}
DEVICE_ATTR(updata, 0220, NULL, lt6911uxc_store);


struct cam_sensor_i2c_reg_array  lt6911_enable_i2c_reg[] = {
    {0xff, 0x80,0xff},
    {0xee, 0x01,0xff},
    {0x10, 0x00,0xff},
    {0xff, 0x81,0xff},
};

struct cam_sensor_i2c_reg_array  lt6911_disable_i2c_reg[] = {
    {0xee, 0x00,0xff},
};

struct cam_sensor_i2c_reg_setting  lt6911_check_id_settings =
{
    .reg_setting = lt6911_enable_i2c_reg,
    .size = 4,
    .addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
    .data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
    .delay =1,
};

int lt6911_check_chip_id(struct cam_sensor_ctrl_t *s_ctrl)
{
    int rc = 0;
    int value = 0;
    uint32_t chipid = 0;
    rc =  camera_io_dev_write(&(s_ctrl->io_master_info), &lt6911_check_id_settings);
    if(rc < 0) {
        CAM_ERR(CAM_SENSOR, "write check_id setting fail,rc %d",rc);
        return rc;
    }

    rc = camera_io_dev_read(&(s_ctrl->io_master_info), 0x00, &value, CAMERA_SENSOR_I2C_TYPE_BYTE, CAMERA_SENSOR_I2C_TYPE_BYTE);
    chipid = value << 8;

    rc = camera_io_dev_read(&(s_ctrl->io_master_info), 0x01, &value, CAMERA_SENSOR_I2C_TYPE_BYTE, CAMERA_SENSOR_I2C_TYPE_BYTE);
    chipid = chipid | value;

    CAM_ERR(CAM_SENSOR, "read id: 0x%x",chipid);
    if(chipid != CHIP_ID){
        CAM_ERR(CAM_SENSOR, "read id:0x%x chip id:0x%x", chipid, CHIP_ID);
        return -ENODEV;
    }
    return rc;
}

int init_lt6911uxc(struct cam_sensor_ctrl_t *s_ctrl, int status)
{
    int rc = 0;
    struct lt6911_data *lt6911 = &lt6911_data;

    CAM_DBG(CAM_SENSOR, "Enter");
    switch(status){
        case 0 :
            if(lt6911->power_status == HDMI_POWRE_UP && !lt6911->is_probe)
                break;
            free_irq(lt6911->irq, lt6911);
            lt6911->power_status = HDMI_POWRE_DOWN;
            break;
        case 1 :
            if(lt6911->power_status == HDMI_POWRE_DOWN && !lt6911->is_probe)
                break;
            rc = get_lt6911_status(lt6911, 1);
            if(rc){
                CAM_INFO(CAM_SENSOR, "get_lt6911_status failed");
                //return rc;
            }
            lt6911->irq = gpio_to_irq(lt6911->int_gpio);
            lt6911->irq_status = 0;
            memset(&lt6911->hdmi_info, 0, sizeof(struct hdmi_info));
            rc = request_irq(lt6911->irq, lt6911_irq_handler, IRQF_TRIGGER_FALLING, "lt6911_irq", lt6911);
            if(rc == -EBUSY){
                pr_err("%s:%d lt6911 irq is busy, free irq and try again, rc: %d", __func__, __LINE__, rc);
                free_irq(lt6911->irq, lt6911);
                rc = request_irq(lt6911->irq, lt6911_irq_handler, IRQF_TRIGGER_FALLING, "lt6911_irq", lt6911);
                if(rc){
                    pr_err("%s:%d try request lt6911 irq again failed, rc: %d", __func__, __LINE__, rc);
                    return rc;
                }
                pr_err("%s:%d try request lt6911 irq again succeed", __func__, __LINE__);
            } else if(rc){
                pr_err("%s:%d set request lt6911 irq failed, rc: %d", __func__, __LINE__, rc);
                return rc;
            }

            lt6911_irq_enable(lt6911);
            lt6911->power_status = HDMI_POWRE_UP;
            break;
        default :
            if(!lt6911->is_probe){
            lt6911->int_gpio = s_ctrl->sensordata->power_info.gpio_num_info->gpio_num[SENSOR_STANDBY];
            if(!gpio_is_valid(lt6911->int_gpio)){
                CAM_INFO(CAM_SENSOR, "invalid INT GPIO %d", lt6911->int_gpio);
                return -1;
            }
            CAM_DBG(CAM_SENSOR, "INT gpio:%d", lt6911->int_gpio);
            rc = device_create_file(&s_ctrl->pdev->dev, &dev_attr_updata);
            if(rc){
                CAM_INFO(CAM_SENSOR, "device create file failed");
                return rc;
            }
            lt6911->write_setting.reg_setting = &lt6911->reg_setting;
            lt6911->write_setting.size = 1;
            lt6911->write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
            lt6911->write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
            lt6911->write_setting.delay = 0;
            lt6911->s_ctrl = s_ctrl;
            rc = get_lt6911_status(lt6911, 0);
            if(rc){
                CAM_INFO(CAM_SENSOR, "get_lt6911_status failed");
                device_remove_file(&s_ctrl->pdev->dev, &dev_attr_updata);
                //return rc;
            }
            spin_lock_init(&lt6911->irq_lock);
            INIT_WORK(&lt6911->lt6911_work, lt6911_schedule_work);
            lt6911->is_probe = 1;
            lt6911->power_status = HDMI_POWRE_DOWN;
            }
            break;
    }

    return 0;
}
