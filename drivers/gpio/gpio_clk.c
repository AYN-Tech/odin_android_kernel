#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/fb.h>

struct sc66_gpio_clk{
	struct device *dev;
	struct clk *pclk;
	struct clk *cbcr_clk;
	void __iomem *clk_base;
	unsigned long val;
	bool enable;
	int fg_detect_gpio;
	int fg_detect_irq;
	unsigned long fan_fg;
	struct notifier_block fb_notif;
};

//add by blithe for pwm bklt -->
static int sdm845_update_pwm(struct dsi_panel *panel, u32 bl_lvl)
{
	int rc = 0;
	u32 duty = 0;
	u32 period_ns = 0;
	struct dsi_backlight_config *bl;

	if (!panel) {
		pr_err("Invalid Params\n");
		return -EINVAL;
	}

	bl = &panel->bl_config;
	if (!bl->pwm_bl) {
		pr_err("pwm device not found\n");
		return -EINVAL;
	}

	period_ns = bl->pwm_period_usecs * NSEC_PER_USEC;
	duty = bl_lvl * period_ns;
	duty /= bl->bl_max_level;

	rc = pwm_config(bl->pwm_bl, duty, period_ns);
	if (rc) {
		pr_err("[%s] failed to change pwm config, rc=%d\n", panel->name,
			rc);
		goto error;
	}

	if (bl_lvl == 0 && bl->pwm_enabled) {
		pwm_disable(bl->pwm_bl);
		bl->pwm_enabled = false;
		return 0;
	}

	if (!bl->pwm_enabled) {
		rc = pwm_enable(bl->pwm_bl);
		if (rc) {
			pr_err("[%s] failed to enable pwm, rc=%d\n", panel->name,
				rc);
			goto error;
		}

		bl->pwm_enabled = true;
	}

error:
	return rc;
}
//<--

static ssize_t gpio_clock_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct sc66_gpio_clk *gpio40_clk = dev_get_drvdata(dev);
	
    return scnprintf(buf, PAGE_SIZE, "%d\n", gpio40_clk->enable);
}

static ssize_t gpio_clock_enable_store(struct device *dev,  struct device_attribute *attr, const char *buf, size_t size)
{ 
	int ret = 0;
	struct sc66_gpio_clk *gpio40_clk = dev_get_drvdata(dev);
	unsigned long clock_enable;
	
	ret = kstrtoul(buf, 10, &clock_enable);
	if (ret < 0){
		dev_err(dev, "%s:kstrtoul failed, ret=0x%x\n", __func__, ret);
		return ret;
	}
	if(clock_enable == 1 && gpio40_clk->enable == 0){
		pr_debug("enable clk!!!\n");
    		ret = clk_prepare_enable(gpio40_clk->pclk);
    		if(ret){
        		pr_err("%s:clk_prepare_enable pclk error!\n", __func__);
        		return -1;
    		}
		ret = clk_prepare_enable(gpio40_clk->cbcr_clk);
    		if(ret){
        		pr_err("%s:clk_prepare_enable cbcr_clk error!\n", __func__);
        		return -1;
    		}
		gpio40_clk->enable = 1;
	}else if(clock_enable == 0 && gpio40_clk->enable == 1){
		 pr_debug("disable clk!!!\n");
		 clk_disable_unprepare(gpio40_clk->cbcr_clk);
		 clk_disable_unprepare(gpio40_clk->pclk);
		 gpio40_clk->enable = 0;
	}else{
		pr_err("%s:IVALID VALUE!\n", __func__);
		return -1;
	}
	
    	return size;
}
static DEVICE_ATTR(gpio_clock_enable, S_IWUSR | S_IRUGO, gpio_clock_enable_show, gpio_clock_enable_store);

/*******************************
D = 24, duty-cycle=D/N= 24/48= 50%
N = 48 M =1
********************************/
static ssize_t clock_duty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sc66_gpio_clk *gpio40_clk = dev_get_drvdata(dev);

	gpio40_clk->val = readl_relaxed(gpio40_clk->clk_base + 0x3430);
	gpio40_clk->val = (255 - gpio40_clk->val)/2;
	
	return scnprintf(buf, PAGE_SIZE, "%lu\n", gpio40_clk->val);
}

static ssize_t clock_duty_store(struct device *dev,  struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	struct sc66_gpio_clk *gpio40_clk = dev_get_drvdata(dev);
	
    	ret = kstrtoul(buf, 10, &gpio40_clk->val);
	if (ret < 0){
		dev_err(dev, "%s:kstrtoul failed, ret=0x%x\n", __func__, ret);
		return ret;
	}
     if(gpio40_clk->val >= 48){
        gpio40_clk->val = 48 - 1;
     }
     if(gpio40_clk->val <= 0){
        gpio40_clk->val = 1;
     }
	writel_relaxed(255 - 2*gpio40_clk->val, gpio40_clk->clk_base + 0x3430);
   	writel_relaxed(0x03, gpio40_clk->clk_base + 0x3420);

	return size;
}
static DEVICE_ATTR(gpio_clock_duty, S_IWUSR | S_IRUGO, clock_duty_show, clock_duty_store);

static ssize_t fan_frq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sc66_gpio_clk *gpio40_clk = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%lums\n", gpio40_clk->fan_fg);
}

static DEVICE_ATTR(gpio_fan_frq,  S_IRUGO, fan_frq_show, NULL);

static int count = 0;
static irqreturn_t gpio_fg_detect_handler(int irq, void *data)
{
	struct sc66_gpio_clk *gpio40_clk = data;
	struct timeval fg_time;
	static unsigned long fg_time_sec_start;
	static unsigned long fg_time_sec_end;
	static unsigned long fg_time_usec_start;
	static unsigned long fg_time_usec_end;
	unsigned long fg_time_sec;
	unsigned long fg_time_usec;
	//pr_err("enter fg detect handle!!!\n");
	if(count == 0){
		do_gettimeofday(&fg_time);
		fg_time_sec_start = fg_time.tv_sec;
		fg_time_usec_start = fg_time.tv_usec;
		pr_debug("fg start time sec:%ld, usec: %ld\n", fg_time.tv_sec, fg_time.tv_usec);
	}

	count++;
	if(count == 21){
		do_gettimeofday(&fg_time);
		fg_time_sec_end = fg_time.tv_sec;
		fg_time_usec_end = fg_time.tv_usec;
		pr_debug("fg end time sec:%ld, usec: %ld\n", fg_time.tv_sec, fg_time.tv_usec);
		fg_time_sec = fg_time_sec_end - fg_time_sec_start;
		if(fg_time_usec_end > fg_time_usec_start)
			fg_time_usec = fg_time_usec_end - fg_time_usec_start;
		else{
			fg_time_usec = 1000000+fg_time_usec_end - fg_time_usec_start;
			fg_time_sec = fg_time_sec - 1;
		}
		gpio40_clk->fan_fg = (fg_time_sec *1000 + fg_time_usec/1000)/10;
		count = 0;
		fg_time_sec_start = 0;
		fg_time_sec_end = 0;
		fg_time_usec_start = 0;
		fg_time_usec_end = 0;
		pr_debug("fan fg time usec: %ldms\n", gpio40_clk->fan_fg);
	}

	return IRQ_HANDLED;
}

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	//int ret;
	struct sc66_gpio_clk *gpio40_clk =
		container_of(self, struct sc66_gpio_clk, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
			gpio40_clk) {
		blank = evdata->data;
		/*if (*blank == FB_BLANK_UNBLANK && gpio40_clk->enable == 0)
		{
			pr_debug("enable clk!!!\n");
    			ret = clk_prepare_enable(gpio40_clk->pclk);
    			if(ret){
        			pr_err("%s:clk_prepare_enable pclk error!\n", __func__);
        			return -1;
    			}
			ret = clk_prepare_enable(gpio40_clk->cbcr_clk);
    			if(ret){
        			pr_err("%s:clk_prepare_enable cbcr_clk error!\n", __func__);
        			return -1;
    			}
			gpio40_clk->enable = 1;
		}
		else if (*blank == FB_BLANK_POWERDOWN && gpio40_clk->enable == 1)
		{
			 pr_debug("disable clk!!!\n");
			 clk_disable_unprepare(gpio40_clk->cbcr_clk);
			 clk_disable_unprepare(gpio40_clk->pclk);

			 gpio40_clk->enable = 0;
		}*/
	}
	return 0;
}

static int sc66_clk_gpio_probe(struct platform_device *pdev)
{
       int ret;
	struct sc66_gpio_clk *gpio40_clk;

	pr_debug("sc66_clk_gpio_probe\n");
	gpio40_clk = devm_kzalloc(&pdev->dev, sizeof(*gpio40_clk), GFP_KERNEL);
	if (!gpio40_clk)
		return -ENOMEM;
	gpio40_clk->dev = &pdev->dev;

	if (of_find_property(gpio40_clk->dev->of_node, "fg-detect-gpio", NULL)) {
		gpio40_clk->fg_detect_gpio = of_get_named_gpio(gpio40_clk->dev->of_node, "fg-detect-gpio", 0);
		if (!gpio_is_valid(gpio40_clk->fg_detect_gpio)) {
			if (gpio40_clk->fg_detect_gpio != -EPROBE_DEFER)
				pr_err("failed to get fg detect gpio=%d\n", gpio40_clk->fg_detect_gpio);
		}

		ret = devm_gpio_request(gpio40_clk->dev, gpio40_clk->fg_detect_gpio, "dc-detect-gpio");
		if (ret) {
			pr_err("failed to request fg detect gpio ret=%d\n", ret);
		}
		ret = gpio_direction_input(gpio40_clk->fg_detect_gpio);
		if (ret) {
			pr_err("failed to set input status ret=%d\n", ret);
		}
		gpio40_clk->fg_detect_irq = gpio_to_irq(gpio40_clk->fg_detect_gpio);
		if (gpio40_clk->fg_detect_irq < 0) {		
			pr_err("gpio irq req failed!\n");	
		}		
		ret = devm_request_irq(gpio40_clk->dev, gpio40_clk->fg_detect_irq,
			      gpio_fg_detect_handler,
			      IRQF_TRIGGER_RISING |IRQF_ONESHOT,
			      "gpio_fg_det_irq", gpio40_clk);
		if (ret) {
			dev_err(gpio40_clk->dev, "request for gpio fg det irq failed: %d\n",ret);
		} 
		
		pr_info("Get fg detect gpio=%d, value= %d,irq=%d\n", gpio40_clk->fg_detect_gpio,
			gpio_get_value(gpio40_clk->fg_detect_gpio),gpio40_clk->fg_detect_irq);
	}

	gpio40_clk->pclk = devm_clk_get(&pdev->dev, "remoteir-pwm-clk0");
	if (IS_ERR_OR_NULL(gpio40_clk->pclk)) {
		dev_err(gpio40_clk->dev, "get remoteir-pwm-clk0 clock failed");
		return -EINVAL;
	}
	gpio40_clk->cbcr_clk = devm_clk_get(&pdev->dev, "remoteir-pwm-clk0-cbcr");
	if (IS_ERR_OR_NULL(gpio40_clk->cbcr_clk)) {
		dev_err(gpio40_clk->dev, "get remoteir-pwm-clk0-cbcr clock failed");
		return -EINVAL;
	}
	gpio40_clk->clk_base = ioremap(0xc8c0000,0x40000);

	ret = clk_set_rate(gpio40_clk->pclk , 25000);
	if (ret) {
		dev_err(gpio40_clk->dev, "pclk clk_set_rate failed");
		return -EINVAL;
	}
	/*
	ret = clk_prepare_enable(gpio40_clk->pclk);
	if (ret) {
		dev_err(gpio40_clk->dev, "pclk clk_prepare_enable failed");
		return -EINVAL;
	}
	ret = clk_prepare_enable(gpio40_clk->cbcr_clk);
	if (ret) {
		dev_err(gpio40_clk->dev, "cbcr_clk clk_prepare_enable failed");
		return -EINVAL;
	}
	*/
	gpio40_clk->enable = 0;

	gpio40_clk->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&gpio40_clk->fb_notif);
	if (ret)
		dev_err(gpio40_clk->dev, "Unable to register fb_notifier: %d\n", ret);
	
	ret = device_create_file(&pdev->dev, &dev_attr_gpio_clock_duty);
	if(ret){
		dev_err(gpio40_clk->dev,"failed to create device file gpio_clock_duty\n");
		return -EINVAL;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_gpio_clock_enable);
    	if(ret){
        	dev_err(&pdev->dev,"failed to create device file gpio_clock_enable\n");
		return -EINVAL;
    	}
		
	ret = device_create_file(&pdev->dev, &dev_attr_gpio_fan_frq);
    	if(ret){
        	dev_err(&pdev->dev,"failed to create device file gpio_fan_frq\n");
		return -EINVAL;
    	}	
	platform_set_drvdata(pdev, gpio40_clk);
	
	dev_info(gpio40_clk->dev, "gpio clk ended");

    return 0;
}

static int sc66_clk_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id sc66_clk_gpio_of_match[] = 
{
	{ .compatible = "remoteir-pwm", },
	{ },
};

static struct platform_driver sc66_clk_gpio_drv = {
	.probe      = sc66_clk_gpio_probe,
	.remove     = sc66_clk_gpio_remove,
	.driver     = {
		.name   = "remoteir-pwm",
		.owner  = THIS_MODULE,
		.of_match_table = sc66_clk_gpio_of_match,
	}
};

static int __init sc66_clk_gpio_init(void)
{
	return platform_driver_register(&sc66_clk_gpio_drv);
}

static void __exit sc66_clk_gpio_exit(void)
{
	platform_driver_unregister(&sc66_clk_gpio_drv);
}

module_init(sc66_clk_gpio_init);
module_exit(sc66_clk_gpio_exit);
MODULE_LICENSE("GPL v2");