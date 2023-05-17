#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>

#define SAMPLING_TIME 100

#define DP_HDMI_UNUSE 2
static int thermal_mode = 0;
int en_backlight = 1;
static int dp_state = 0;
static int hdmi_state = 0;
EXPORT_SYMBOL(en_backlight);
extern int dp_display_connected;
extern int default_display_connected;

struct gpio5_pwm_data
{
	unsigned int state;
	unsigned int duty_ns;
	unsigned int period_ns;
	unsigned int fan_en_gpio;
	unsigned int fan_int_gpio;
	int fan_irq;
	struct pwm_device *pwm_dev;
};

static struct gpio5_pwm_data *data;
static int fan_fg_count,rpm;
//static struct timeval fg_time;
//static unsigned long fg_time_sec_start;
//static unsigned long fg_time_sec_end;
//static unsigned long fg_time_usec_start;
//static unsigned long fg_time_usec_end;
//static unsigned long fg_time_sec;
//static unsigned long fg_time_usec;

static ssize_t state_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return (sprintf(buf, "%u\n", data->state));
}

static ssize_t state_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	if (sysfs_streq(buf, "1"))
	{
		pwm_config(data->pwm_dev, data->duty_ns, data->period_ns);
		printk("[kevin]%s: %d %d\n", __func__, data->duty_ns, data->period_ns);
		pwm_enable(data->pwm_dev);
		data->state = 1;
		gpio_direction_output(data->fan_en_gpio, 1);
		__gpio_set_value(data->fan_en_gpio,1);
		pr_err("kevin fan_en_gpio = 1!\n");
	}
	else if (sysfs_streq(buf, "0"))
	{
		pwm_disable(data->pwm_dev);
		data->state = 0;
		gpio_direction_output(data->fan_en_gpio, 0);
		__gpio_set_value(data->fan_en_gpio,0);
		pr_err("kevin fan_en_gpio = 0!\n");
	}

	return len;
}

static ssize_t duty_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return (sprintf(buf, "%u\n", data->duty_ns));
}

static ssize_t thermal_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return (sprintf(buf, "%u\n", thermal_mode));
}

static ssize_t en_backlight_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return (sprintf(buf, "%u\n", en_backlight));
}

static ssize_t dp_state_show(struct class *class, struct class_attribute *attr, char *buf)
{
	if(dp_display_connected == DP_HDMI_UNUSE)
		dp_state = 0;
	else
		dp_state = dp_display_connected;
	return (sprintf(buf, "%u\n", dp_state));
}

static ssize_t hdmi_state_show(struct class *class, struct class_attribute *attr, char *buf)
{
	if(default_display_connected == DP_HDMI_UNUSE)
		hdmi_state = 0;
	else
		hdmi_state = default_display_connected;
	return (sprintf(buf, "%u\n", hdmi_state));
}

static ssize_t duty_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	int rc;

	rc = kstrtouint(buf, 0, &data->duty_ns);
	if (rc < 0)
		pr_err("get duty value failed!\n");
	
	pwm_config(data->pwm_dev, data->duty_ns, data->period_ns);

	return len;
}

static ssize_t thermal_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	int rc;

	rc = kstrtouint(buf, 0, &thermal_mode);
	if (rc < 0)
		pr_err("get thermal_mode value failed!\n");

	return len;
}

static ssize_t en_backlight_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	int rc;

	rc = kstrtouint(buf, 0, &en_backlight);
	if (rc < 0)
		pr_err("get en_backlight value failed!\n");

	return len;
}

static ssize_t dp_state_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	int rc;
	int pre_dp_state = dp_state;
	rc = kstrtouint(buf, 0, &dp_state);
	if (rc < 0)
		pr_err("get en_backlight value failed!\n");
	if(dp_state != 0 || dp_state != 1){
		dp_state = pre_dp_state;
	}
	dp_display_connected = dp_state;
	return len;
}

static ssize_t hdmi_state_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	int rc;
	int pre_hdmi_state = hdmi_state;
	rc = kstrtouint(buf, 0, &hdmi_state);
	if (rc < 0)
		pr_err("get en_backlight value failed!\n");
	if(hdmi_state != 0 || hdmi_state != 1){
		hdmi_state = pre_hdmi_state;
	}
	default_display_connected = hdmi_state;
	return len;
}

static ssize_t period_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return (sprintf(buf, "%u\n", data->period_ns));
}

static ssize_t period_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	int rc;

	rc = kstrtouint(buf, 0, &data->period_ns);
	if (rc < 0)
		pr_err("get period value failed!\n");

	pwm_config(data->pwm_dev, data->duty_ns, data->period_ns);

	return len;
}

//cat sys/class/gpio5_pwm2/speed

static ssize_t speed_show(struct class *class, struct class_attribute *attr, char *buf)
{
	
	rpm = 0;
	fan_fg_count = 0;
	enable_irq(data->fan_irq);
	mdelay(SAMPLING_TIME);
	disable_irq(data->fan_irq);
	//printk("fan_fg_count = %d\n",fan_fg_count);
	rpm = 60000/(SAMPLING_TIME*2)*fan_fg_count;
	return (sprintf(buf, "%d\n", rpm));
}
static irqreturn_t fan_irq_handler(int irq, void *data)
{
	fan_fg_count = fan_fg_count+1;
    return IRQ_HANDLED;
}
#if 0
static irqreturn_t fan_irq_handler(int irq, void *data)
{
	//printk("******* %s ********start \n",__func__);
	if (fan_fg_count == 0){
		do_gettimeofday(&fg_time);
		fg_time_sec_start = fg_time.tv_sec;
		fg_time_usec_start = fg_time.tv_usec;
		//printk("******* fg start time sec:%ld, usec: %ld\n", fg_time.tv_sec, fg_time.tv_usec);
		//disable_irq_nosync(fan_irq);
	}
	fan_fg_count++;
	if (fan_fg_count == 3){
		do_gettimeofday(&fg_time);
		fg_time_sec_end = fg_time.tv_sec;
		fg_time_usec_end = fg_time.tv_usec;
		//printk("********** fg end time sec:%ld, usec: %ld\n", fg_time.tv_sec, fg_time.tv_usec);
		
		fg_time_sec = fg_time_sec_end - fg_time_sec_start;
		if(fg_time_usec_end > fg_time_usec_start)
			fg_time_usec = fg_time_usec_end - fg_time_usec_start;
		else{
			fg_time_usec = 1000000+fg_time_usec_end - fg_time_usec_start;
			fg_time_sec = fg_time_sec - 1;
		}
		rpm = 1000/((fg_time_sec *1000 + fg_time_usec/1000));//unit: 1sec/rpm
		//printk("********** rpm = %d \n",rpm);
		printk("count=%d,s_sec:%ld, s_usec: %ld,e_sec:%ld,e_usec:%ld\n",fan_fg_count, fg_time_sec_start, fg_time_usec_start,fg_time_sec_end,fg_time_usec_end);
	}
	//printk("******* %s ********end \n",__func__);
    return IRQ_HANDLED;
}
#endif

static int fan_pwm_suspend(struct device *dev)
{
	if (data->state) {
		pwm_disable(data->pwm_dev);
		// data->state = 0;
		gpio_direction_output(data->fan_en_gpio, 0);
		__gpio_set_value(data->fan_en_gpio,0);
		pr_err("kevin fan_en_gpio = 0!\n");
	}

	return 0;
}

static int fan_pwm_resume(struct device *dev)
{
	if (data->state) {
		pwm_config(data->pwm_dev, data->duty_ns, data->period_ns);
		printk("[kevin]%s: %d %d\n", __func__, data->duty_ns, data->period_ns);
		pwm_enable(data->pwm_dev);
		// data->state = 1;
		gpio_direction_output(data->fan_en_gpio, 1);
		__gpio_set_value(data->fan_en_gpio,1);
		pr_err("kevin fan_en_gpio = 1!\n");
	}

	return 0;
}

static int gpio5_pwm_probe(struct platform_device *pdev)
{
	int rc = 0;
	int ret = 0;
	struct device_node *of_node = pdev->dev.of_node;
	printk("%s start!\n",__func__);
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
	{
		return -ENOMEM;
	}

	data->pwm_dev = devm_of_pwm_get(&pdev->dev, of_node, NULL);
	if (!data->pwm_dev)
	{
		pr_err("%s: failed to request pwm\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	rc = of_property_read_s32(of_node, "gpio5-pwm-duty-ns", &data->duty_ns);
	if (rc)
	{
		pr_warn("%s: duty-ns is not defind\n", __func__);
	}

	rc = of_property_read_s32(of_node, "gpio5-pwm-period-ns", &data->period_ns);
	if (rc)
	{
		pr_warn("%s: gpio5-pwm-period-ns is not defind\n", __func__);
	}

	rc = of_property_read_s32(of_node, "gpio5-pwm-state", &data->state);
	if (rc)
	{
		pr_warn("%s: gpio5-pwm-state is not defind\n", __func__);
	}

	//rc = pwm_config(data->pwm_dev, data->duty_ns, data->period_ns);
	//if (rc)
	//{
	//	pr_err("%s: failed to change pwm config\n", __func__);
	//	goto error;
	//}

    data->fan_en_gpio = of_get_named_gpio(of_node, "qcom,fan-en-gpio", 0);
	if (data->fan_en_gpio < 0){
		pr_err("%s: get qcom,fan-en-gpio failed\n", __func__);
		goto error;
	}
	
	if (gpio_is_valid(data->fan_en_gpio)) {
		ret = gpio_request(data->fan_en_gpio, "quectel_red_led_en");
		if (ret) {
			pr_err("%s: Unable to request fan_en_gpio  [%d]\n", __func__,data->fan_en_gpio);
			goto error;
		}		
	} else {
		pr_err("%s: Invalid fan_en_gpio [%d]!\n", __func__,data->fan_en_gpio);
		ret = -EINVAL;
		goto err_free_red_led_en;
	}

	data->fan_int_gpio = of_get_named_gpio(of_node, "qcom,fan-fg-gpio", 0);
	if (data->fan_int_gpio < 0){
		pr_err("%s: get qcom,fan-int-gpio failed\n", __func__);
		goto error;
	}

    if (gpio_request(data->fan_int_gpio, "fan_fg_int_gpio") != 0) {
        gpio_free(data->fan_int_gpio);
        pr_err("%s: get data->fan_int_gpio failed\n", __func__);
        goto error;
    }

    gpio_direction_input(data->fan_int_gpio);
	data->fan_irq = gpio_to_irq(data->fan_int_gpio);

	rc = request_threaded_irq(data->fan_irq, NULL, 
			fan_irq_handler, 
			IRQF_TRIGGER_RISING |IRQF_ONESHOT, 
			"gpio5-pwm", data);
	if (rc)
	{
        pr_err("%s: request irq err = %d\n",__func__,rc);
		gpio_free(data->fan_int_gpio);
		goto error;
	}
	printk("%s: fan_irq= %d\n",__func__,data->fan_irq);


	//if (1 == data->state)
	//{
	//	rc = pwm_enable(data->pwm_dev);
	//	if (rc)
	//	{
	//		pr_err("%s: failed to enable pwm\n", __func__);
	//		goto error;
	//	}
	//}
	pwm_config(data->pwm_dev, 0, 50000);
	pwm_enable(data->pwm_dev);
	pwm_disable(data->pwm_dev);
	data->state = 0;
	gpio_direction_output(data->fan_en_gpio, 0);
	__gpio_set_value(data->fan_en_gpio,0);
	printk("%s end!\n",__func__);
	return 0;
err_free_red_led_en:
	if (gpio_is_valid(data->fan_en_gpio))
		gpio_free(data->fan_en_gpio);
error:
	kfree(data);
	return rc;

}

static int gpio5_pwm_remove(struct platform_device *pdev)
{
	if (data->state)
		pwm_disable(data->pwm_dev);

	kfree(data);

	return 0;
}

static struct class_attribute gpio_class_attrs[] =
{
	__ATTR(state, 0664, state_show, state_store),
	__ATTR(duty, 0664, duty_show, duty_store),
	__ATTR(period, 0664, period_show, period_store),
	__ATTR(speed, 0664, speed_show, NULL),
	__ATTR(thermal_mode, 0664, thermal_show, thermal_store),
	__ATTR(en_backlight, 0664, en_backlight_show, en_backlight_store),
	__ATTR(hdmi_state, 0664, hdmi_state_show, hdmi_state_store),
	__ATTR(dp_state, 0664, dp_state_show, dp_state_store),
	__ATTR_NULL,
};

static struct class gpio5_pwm_class =
{
	.name = "gpio5_pwm2",
	.owner = THIS_MODULE,
	.class_attrs = gpio_class_attrs,
};

static const struct of_device_id pwm_match_table[] =
{
	{ .compatible = "gpio5-pwm", },
	{},
};

#ifdef CONFIG_PM
static const struct dev_pm_ops fan_pwm_dev_pm_ops = {
	.suspend = fan_pwm_suspend,
	.resume = fan_pwm_resume,
};
#endif

static struct platform_driver gpio5_pwm_driver =
{
	.driver = {
		.name = "gpio5-pwm",
		.of_match_table = pwm_match_table,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &fan_pwm_dev_pm_ops,
#endif
	},
	.probe = gpio5_pwm_probe,
	.remove = gpio5_pwm_remove,
};

static int __init gpio5_pwm_init(void)
{
	int rc = 0;
	printk("[kevin]%s: %d\n", __func__,__LINE__);
	rc = class_register(&gpio5_pwm_class);
	if (0 == rc)
	{
		platform_driver_register(&gpio5_pwm_driver);
		printk("[kevin]%s: %d\n", __func__,__LINE__);
	}

	return rc;
}

static void __exit gpio5_pwm_exit(void)
{
	platform_driver_unregister(&gpio5_pwm_driver);
	class_unregister(&gpio5_pwm_class);
}

module_init(gpio5_pwm_init);
module_exit(gpio5_pwm_exit);

MODULE_LICENSE("GPL");

