#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>


#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <asm/fcntl.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

struct qpdata_x_data
{
	unsigned int hdmi_dp_change;
};
static struct qpdata_x_data *data;
static char qpdata1_path[] = "/dev/block/by-name/qpdata1";
#define FILE_READ 0
#define FILE_WRITE 1
static int file_rw(char* path,char* buf, int size,int num,int offset,int rw_flag)
{
	
    struct file *fp;  
    mm_segment_t fs;  
    loff_t pos;
	int ret;
	if((0 > num) ||(0 >= size)  ||(0 > rw_flag) ||(buf == NULL) ||path == NULL)
	{
		printk("[kevin]%s,%d,file_rw arg error\n",__func__,__LINE__);
		return -EBADF;
	}
	
	
    fp = filp_open(path, O_RDWR, 0664);  
    if (IS_ERR(fp)) {  
        printk("[kevin]%s,%d,fopen error\n",__func__,__LINE__);
		return -EBADF;
    }
    fs = get_fs();  
    set_fs(KERNEL_DS);
    pos = fp->f_pos; 

	if(rw_flag == 0)
	{
		ret = vfs_read(fp, buf, size, &pos);
		if(ret < 0){
			printk("[kevin]%s,%d,vfs_read error:%d\n",__func__,__LINE__,ret);
			goto err;
		}else{
			fp->f_pos = pos;
			set_fs(fs);
			printk("[kevin]%s,%d,vfs_read sucess:%s,ret = %d\n",__func__,__LINE__,buf,ret);
		}	
	}
	else
	{
		ret = vfs_write(fp, buf, size, &pos);  
		if(ret < 0){
			printk("[kevin]%s,%d,vfs_write error:%d\n",__func__,__LINE__,ret);
			goto err;
		}else{
			fp->f_pos = pos;
			set_fs(fs);
			printk("[kevin]%s,%d,vfs_write sucess:%s,ret = %d\n",__func__,__LINE__,buf,ret);
		}	
	}

	filp_close(fp, NULL);  
	printk("[kevin]%s,%d,file_rw sucess\n",__func__,__LINE__);
	return 0;
	err:
	filp_close(fp, NULL);  
	printk("[kevin]%s,%d,file_rw fail\n",__func__,__LINE__);
	return -EBADF;
}

static ssize_t qpdata_x_show(struct class *class, struct class_attribute *attr, char *buf)
{
	char hdmi_dp_flag[] = {1};
	int ret;
	if(0 > (ret = file_rw(qpdata1_path,hdmi_dp_flag,sizeof(char),1,1,FILE_READ)))
	{
		printk("[kevin]%s,%d qpdata_x_show error\n",__func__,__LINE__);
		return ret;
	}
	printk("[kevin]%s,%d qpdata_x_show sucess\n",__func__,__LINE__);
	return (sprintf(buf, "%s\n", hdmi_dp_flag));
}

static ssize_t qpdata_x_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	//int rc;
	int ret;

	//rc = kstrtouint(buf, 0, &data->hdmi_dp_change);
	//if (rc < 0)
	//	printk("[kevin]%s,%d,get hdmi_dp_change value failed!\n",__func__,__LINE__);
	
	if(0 > (ret = file_rw(qpdata1_path,(char*)buf,sizeof(char),1,1,FILE_WRITE)))
	{
		printk("[kevin]%s,%d qpdata_x_store error\n",__func__,__LINE__);
		return ret;
	}
	printk("[kevin]%s,%d qpdata_x_store sucess\n",__func__,__LINE__);
	return len;
}

static struct class_attribute qpdata_class_attrs[] =
{

	__ATTR(qpdata_x, 0664, qpdata_x_show, qpdata_x_store),
	__ATTR_NULL,
};

static struct class qpdata_x_class =
{
	.name = "qpdata_x",
	.owner = THIS_MODULE,
	.class_attrs = qpdata_class_attrs,
};


static int __init qpdata_x_init(void)
{
	int rc = 0;
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
	{
		return -ENOMEM;
	}
	printk("[kevin]%s: %d\n", __func__,__LINE__);
	rc = class_register(&qpdata_x_class);

	return rc;
}

static void __exit qpdata_x_exit(void)
{
	class_unregister(&qpdata_x_class);
}

module_init(qpdata_x_init);
module_exit(qpdata_x_exit);

MODULE_LICENSE("GPL");

