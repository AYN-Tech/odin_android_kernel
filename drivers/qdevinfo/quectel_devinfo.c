/*********************************************************************************
 *      Copyright:  (C) 2019 quectel
 *                  All rights reserved.
 *
 *       Filename:  quectel_devinfo.c
 *    Description:  add this driver for get device information(AT+QDEVINFO)
 *
 *        Version:  1.0.0(20190227)
 *         Author:  Geoff Liu <geoff.liu@quectel.com>
 *      ChangeLog:  1, Release initial version on 20190227
 *                  Modify by Peeta Chen.
 ********************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#define QUECTEL_QDEVINFO_CMD
#ifdef QUECTEL_QDEVINFO_CMD

#define EMMC_NAME_STR_LEN   32
#define DDR_STR_LEN         64
#define EXT_CSD_STR_LEN     1025
#define MMC_NAME            "/sys/class/mmc_host/mmc0/mmc0:0001/name"
#define MMC_EXT_CSD         "/sys/kernel/debug/mmc0/mmc0:0001/ext_csd"
#define MEMINFO             "/proc/meminfo"
#define UFS_CAPACITY "/sys/kernel/debug/1d84000.ufshc/quec_ufs_capacity"
#define UFS_LIFE "/sys/kernel/debug/1d84000.ufshc/quec_ufs_life"
#define UFS_NAME "/sys/kernel/debug/1d84000.ufshc/quec_ufs_name"


/* read file */
int get_buf(const char *filename, char *buf, int size)
{
    int length;
    struct file *fp;
    mm_segment_t fs;
    loff_t pos;

    fp = filp_open(filename, O_RDONLY | O_LARGEFILE, 0);
    if (IS_ERR(fp)) {
        printk(KERN_ERR "create file error\n");
        return -1;
    }

    fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
    length = vfs_read(fp, buf, size, &pos);
    filp_close(fp, NULL);
    set_fs(fs);
    printk(KERN_INFO "length = %d", length);

    return length;
}


extern void quectel_get_ufs_model_info(char *buf, int size);

static int quec_ufs_model_proc_show(struct seq_file *m, void *v)
{
    char *kbuf;

    kbuf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
    memset(kbuf, 0, EXT_CSD_STR_LEN + 1);

    quectel_get_ufs_model_info(kbuf, EXT_CSD_STR_LEN);

    seq_printf(m, "%s", kbuf);

    kfree(kbuf);

    return 0;
}

static int quec_ufs_model_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, quec_ufs_model_proc_show, inode->i_private);
}

static const struct file_operations quec_ufs_model_info_proc_fops = {
    .open       = quec_ufs_model_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};

char change_char_hex(char c)
{
    if ((c >= '0') && (c <= '9'))
        return (c - '0');
    else if ((c >= 'a') && (c <= 'f'))
        return (c - 'a' + 10);
    else if ((c >= 'A') && (c <= 'F'))
        return (c - 'A' + 10);

    return 0;
}

extern void quectel_get_pmic_info(char *buf);

static int quec_pmu_info_proc_show(struct seq_file *m, void *v)
{
    char pmu_info[64] = {'\0'};

    quectel_get_pmic_info(pmu_info);
    seq_printf(m, "%s\n", pmu_info);
    return 0;
}

static int quec_ufs_size_proc_show(struct seq_file *m, void *v)
{
    char *kbuf;

    kbuf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
    memset(kbuf, 0, EXT_CSD_STR_LEN + 1);
    get_buf(UFS_CAPACITY, kbuf, EXT_CSD_STR_LEN);
	
	if(!strncmp(kbuf,"ufs_capacity",sizeof("ufs_capacity")-1))
		kbuf += (sizeof("ufs_capacity")-1);
    seq_printf(m, "%s", kbuf);
    kfree(kbuf);

    return 0;
}

static int quec_ufs_life_proc_show(struct seq_file *m, void *v)
{
    char *kbuf;

    kbuf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
    memset(kbuf, 0, EXT_CSD_STR_LEN + 1);
    get_buf(UFS_LIFE, kbuf, EXT_CSD_STR_LEN);
	seq_printf(m, "%s", kbuf);
	
    kfree(kbuf);

    return 0;
}
static int quec_ufs_name_proc_show(struct seq_file *m, void *v)
{
    char *kbuf;

    kbuf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
    memset(kbuf, 0, EXT_CSD_STR_LEN + 1);
    get_buf(UFS_NAME, kbuf, EXT_CSD_STR_LEN);
	seq_printf(m, "%s", kbuf);
	
    kfree(kbuf);

    return 0;
}

static int quec_pmu_info_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, quec_pmu_info_proc_show, inode->i_private);
}
static int quec_ufs_size_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, quec_ufs_size_proc_show, inode->i_private);
}
static int quec_ufs_life_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, quec_ufs_life_proc_show, inode->i_private);
}
static int quec_ufs_name_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, quec_ufs_name_proc_show, inode->i_private);
}

static const struct file_operations quec_pmu_info_proc_fops = {
    .open       = quec_pmu_info_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
static const struct file_operations quec_ufs_size_proc_fops = {
	.open		= quec_ufs_size_proc_open,
	.read		= seq_read,
	.llseek 	= seq_lseek,
	.release	= single_release,
};
static const struct file_operations quec_ufs_life_proc_fops = {
	.open		= quec_ufs_life_proc_open,
	.read		= seq_read,
	.llseek 	= seq_lseek,
	.release	= single_release,
};
static const struct file_operations quec_ufs_name_proc_fops = {
	.open		= quec_ufs_name_proc_open,
	.read		= seq_read,
	.llseek 	= seq_lseek,
	.release	= single_release,
};

static int qdevinfo_proc_create(void)
{
    printk(KERN_INFO "proc create\n");
    proc_create("quec_ufs_model_info", 0444, NULL, &quec_ufs_model_info_proc_fops);
    proc_create("quec_pmu_info", 0444, NULL, &quec_pmu_info_proc_fops);
    proc_create("quec_ufs_size", 0444, NULL, &quec_ufs_size_proc_fops); 
    proc_create("quec_ufs_life", 0444, NULL, &quec_ufs_life_proc_fops); 
	proc_create("quec_ufs_name", 0444, NULL, &quec_ufs_name_proc_fops); 
    return 0;
}

static int __init quec_devinfo_init(void)
{
    if (qdevinfo_proc_create()){
        printk(KERN_ERR "quec_devinfo init failed!\n");
    	}
    else
        printk(KERN_INFO "quec_devinfo init success!\n");

    pr_info("fulinux I am here\n");

    return 0;
}

static void __exit quec_devinfo_exit(void)
{
    printk(KERN_DEBUG "quectel devinfo exit!");
}

module_init(quec_devinfo_init);
module_exit(quec_devinfo_exit);

MODULE_AUTHOR("geoff.liu@quectel.com");
MODULE_LICENSE("GPL");

#endif /* QUECTEL_QDEVINFO_CMD */
