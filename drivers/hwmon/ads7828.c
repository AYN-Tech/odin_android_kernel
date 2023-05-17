/*
 * ads7828.c - driver for TI ADS7828 8-channel A/D converter and compatibles
 * (C) 2007 EADS Astrium
 *
 * This driver is based on the lm75 and other lm_sensors/hwmon drivers
 *
 * Written by Steve Hardy <shardy@redhat.com>
 *
 * ADS7830 support, by Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 * For further information, see the Documentation/hwmon/ads7828 file.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_data/ads7828.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#ifdef CONFIG_FB
#include <linux/msm_drm_notify.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

/* The ADS7828 registers */
#define ADS7828_CMD_SD_SE	0x80	/* Single ended inputs */
#define ADS7828_CMD_PD1		0x04	/* Internal vref OFF && A/D ON */
#define ADS7828_CMD_PD3		0x0C	/* Internal vref ON && A/D ON */
#define ADS7828_INT_VREF_MV	2500	/* Internal vref is 2.5V, 2500mV */
#define ADS7828_EXT_VREF_MV_MIN	50	/* External vref min value 0.05V */
#define ADS7828_EXT_VREF_MV_MAX	5250	/* External vref max value 5.25V */

/* List of supported devices */
enum ads7828_chips { ads7828, ads7830 };

/* Client specific data */
struct ads7828_data {
	struct regmap *regmap;
	u8 cmd_byte;			/* Command byte without channel bits */
	unsigned int lsb_resol;		/* Resolution of the ADC sample LSB */

	struct workqueue_struct *ads7828_adc_workqueue;
	struct delayed_work ads7828_adc_work;
#ifdef CONFIG_FB
	struct notifier_block fb_notifier;
	bool fb_ready;
#endif
};

int result_buff[6];

/* Command byte C2,C1,C0 - see datasheet */
static inline u8 ads7828_cmd_byte(u8 cmd, int ch)
{
	return cmd | (((ch >> 1) | (ch & 0x01) << 2) << 4);
}

/* sysfs callback function */
static ssize_t ads7828_show_in(struct device *dev, struct device_attribute *da,
			       char *buf)
{
#if 0
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct ads7828_data *data = dev_get_drvdata(dev);
	u8 cmd = ads7828_cmd_byte(data->cmd_byte, attr->index);
	unsigned int regval;
	int err;

	err = regmap_read(data->regmap, cmd, &regval);
	if (err < 0)
		return err;

	return sprintf(buf, "%d\n",
		       DIV_ROUND_CLOSEST(regval * data->lsb_resol, 1000));
#else
	return snprintf(buf, sizeof(result_buff),
		"[zhb]L-KEY:%d L-1:%d L-2:%d R-key:%d R-3:%d R-4:%d\n",
		result_buff[0],result_buff[1],result_buff[2],result_buff[3],result_buff[4],result_buff[5]);
#endif
}



#if 0
static SENSOR_DEVICE_ATTR(in0_input, S_IRUGO, ads7828_show_in, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_input, S_IRUGO, ads7828_show_in, NULL, 1);
static SENSOR_DEVICE_ATTR(in2_input, S_IRUGO, ads7828_show_in, NULL, 2);
static SENSOR_DEVICE_ATTR(in3_input, S_IRUGO, ads7828_show_in, NULL, 3);
static SENSOR_DEVICE_ATTR(in4_input, S_IRUGO, ads7828_show_in, NULL, 4);
static SENSOR_DEVICE_ATTR(in5_input, S_IRUGO, ads7828_show_in, NULL, 5);
static SENSOR_DEVICE_ATTR(in6_input, S_IRUGO, ads7828_show_in, NULL, 6);
static SENSOR_DEVICE_ATTR(in7_input, S_IRUGO, ads7828_show_in, NULL, 7);
#else
static SENSOR_DEVICE_ATTR(all_input, S_IRUGO, ads7828_show_in, NULL, 0);
#endif

static struct attribute *ads7828_attrs[] = {
#if 0
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in7_input.dev_attr.attr,
#else
	&sensor_dev_attr_all_input.dev_attr.attr,
#endif
	NULL
};

ATTRIBUTE_GROUPS(ads7828);

static const struct regmap_config ads2828_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
};

static const struct regmap_config ads2830_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
int ads7828_is_suspend = 0;

struct ads7828_data *ayn_data;
 ssize_t ads7828_read(int channel, int *v)
{
	u8 cmd = 0;
	//unsigned int regval;
	int err;
	if(ayn_data == NULL){
		printk(KERN_ERR "Ayn [%s():%d]\n", __FUNCTION__, __LINE__);
		return -ENXIO;
	}

	if(ads7828_is_suspend)
		return -1;
	cmd = ads7828_cmd_byte(ayn_data->cmd_byte, channel);

	err = regmap_read(ayn_data->regmap, cmd, v);
	if (err < 0){
		//printk(KERN_ERR "Ayn [%s():%d]\n", __FUNCTION__, __LINE__);
		return err;
	}
	//pr_err("[kevin] %d ",*v);
	return 0 ;
	//return sprintf(buf, "%d\n", DIV_ROUND_CLOSEST(regval * ayn_data->lsb_resol, 1000));
}
 EXPORT_SYMBOL(ads7828_read);
#if 1
void ads7828_adc_work_fn(struct work_struct *work)
{
	int rc = -1;
	int i = 0;
	//unsigned int regval;
	//int result_buff[6];
	struct delayed_work *delayed_work = container_of(work, struct delayed_work, work);
	struct ads7828_data *data = container_of(delayed_work, struct ads7828_data, ads7828_adc_work);
	
	pr_err("adc work start\n");
	while(0/*!ads7828_is_suspend*/){
		for (i = 0; i < 6; i++){
			rc = regmap_read(data->regmap, ads7828_cmd_byte(data->cmd_byte, i), &result_buff[i]);
			if (rc < 0)
				pr_err("ads7828 regmap_read i = %d fail!\n",i);
			result_buff[i] = DIV_ROUND_CLOSEST(result_buff[i] * data->lsb_resol, 1000);
		}
		
		pr_err("[kevin]ADC0:%d ADC1:%d ADC2:%d ADC3:%d ADC4:%d ADC5:%d\n", 
		result_buff[0],result_buff[1],result_buff[2],result_buff[3],result_buff[4],result_buff[5]);
		msleep(1000);
	}
	pr_err("adc work exit\n");
}
#endif
#ifdef CONFIG_FB

static int ads7828_suspend(struct ads7828_data *data)
{
	pr_err("[zhb]ads7828_suspend +++\n");
	ads7828_is_suspend = 1;
	mdelay(1);
	flush_delayed_work(&data->ads7828_adc_work);
	cancel_delayed_work_sync(&data->ads7828_adc_work);
	//pr_err("ads7828_is_suspend=%d\n", ads7828_is_suspend);
	return 0;
}

static int ads7828_resume(struct ads7828_data *data)
{
	pr_err("[zhb]ads7828_resume +++\n");
	ads7828_is_suspend = 0;
	mdelay(1);
	queue_delayed_work(data->ads7828_adc_workqueue, &data->ads7828_adc_work, msecs_to_jiffies(1200));
	schedule_delayed_work(&data->ads7828_adc_work, msecs_to_jiffies(1200));
	//pr_err("ads7828_is_suspend=%d\n", ads7828_is_suspend);
	
	return 0;
}

static int ads7828_fb_notifier_cb(struct notifier_block *self,
		unsigned long event, void *data)
{
	int *transition;
	struct fb_event *evdata = data;
	struct ads7828_data *adc_data = container_of(self, struct ads7828_data, fb_notifier);
	//dump_stack();
	
	if (evdata && evdata->data && adc_data) {
		transition = evdata->data;
		if ((event == MSM_DRM_EARLY_EVENT_BLANK) && (adc_data->fb_ready == false)) {
			/* Resume */
			if (*transition == MSM_DRM_BLANK_UNBLANK) {
				ads7828_resume(adc_data);
				adc_data->fb_ready = true;
			}
		} else if ((event == MSM_DRM_EVENT_BLANK)&& (adc_data->fb_ready == true)) {
			/* Suspend */			
			if (*transition == MSM_DRM_BLANK_POWERDOWN) {
				ads7828_suspend(adc_data);
				adc_data->fb_ready = false;
			}	
		}
	}

	return 0;
}
#endif


static int ads7828_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ads7828_platform_data *pdata = dev_get_platdata(dev);
	struct ads7828_data *data;
	struct device *hwmon_dev;
	unsigned int vref_mv = ADS7828_INT_VREF_MV;
	bool diff_input = false;
	bool ext_vref = false;
	unsigned int regval;
	int ret;
	int rc,v;
	data = devm_kzalloc(dev, sizeof(struct ads7828_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (pdata) {
		diff_input = pdata->diff_input;
		ext_vref = pdata->ext_vref;
		if (ext_vref && pdata->vref_mv)
			vref_mv = pdata->vref_mv;
	}

	/* Bound Vref with min/max values */
	vref_mv = clamp_val(vref_mv, ADS7828_EXT_VREF_MV_MIN,
			    ADS7828_EXT_VREF_MV_MAX);

	/* ADS7828 uses 12-bit samples, while ADS7830 is 8-bit */
	if (id->driver_data == ads7828) {
		data->lsb_resol = DIV_ROUND_CLOSEST(vref_mv * 1000, 4096);
		data->regmap = devm_regmap_init_i2c(client,
						    &ads2828_regmap_config);
	} else {
		data->lsb_resol = DIV_ROUND_CLOSEST(vref_mv * 1000, 256);
		data->regmap = devm_regmap_init_i2c(client,
						    &ads2830_regmap_config);
	}

	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);

	data->cmd_byte = ext_vref ? ADS7828_CMD_PD1 : ADS7828_CMD_PD3;
	if (!diff_input)
		data->cmd_byte |= ADS7828_CMD_SD_SE;

	/*
	 * Datasheet specifies internal reference voltage is disabled by
	 * default. The internal reference voltage needs to be enabled and
	 * voltage needs to settle before getting valid ADC data. So perform a
	 * dummy read to enable the internal reference voltage.
	 */
	if (!ext_vref)
		regmap_read(data->regmap, data->cmd_byte, &regval);

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, client->name,
							   data,
							   ads7828_groups);
#ifdef CONFIG_FB
	//data->fb_ready = true;

	data->fb_notifier.notifier_call = ads7828_fb_notifier_cb;
	//ret = fb_register_client(&data->fb_notifier);
	ret = msm_drm_register_client(&data->fb_notifier);
	pr_debug("[zhb]ads7828_fb_notifier_cb %d\n",ret);
	if (ret < 0) {
		pr_err("[zhb]Failed to register ads7828_fb_notifier_cb client\n");
		return -1;
	}
#endif
	rc = ads7828_read(0,&v);
	pr_err("Ayn ads7828 mmm rc %d",rc);

	data->ads7828_adc_workqueue = create_singlethread_workqueue("adc workqueue");
	INIT_DELAYED_WORK(&data->ads7828_adc_work, ads7828_adc_work_fn);
	queue_delayed_work(data->ads7828_adc_workqueue, &data->ads7828_adc_work, msecs_to_jiffies(15000));
	ayn_data = data;
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static const struct i2c_device_id ads7828_device_ids[] = {
	{ "ads7828", ads7828 },
	{ "ads7830", ads7830 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ads7828_device_ids);

static struct i2c_driver ads7828_driver = {
	.driver = {
		.name = "ads7828",
	},

	.id_table = ads7828_device_ids,
	.probe = ads7828_probe,
};

module_i2c_driver(ads7828_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steve Hardy <shardy@redhat.com>");
MODULE_DESCRIPTION("Driver for TI ADS7828 A/D converter and compatibles");
