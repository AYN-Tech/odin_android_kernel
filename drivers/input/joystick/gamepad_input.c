#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include <linux/input.h>
#include <linux/gpio.h>

#define IOC_MAGIC 'k'

/* ioctl commands */
#define GAMEPAD_INPUT_NONE	            _IO(IOC_MAGIC, 0)
#define GAMEPAD_INPUT_SET_GPIO_PIN	    _IOW(IOC_MAGIC, 1, int)
#define GAMEPAD_INPUT_SET_ADC_CHANNEL	_IOW(IOC_MAGIC, 2, int)

#define DEVICE_NAME "gamepad_input"
#define DRIVER_DESC "gamepad input"

#define INFO(...)   printk(KERN_INFO __VA_ARGS__);
#define ALERT(...)  printk(KERN_ALERT __VA_ARGS__);

#define GAMEPAD_KEYS_SIZE  32
#define GAMEPAD_ADCS_SIZE   6

typedef struct {
	unsigned int pin;
} gamepad_gpio_t;

typedef struct {
	unsigned int channel;
} gamepad_adc_t;

typedef struct  {
	int open;
	int major;
	int minor;
	struct class *class;
    struct device *device;
	gamepad_gpio_t gpio[GAMEPAD_KEYS_SIZE];
	gamepad_adc_t  adc[GAMEPAD_ADCS_SIZE];
	struct cdev cdev;
}gamepad_dev_t;

static gamepad_gpio_t re_btns[GAMEPAD_KEYS_SIZE];

static gamepad_dev_t *pgamepad_dev = 0;
/**
static int gamepad_input_open(struct inode *inode, struct file *filp)
{
	gamepad_dev_t *dev;
	dev = container_of(inode->i_cdev, gamepad_dev_t, cdev);
	filp->private_data = dev;
	if(dev->open != 0)
		return -ENOMEM;
	dev->open = 1;
	try_module_get(THIS_MODULE);
	return 0;
}
***/
static int gamepad_input_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int gamepad_input_release(struct inode *inode, struct file *filp)
{
	gamepad_dev_t *dev;
	dev = container_of(inode->i_cdev, gamepad_dev_t, cdev);
	dev->open = 0;
	module_put(THIS_MODULE);
	return 0;
}

static ssize_t gamepad_input_read(struct file *filp, char *buff, size_t count, loff_t *offp)
{
	ssize_t bytes_read = 0;
	ssize_t i = 0;
	while(count--)
	{
		put_user(gpio_get_value(re_btns[i].pin), buff++);
		bytes_read++;
		i++;		
	}

	return bytes_read;
}

static ssize_t gamepad_input_write(struct file *filp, const char *buff, size_t count, loff_t *offp)
{
	return 0;
}

static long gamepad_input_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
//	gamepad_dev_t *dev = (gamepad_dev_t *)filp->private_data;
	ssize_t i = 0;

   	switch (cmd) {
		case GAMEPAD_INPUT_SET_GPIO_PIN:
			while (re_btns[i].pin > 0) 	i++;
				re_btns[i].pin = (unsigned int)arg;
				INFO("gamepad_input_ioctl pgpio->pin = %d ",re_btns[i].pin);
			break;
		default:
			ALERT("Command unknown\n");
			return -EINVAL;
			break;
	}

	/***
   	switch (cmd) {
		case GAMEPAD_INPUT_SET_GPIO_PIN:
			pgpio = &dev->gpio[0];
			while (pgpio->pin > 0) 	pgpio++;
				pgpio->pin = (unsigned int)arg;
				INFO("gamepad_input_ioctl pgpio->pin = %d ",pgpio->pin);
			break;
		case GAMEPAD_INPUT_SET_ADC_CHANNEL:
			padc = &dev->adc[0];
			while (padc->channel > 0) padc++;
			padc->channel = (unsigned int)arg;
			break;	
		default:
			ALERT("Command unknown\n");
			return -EINVAL;
			break;
	}
	***/
	return 0;
}

static ssize_t gamepad_getinfo_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int count;
	int value = gpio_get_value(0);
	
	count = sprintf(buf,"adc channel 1:%d ,channel 12:%d pin 0 %d  \n",0,0,value);
	
	return count;
}

static DEVICE_ATTR(getinfo, 0664, gamepad_getinfo_show, NULL);

static struct device_attribute *gamepad_attributes[] = {
	&dev_attr_getinfo,
	NULL
};

static struct file_operations gamepad_fops = {
	.read	    = gamepad_input_read,
	.write      = gamepad_input_write,
	.open	    = gamepad_input_open,
	.unlocked_ioctl		= gamepad_input_ioctl,
	.release    = gamepad_input_release,
};

static int __init gamepad_input_init(void)
{
	gamepad_dev_t  *gdev;
	dev_t dev;
	int devno;
	int result = -1;
	struct device_attribute **attrs = gamepad_attributes;
	struct device_attribute *attr;

	gdev = (gamepad_dev_t *)kmalloc(sizeof(gamepad_dev_t), GFP_KERNEL);
	if (!gdev) {
		result = -ENOMEM;
		goto fail;
	}
	memset(gdev, 0, sizeof(gamepad_dev_t));
	result = alloc_chrdev_region(&dev, gdev->minor, 1, DEVICE_NAME);
	gdev->major = MAJOR(dev);
	devno = MKDEV(gdev->major, gdev->minor);
	cdev_init(&gdev->cdev, &gamepad_fops);
	gdev->cdev.owner = THIS_MODULE;
	result = cdev_add (&gdev->cdev, devno, 1);

	/* /sys device*/
	gdev->class = class_create(THIS_MODULE, DEVICE_NAME);
	gdev->device = device_create(gdev->class, NULL, devno, "%s", DEVICE_NAME);
	while ((attr = *attrs++)) {
		result = device_create_file(gdev->device, attr);
		if (result) {
			device_destroy(gdev->class, gdev->device->devt);
			goto fail;
		}
	}
	printk("gamepad_input_init major:%d minor:%d\n",gdev->major,gdev->minor);
	pgamepad_dev = gdev;
	return 0;
fail:
	printk("gamepad_input_init fail:%d\n",result);
	return -1;
}

static void __exit gamepad_input_exit(void)
{
	dev_t devno = MKDEV(pgamepad_dev->major, pgamepad_dev->minor);
	unregister_chrdev_region(devno, 1);
	device_unregister(pgamepad_dev->device);  
	class_destroy(pgamepad_dev->class);
	kfree(pgamepad_dev);
	pgamepad_dev = 0;
}

module_init(gamepad_input_init);
module_exit(gamepad_input_exit);

MODULE_AUTHOR("retrostation");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
