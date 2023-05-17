#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#include <linux/timer.h>
#include <linux/string.h>

static int quec_gpio_probe(struct platform_device *pdev)
{

    int i, ret;
	enum of_gpio_flags flag;
	int gpio_array_size = 0;
	uint16_t *gpio_array = NULL;

	pr_err("-----------quectel_probe start--------------\n");

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "can not get quec_gpio node \n");
		return -EINVAL;
	}

	gpio_array_size = of_gpio_count(pdev->dev.of_node);
	dev_info(&pdev->dev, "gpio count %d\n", gpio_array_size);
	if (gpio_array_size <= 0)
		return -EINVAL;
	
	gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t), GFP_KERNEL);
	if (!gpio_array)
		goto free_gpio;
	
	for (i = 0; i < gpio_array_size; i++) {
		gpio_array[i] = of_get_gpio_flags(pdev->dev.of_node, i, &flag);
		dev_info(&pdev->dev, "gpio_array[%d] = %d\n", i, gpio_array[i]);
		if (gpio_is_valid(gpio_array[i])) {
			ret = gpio_request(gpio_array[i], "quec_gpios");
			if (ret) {
				dev_err(&pdev->dev, "Unable to request gpio%d \n", gpio_array[i]);
				goto free_gpio;
			}

			if(flag)
				gpio_direction_output(gpio_array[i], 0);
			else
				gpio_direction_input(gpio_array[i]);
			
			ret = gpio_export(gpio_array[i], 1);
			if (ret) {
				dev_err(&pdev->dev, "Unable to export gpio%d \n", gpio_array[i]);
				goto free_gpio;
			}
		} else {
			dev_err(&pdev->dev, "Invalid gpio%d!\n", gpio_array[i]);
			goto free_gpio;
		}
	}
	

	pr_err("-----------quectel_probe success--------------\n");
	return 0;
	
free_gpio:
kfree(gpio_array);
	
    return -EINVAL;
}


static int quec_gpio_remove(struct platform_device *pdev)
{

	return 0;
}



static const struct of_device_id quec_gpio_match[] = {
        { .compatible = "quec,gpios" },
        {},  
};


static struct platform_driver quec_gpio_driver = {
    .driver = {
            .name = "quec_gpio",
            .owner = THIS_MODULE,
            .of_match_table = quec_gpio_match,
    },   
    .probe = quec_gpio_probe,
    .remove = quec_gpio_remove,
};

module_platform_driver(quec_gpio_driver);

MODULE_AUTHOR("zefeng <zefeng.lin@quectel.com>");
MODULE_DESCRIPTION("quectel misc driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
