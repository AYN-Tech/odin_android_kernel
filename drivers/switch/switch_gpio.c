/*
 *  drivers/switch/switch_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

#include <linux/of_gpio.h>

#define QUECTEL_CHANGE 1
#define FUNC(f,args...)         printk("[kevin][FUNC = %s][LINE=%d]\n"f,__FUNCTION__,__LINE__,##args)
#define DEBUG(f,args...)        printk("[kevin][DEBUG = %s][LINE=%d]\n"f,__FUNCTION__,__LINE__,##args)
#define ERROR(f,args...)        printk("[kevin][ERROR = %s][LINE=%d]\n"f,__FUNCTION__,__LINE__,##args)
#define PINCTRL_STATE_INT_DEFAULT          "pmx_switch_int_default"
#define PINCTRL_STATE_INT_OUT_HIGHT        "pmx_switch_int_out_Hight"
#define PINCTRL_STATE_INT_INPUT            "pmx_switch_int_input"

struct gpio_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct work_struct work;
};

static void gpio_switch_work(struct work_struct *work)
{
	int state;
	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, work);

	state = gpio_get_value(data->gpio);
	#if QUECTEL_CHANGE
	state = !state;
	FUNC("[kevin]state = %d\n",state);
	#endif
	switch_set_state(&data->sdev, state);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
	    (struct gpio_switch_data *)dev_id;

	schedule_work(&switch_data->work);
	return IRQ_HANDLED;
}

static ssize_t switch_gpio_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}
#if QUECTEL_CHANGE
struct switch_pinctrl {
	struct pinctrl *pinctrl;
	struct pinctrl_state *default_sta;
	struct pinctrl_state *int_out_high;
	struct pinctrl_state *int_input;
};
static struct switch_pinctrl *pinctrl;
static int switch_pinctrl_init(struct platform_device *pdev)
{
	if(&pdev->dev == NULL){
		ERROR();
		return ENOMEM;
	}

	pinctrl = devm_kzalloc(&pdev->dev, 
		sizeof(struct switch_pinctrl), GFP_KERNEL);
	if(!pinctrl) {
		ERROR();
		return -ENOMEM;
	}

	
	pinctrl->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		ERROR();
		pinctrl->pinctrl = NULL;
		return ENOMEM;
	}

	pinctrl->default_sta = pinctrl_lookup_state(pinctrl->pinctrl,
						    PINCTRL_STATE_INT_DEFAULT);
	if (IS_ERR_OR_NULL(pinctrl->default_sta)) {
		ERROR();
		goto exit_pinctrl_init;
	} else {
		FUNC();
	}

	pinctrl->int_out_high = pinctrl_lookup_state(pinctrl->pinctrl,
						     PINCTRL_STATE_INT_OUT_HIGHT);
	if (IS_ERR_OR_NULL(pinctrl->int_out_high)) {
		ERROR();
		goto exit_pinctrl_init;
	} else {
		FUNC();
	}

	pinctrl->int_input = pinctrl_lookup_state(pinctrl->pinctrl,
						  PINCTRL_STATE_INT_INPUT);
	if (IS_ERR_OR_NULL(pinctrl->int_input)) {
		ERROR();
		goto exit_pinctrl_init;
	} else {
		FUNC();
	}
	
	DEBUG("Success init pinctrl\n");
	return 0;
exit_pinctrl_init:
	devm_pinctrl_put(pinctrl->pinctrl);
	pinctrl->pinctrl = NULL;
	pinctrl->int_out_high = NULL;
	pinctrl->int_input = NULL;
	return 0;
}

void switch_int_output(void)
{
	FUNC();
	if (pinctrl->pinctrl)
		pinctrl_select_state(pinctrl->pinctrl,
					 pinctrl->int_out_high);
	else
		ERROR();
}

void switch_int_sync(void)
{
	FUNC();
	if (pinctrl->pinctrl) {
		pinctrl_select_state(pinctrl->pinctrl,
				     pinctrl->default_sta);
		switch_int_output();
		pinctrl_select_state(pinctrl->pinctrl,
				     pinctrl->int_input);
	} else {
		ERROR();
	}
}
#endif
static int gpio_switch_probe(struct platform_device *pdev)
{
	//struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_switch_platform_data *pdata;
	struct gpio_switch_data *switch_data;
	int ret = 0;
	FUNC();
	
	pdata = kzalloc(sizeof(struct gpio_switch_platform_data), GFP_KERNEL);
	if (!pdata)
		return -EBUSY;
	pdata->name = "hdmi_audio";

	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;
	FUNC();
	
	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->sdev.print_state = switch_gpio_print_state;

	ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;
	
	FUNC();
	#if 0
	ret = gpio_request(switch_data->gpio, pdev->name);
	if (ret < 0)
		goto err_request_gpio;

	ret = gpio_direction_input(switch_data->gpio);
	if (ret < 0)
		goto err_set_gpio_input;

	INIT_WORK(&switch_data->work, gpio_switch_work);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		goto err_detect_irq_num_failed;
	}
	#endif
	#if QUECTEL_CHANGE
	FUNC();
	switch_data->gpio = of_get_named_gpio(pdev->dev.of_node,
                         "qcom,switch-en", 0);
    if (!gpio_is_valid(switch_data->gpio))
                printk("[kevin]%s:%d, vci gpio not specified\n",__func__, __LINE__);
	switch_pinctrl_init(pdev);
	gpio_direction_input(switch_data->gpio);
	if (ret < 0)
		goto err_set_gpio_input;
	INIT_WORK(&switch_data->work, gpio_switch_work);
	ret = request_irq(gpio_to_irq(switch_data->gpio),gpio_irq_handler, IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,"lt8912_hdp_irq", switch_data);
	if (ret < 0){
		ERROR();
		goto err_request_irq;	
	}
	switch_int_sync();
	#else
	ret = request_irq(switch_data->irq, gpio_irq_handler,
			  IRQF_TRIGGER_LOW, pdev->name, switch_data);
	if (ret < 0)
		goto err_request_irq;
	#endif

	/* Perform initial detection */
	gpio_switch_work(&switch_data->work);
	FUNC();
	return 0;

err_request_irq:
#if 0
err_detect_irq_num_failed:
#endif
err_set_gpio_input:
	gpio_free(switch_data->gpio);
#if 0
err_request_gpio:
#endif
	switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	cancel_work_sync(&switch_data->work);
	gpio_free(switch_data->gpio);
	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	return 0;
}
#if QUECTEL_CHANGE
static struct of_device_id switch_gpio_id[] =
{
        {
            .compatible = "qcom,switch_gpio",
        },
};
#endif
static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= gpio_switch_remove,
	.driver		= {
		.name	= "switch-gpio",
		.owner	= THIS_MODULE,
		#if QUECTEL_CHANGE
		.of_match_table = switch_gpio_id,
		#endif
	},
};

static int __init gpio_switch_init(void)
{
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	platform_driver_unregister(&gpio_switch_driver);
}

module_init(gpio_switch_init);
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
