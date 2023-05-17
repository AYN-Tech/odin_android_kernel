#include <linux/kernel.h> // We're doing kernel work
#include <linux/module.h> // Specifically, a module
#include <linux/proc_fs.h> // Necessary because we use the proc fs
#include <linux/workqueue.h> // We scheduale tasks here
#include <linux/sched.h> // We need to put ourselves to sleep and wake up later
#include <linux/init.h> // For __init and __exit
#include <linux/interrupt.h> // For irqreturn_t
#include <asm/delay.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/miscdevice.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/switch.h>

#include <linux/qpnp/qpnp-adc.h>
#include <linux/msm_drm_notify.h>
#include <linux/notifier.h>
#include <linux/fb.h>


#define KERNEL_PRINTK 1
//__FILE__, 

#define driver_debug(level, format, args...)  \
			do{ \
				printk(level "Ayn debug [%s():%d]" format, __FUNCTION__, __LINE__, ##args); \
			}while(0)


#define driver_info(level, format, args...)  \
			do{ \
				printk(level "Ayn [%s():%d]" format, __FUNCTION__, __LINE__, ##args); \
			}while(0)



#define KERNEL_INFO(format , args...) driver_info(KERN_ALERT, format, ## args)
#define KERNEL_ERR(format , args...) driver_info(KERN_ERR, format, ## args)

#if KERNEL_PRINTK
#define KERNEL_DEBUG(format , args...) driver_debug(KERN_DEBUG, format, ## args)
#else
#define KERNEL_DEBUG(format , args...)
#endif

typedef struct {
	unsigned int pin;
	unsigned int cur_status;
	unsigned int new_status;
	unsigned int counter;
} aynkey_gpio_t;

typedef struct {
	struct qpnp_vadc_result new_result;
	int32_t			cur_adc;
	int64_t			cur_phy;
	unsigned int threshold;
} aynkey_adcs_t;

struct aynkey_data {
	struct notifier_block fb_notifier;
	bool fb_ready;
};


#define AYNKEY_GPIO_SIZE		17
#define AYNKEY_ADCS_SIZE		6

int AYNKEY_GPIO_COUNTER			= 5;
int INTERVAL_RUNNING_GPIO		= 6000;
int INTERVAL_RUNNING_ADCS		= 5500; // 5500 120hz 12000 60hz
int THRESHOLD_REPORT14			= 2000;
int THRESHOLD_REPORT56			= 6000;

int THRESHOLD_REPORT14_ADS7828			= 3;
int THRESHOLD_REPORT56_ADS7828			= 3;


//#define ADC_VALUE_MAX			1700000
#define ADC_VALUE_MIN			100000
#define ADC_VALUE_MIN_ADS7830	10

#define ADC_VALUE_JUMP			80000

#define MSG_BUF_MAX				1024
#define MSG_BUF_MIM				64

static unsigned int gpiopin[AYNKEY_GPIO_SIZE] = {
	124, 128, 52, 122, 96,
	95, 92, 41, 26, 91,
	49, 3, 40, 22, 116,
	24, 0,
};

static int working_start = 0;
static int bright = 1;

static aynkey_gpio_t gpiokey[AYNKEY_GPIO_SIZE];
static aynkey_adcs_t adcskey[AYNKEY_ADCS_SIZE];

struct mutex lock_send_msg;
static char send_msg[MSG_BUF_MAX]={0};
static unsigned int send_msg_len = 0;

extern ssize_t ads7828_read(int channel, int *v);

void aynkey_gpio_init(void)
{
	int index;
	for(index=0; index<AYNKEY_GPIO_SIZE; index++){
		gpiokey[index].pin = gpiopin[index];
		gpiokey[index].cur_status = 1; // 松开为1
		gpiokey[index].new_status = -1;
		gpiokey[index].counter = 0;
		KERNEL_INFO("gpio%4d init success\n", gpiokey[index].pin);
	}
	
	for(index=0; index<AYNKEY_ADCS_SIZE; index++){
		if(index < 4){
			adcskey[index].threshold = THRESHOLD_REPORT14;
		} else {
			adcskey[index].threshold = THRESHOLD_REPORT56;
		}
		adcskey[index].cur_adc = 0;
		adcskey[index].cur_phy = 0;
		adcskey[index].new_result.adc_code = 0;
		adcskey[index].new_result.chan = 0;
		adcskey[index].new_result.measurement = 0;
		adcskey[index].new_result.physical = 0;
		KERNEL_INFO("adcs%4d init success\n", gpiokey[index].pin);
	}

	KERNEL_INFO("aynkey parameter counter=%d, intervalgpio=%d, intervaladc=%d, threshold14=%d, threshold56=%d, start=%d\n",
		AYNKEY_GPIO_COUNTER,
		INTERVAL_RUNNING_GPIO,
		INTERVAL_RUNNING_ADCS,
		THRESHOLD_REPORT14,
		THRESHOLD_REPORT56,
		working_start);

	/*
	for(index=0; index<AYNKEY_GPIO_SIZE; index++){
		if (gpio_is_valid(gpiokey[index].pin)) {
			ret = gpio_request(gpiokey[index].pin, "aynkey_gpio");
			if (ret) {
				KERNEL_INFO("Unable to request gpio%d \n", gpiokey[index].pin);
				gpio_free(gpiokey[index].pin);
				gpiokey[index].pin = -1;
				continue;
			}

			ret = gpio_direction_input(gpiokey[index].pin);
			if (ret) {
				KERNEL_INFO("Unable to set direction for gpio%d\n", gpiokey[index].pin);
				gpio_free(gpiokey[index].pin);
				gpiokey[index].pin = -1;
				continue;
			}
			
			ret = gpio_export(gpiokey[index].pin, 1);
			if (ret) {
				KERNEL_INFO("Unable to export gpio%d \n", gpiokey[index].pin);
				gpio_free(gpiokey[index].pin);
				gpiokey[index].pin = -1;
				continue;
			}
			//gpiokey[index].cur_status = gpio_get_value(gpiokey[index].pin);
			KERNEL_INFO("request gpio%d=%d success\n", gpiokey[index].pin, gpiokey[index].cur_status);
			
		} else {
			KERNEL_INFO("Invalid gpio%d!\n", gpiokey[index].pin);
			gpiokey[index].pin = -1;
			continue;
		}
	}*/

}

void aynkey_gpio_free(void)
{
	/*
	int index;
	for(index=0; index<AYNKEY_GPIO_SIZE; index++){
		if (gpiokey[index].pin >= 0) {
			gpio_unexport(gpiokey[index].pin);
			gpio_free(gpiokey[index].pin);
			KERNEL_INFO("free gpio%d success\n", gpiokey[index].pin);
		}
	}*/

}


static struct fasync_struct *signal_async = NULL;
#define FORMAT_KEY			"K:%d,V:%d;" // K：对应的GPIO号，V：0表示低电平，1表示高电平
#define FORMAT_ADC			"A:%d,V:%lld;" // A：对应的ADC号，V：表示adc的数值


void delete_front_data(void)
{
	int index = 0;
	int find_two_frame = 0;
	for(index=0; index<MSG_BUF_MIM; index++){
		if(send_msg[index] == ';'){
			find_two_frame += 1;
			if(find_two_frame == 2){ // 去掉BUF中的前面二帧数据
				index++;
				memmove(send_msg, send_msg+index, send_msg_len-index);
				send_msg_len -= index;
				//KERNEL_INFO("send_msg_len=%d send_msg=%s \n", send_msg_len, send_msg);
				return;
			}
		}
	}
}

int read_gpio_work(void *arg)
{
	int index=0, gpio_status=-1;

	//set_freezable();
	KERNEL_INFO("read_gpio_work enter!\n");

	while (1) {
		//set_current_state(TASK_INTERRUPTIBLE);
		if(working_start){
			usleep_range(INTERVAL_RUNNING_GPIO, INTERVAL_RUNNING_GPIO+100);
		} else {
			usleep_range(INTERVAL_RUNNING_GPIO*100,INTERVAL_RUNNING_GPIO*1000);
			continue;
		}

		//if (kthread_freezable_should_stop(NULL))
			//break;

		for(index=0; index<AYNKEY_GPIO_SIZE; index++){
			if (gpiokey[index].pin >= 0) {
				gpio_status = gpio_get_value(gpiokey[index].pin);
				if(gpio_status == gpiokey[index].new_status && gpio_status != gpiokey[index].cur_status){
					gpiokey[index].counter++;
					if(gpiokey[index].counter >= AYNKEY_GPIO_COUNTER){
						gpiokey[index].new_status = gpio_status;
						gpiokey[index].cur_status = gpio_status;

						mutex_lock(&lock_send_msg);
						if(send_msg_len > MSG_BUF_MAX - MSG_BUF_MIM){
							delete_front_data();
						}
						send_msg_len += sprintf(send_msg+send_msg_len, FORMAT_KEY, gpiokey[index].pin, gpiokey[index].new_status);
						mutex_unlock(&lock_send_msg);

						//发送SIGIO信号给fasync_struct结构体所描述的PID，触发应用程序的SIGIO信号处理函数
						if(signal_async != NULL){
							kill_fasync(&signal_async, SIGIO, POLL_IN);
						}
						//KERNEL_INFO("index=%d gpio%d=%d \n", index, gpiokey[index].pin, gpio_status);

					}
				} else {
					gpiokey[index].counter = 0;
					gpiokey[index].new_status = gpio_status;
				}
				//KERNEL_INFO("index=%d, gpio%d=%d\n", index, gpiokey[index].pin, gpio_status);
			}
			//KERNEL_INFO("index=%d, gpio_status=%d\n", index, gpio_status);
		}
	}
	KERNEL_INFO("read_gpio_work exit!\n");
	return 0;
}



extern int32_t qpnp_vadc_odin_read(enum qpnp_vadc_channels channel, struct qpnp_vadc_result *result);
extern int32_t qpnp_vadc_odin_hc_read(enum qpnp_vadc_channels channel, struct qpnp_vadc_result *result);
extern int32_t qpnp_vadc_irq_read(enum qpnp_vadc_channels channel, struct qpnp_vadc_result *result);

static char adcs_init_data[128]={0};
struct timeval tv;

//void ads7828_adc_work_fn(struct work_struct *work)

int read_adcs_work(void *arg)
{
	int index=0, rc=-1;
	int threshold = 0;
	char need_send = 0;
	//int channel[] = {19, 20, 151, 22, 18, 21};
	//int channel[] = {0,1,2,3,4,5};
	//int cur_sec = 0;
	//int hit = 0;

	KERNEL_INFO("enter!\n");


	for(index=0; index<AYNKEY_ADCS_SIZE; index++){
READ_AGAIN:
	
		usleep_range(INTERVAL_RUNNING_ADCS, INTERVAL_RUNNING_ADCS+100);
		rc = qpnp_vadc_odin_read(index, &(adcskey[index].new_result));
		if (rc) {
			need_send++;
			KERNEL_ERR("VADC read error with %d, readTime=%d\n", rc, need_send);
			goto READ_AGAIN;
		}
		threshold += sprintf(adcs_init_data+threshold, FORMAT_ADC, index, adcskey[index].new_result.physical);
		adcskey[index].cur_phy = adcskey[index].new_result.physical;
		KERNEL_INFO("init data adc%d=%lld \n", index, adcskey[index].new_result.physical);
		
	}
	KERNEL_ERR("adcs readTime=%d\n", need_send);

	while (1) {
		if(working_start && bright){
			usleep_range(INTERVAL_RUNNING_ADCS, INTERVAL_RUNNING_ADCS+100);
		} else {
			usleep_range(INTERVAL_RUNNING_ADCS*100,INTERVAL_RUNNING_ADCS*100+100);
			continue;
		}
		for(index=0; index<AYNKEY_ADCS_SIZE; index++){
			
			//KERNEL_ERR("qpnp_vadc_odin_read start \n");
			//qpnp_vadc_irq_read(channel[index], &(adcskey[index].new_result));
			rc = qpnp_vadc_odin_read(index, &(adcskey[index].new_result));
			//rc = qpnp_vadc_odin_hc_read(channel[index], &(adcskey[index].new_result));
			if (rc) {
				printk_ratelimited("VADC read error with %d\n", rc);
				continue;
			} else {
				//KERNEL_INFO("index=%d adc%d=%d physical=%lld\n", index, index, adcskey[index].new_result.adc_code, adcskey[index].new_result.physical);
				//if(adcskey[index].new_result.physical < ADC_VALUE_MIN || adcskey[index].new_result.physical > ADC_VALUE_MAX){
				//	KERNEL_INFO("continue  ");
				//	continue; // 小于8万或者大于170万的值舍弃
				//}
				if(adcskey[index].new_result.physical > adcskey[index].cur_phy){
					threshold = adcskey[index].new_result.physical - adcskey[index].cur_phy;
				} else {
					threshold = adcskey[index].cur_phy - adcskey[index].new_result.physical;
				}
			
				//if(threshold > ADC_VALUE_JUMP){
				//	continue; // 跳变大于8万的值舍弃
				//}

				if(threshold > adcskey[index].threshold){
					adcskey[index].cur_phy = adcskey[index].new_result.physical;

					mutex_lock(&lock_send_msg);
					if(send_msg_len > MSG_BUF_MAX - MSG_BUF_MIM){
						delete_front_data();
					}
					send_msg_len += sprintf(send_msg+send_msg_len, FORMAT_ADC, index, adcskey[index].new_result.physical);
					mutex_unlock(&lock_send_msg);

					need_send = 1;
					//KERNEL_INFO("adc%d=%lld send_msg_len=%d send_msg=%s\n", index, adcskey[index].new_result.physical, send_msg_len, send_msg);
				}
			}
		}
		/*
		hit++;
		do_gettimeofday(&tv);
		if(tv.tv_sec - cur_sec >=1){
			//KERNEL_ERR("hit:%d\n tv_sec %d  tv_nsec %ld",hit,(int)tv.tv_sec,tv.tv_usec);
			//KERNEL_ERR("hit:%d\n ",hit);
			hit = 0;
		    cur_sec = tv.tv_sec;
		}
*/
		//KERNEL_ERR("tv_sec %d	tv_nsec %ld",(int)tv.tv_sec,tv.tv_usec);

		if(need_send){
			need_send = 0;
			//发送SIGIO信号给fasync_struct结构体所描述的PID，触发应用程序的SIGIO信号处理函数
			if(signal_async != NULL){
				kill_fasync(&signal_async, SIGIO, POLL_IN);
			}
			//KERNEL_INFO("send_msg_len=%d send_msg=%s\n", send_msg_len, send_msg);
		}

	}
	KERNEL_INFO("read_adcs_work exit!\n");
	return 0;
}

int ads7828_read_adcs_work(void *arg)
{
	int index=0, rc=-1;
	int threshold = 0;
	char need_send = 0;
	
	//int channel[] = {19, 20, 151, 22, 18, 21};
	int channel[] = {4, 0, 1, 5, 3, 2};

	int v;
	usleep_range(INTERVAL_RUNNING_ADCS*500, INTERVAL_RUNNING_ADCS*500+100);
	for(index=0; index<AYNKEY_ADCS_SIZE; index++){
READ_AGAIN:
	
		usleep_range(INTERVAL_RUNNING_ADCS, INTERVAL_RUNNING_ADCS+100);
		rc = ads7828_read(index, &v);
		
		adcskey[index].new_result.physical = v;
		if (rc) {
			need_send++;
			KERNEL_ERR("VADC read error with %d, readTime=%d\n", rc, need_send);
			goto READ_AGAIN;
		}
		if(adcskey[index].new_result.physical < 80)
		{
			KERNEL_ERR("init data error try again v %d \n", (int)adcskey[index].new_result.physical);
			goto READ_AGAIN;	
		}
		threshold += sprintf(adcs_init_data+threshold, FORMAT_ADC, channel[index], adcskey[index].new_result.physical);
		adcskey[index].cur_phy = adcskey[index].new_result.physical;
		KERNEL_INFO("init data adc%d=%lld \n", index, adcskey[index].new_result.physical);
		
	}
	KERNEL_ERR("adcs readTime=%d\n", need_send);

	while (1) {
		if(working_start && bright){
			usleep_range(INTERVAL_RUNNING_ADCS, INTERVAL_RUNNING_ADCS+100);
		} else {
			usleep_range(INTERVAL_RUNNING_ADCS*100,INTERVAL_RUNNING_ADCS*100+100);
			continue;
		}

		for(index=0; index<AYNKEY_ADCS_SIZE; index++){
			rc = ads7828_read(index, &v);
			adcskey[index].new_result.physical = v;
			if (rc) {
				//KERNEL_ERR("VADC read error with %d\n", rc);
				continue;
			} else {
				//KERNEL_INFO("adc%d=%d\n", index, adcskey[index].new_result.physical);
				
				if(adcskey[index].new_result.physical > adcskey[index].cur_phy){
					threshold = adcskey[index].new_result.physical - adcskey[index].cur_phy;
				} else {
					threshold = adcskey[index].cur_phy - adcskey[index].new_result.physical;
				}
				if(threshold > adcskey[index].threshold){
					adcskey[index].cur_phy = adcskey[index].new_result.physical;

					//mutex_lock(&lock_send_msg);
					if(send_msg_len > MSG_BUF_MAX - MSG_BUF_MIM){
						delete_front_data();
					}
					send_msg_len += sprintf(send_msg+send_msg_len, FORMAT_ADC, channel[index], adcskey[index].new_result.physical);
					//mutex_unlock(&lock_send_msg);

					need_send = 1;
					KERNEL_INFO("channel[%d]=%d adc%d=%lld send_msg_len=%d send_msg=%s\n", index, channel[index], channel[index], adcskey[index].new_result.physical, send_msg_len, send_msg);
				}

			}
		}

		if(need_send){
			need_send = 0;
			//发送SIGIO信号给fasync_struct结构体所描述的PID，触发应用程序的SIGIO信号处理函数
			if(signal_async != NULL){
				kill_fasync(&signal_async, SIGIO, POLL_IN);
			}
		}
		
		//KERNEL_ERR("hit:%d\n tv_sec %d	tv_nsec %ld",hit,(int)tv.tv_sec,tv.tv_usec);
		/*	
		hit++;
		do_gettimeofday(&tv);
		if(tv.tv_sec - cur_sec >=1){
			KERNEL_ERR("hit:%d	INTERVAL_RUNNING_ADCS %d\n ",hit,INTERVAL_RUNNING_ADCS);
			hit = 0;
			cur_sec = tv.tv_sec;
		}
		*/

	}
	
	KERNEL_INFO("ads7828_read_adcs_work exit!\n");
	return 0;
}



static struct task_struct *gpio_detect_task = NULL;
static struct task_struct *adcs_detect_task = NULL;

int init_start_task(void)
{
	int rc;
	int index;
	mutex_init(&lock_send_msg);

	if(gpio_detect_task == NULL){
		gpio_detect_task = kthread_run(read_gpio_work, NULL, "aynkey_gpio");
		if (IS_ERR(gpio_detect_task)) {
			KERNEL_ERR("kthread_create failed\n");
			return -1;
		}
	}
	
	rc = qpnp_vadc_odin_read(1, &(adcskey[1].new_result));
	KERNEL_INFO("init qpnp_vadc_odin_read :%d rc %d.\n", (int)adcskey[1].new_result.physical,rc);
	
	//if((int)adcskey[1].new_result.physical < 10000)
	if(0)
	{
		
		KERNEL_INFO("init use ads7830 ");
		if(adcs_detect_task == NULL){
			adcs_detect_task = kthread_run(ads7828_read_adcs_work, NULL, "aynkey_7828_adcs");
			if (IS_ERR(adcs_detect_task)) {
				KERNEL_ERR("kthread_create failed\n");
				return -1;
			}
		}

		for(index=0; index<AYNKEY_ADCS_SIZE; index++){
				adcskey[index].threshold = THRESHOLD_REPORT14_ADS7828;			
		}
	}
	else
	{
		
		KERNEL_INFO("init default");
		if(adcs_detect_task == NULL){
			adcs_detect_task = kthread_run(read_adcs_work, NULL, "aynkey_adcs");
			if (IS_ERR(adcs_detect_task)) {
				KERNEL_ERR("kthread_create failed\n");
				return -1;
			}
		}			
	}


	
	return 0;
}

void destroy_task(void)
{
	if(gpio_detect_task != NULL){
		kthread_stop(gpio_detect_task);
		gpio_detect_task = NULL;
	}
	if(adcs_detect_task != NULL){
		kthread_stop(adcs_detect_task);
		adcs_detect_task = NULL;
	}


}

int para(const char *p, const char **pend, int len, int *pd)
{
	p = memchr(p, ' ', len);
	if(!p){
		return 0;
	}
	while(*p == ' ')p++;
	*pd = simple_strtol(p, NULL, 10);
	*pend = p;
	return 1;
}

#define USER_READ_ADCS_INIT_DATA 3000

// 当用户试图从设备空间读取数据时，调用这个函数
ssize_t aynkey_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t cnt = 0;

	//mutex_lock(&lock_send_msg);
	if(len == USER_READ_ADCS_INIT_DATA){
		cnt = copy_to_user(buf, adcs_init_data, sizeof(adcs_init_data));// 将内核空间的数据copy到用户空间
		if(cnt){
			KERNEL_ERR("ERROR occur when reading init!!\n");
			return -EFAULT;
		}
		
		KERNEL_INFO("init send_msg_len=%d, %s\n", send_msg_len, send_msg);
		return cnt;
	}


	cnt = copy_to_user(buf, send_msg, sizeof(send_msg));// 将内核空间的数据copy到用户空间
	if(cnt){
		KERNEL_ERR("ERROR occur when reading!!\n");
		return -EFAULT;
	}
	//KERNEL_INFO("send_msg_len=%d, %s\n", send_msg_len, send_msg);
	memset(send_msg, 0, send_msg_len);
	cnt = send_msg_len;
	send_msg_len = 0;
	
	return cnt;
}

// 当用户往设备文件写数据时，调用这个函数
// counter 10 intervalgpio 30 intervaladc 30 bufsize 2048 gpiopin 总个数 0 122 124 threshold
ssize_t aynkey_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const char *p;
	int  len, total; 
	char buf[128] = {0};

	if(copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))// 将用户空间的数据copy到内核空间
		return -EFAULT;

	p = memchr(buf, '\n', count);
	len = p ? p - buf : count;
	total = len;

	p = buf;
	while(*p == ' ')p++;
	KERNEL_INFO("aynkey_write:%s.\n", p);
	while(1)
	{
		if(!strncmp(p, "counter", 7)){
			p = memchr(p, ' ', len);
			para(p, &p, len, &AYNKEY_GPIO_COUNTER);

		} else if(!strncmp(p, "intervalgpio", 12)){
			p = memchr(p, ' ', len);
			para(p, &p, len, &INTERVAL_RUNNING_GPIO);

		} else if(!strncmp(p, "intervaladc", 11)){
			p = memchr(p, ' ', len);
			para(p, &p, len, &INTERVAL_RUNNING_ADCS);

		} else if(!strncmp(p, "threshold14", 9)){
			p = memchr(p, ' ', len);
			para(p, &p, len, &THRESHOLD_REPORT14);

		} else if(!strncmp(p, "threshold56", 9)){
			p = memchr(p, ' ', len);
			para(p, &p, len, &THRESHOLD_REPORT56);

		} else if(!strncmp(p, "start", 5)){
			working_start = 1;

		} else if(!strncmp(p, "stop", 4)){
			working_start = 0;

		} else if(!strncmp(p, "parameter", 9)){
			KERNEL_ERR("aynkey parameter counter=%d, intervalgpio=%d, intervaladc=%d, threshold14=%d, threshold56=%d, start=%d\n",
				AYNKEY_GPIO_COUNTER,
				INTERVAL_RUNNING_GPIO,
				INTERVAL_RUNNING_ADCS,
				THRESHOLD_REPORT14,
				THRESHOLD_REPORT56,
				working_start);

		} else {
			KERNEL_INFO("aynkey_write muxcmd error for cmd %s.\n", buf);
			return count;
		}
		len  = p - buf;
		len  = total - len;

		p = memchr(p, ' ', len);
		if(!p)
			break;
		while(*p == ' ')p++;
	}

    return count;
}

int aynkey_fasync(int fd, struct file *filp, int on)
{
	KERNEL_INFO("driver: aynkey_fasync\n");
	//初始化/释放 fasync_struct 结构体(fasync_struct->fa_file->f_owner->pid)
	return fasync_helper(fd, filp, on, &signal_async);
}



// File opertion 结构体，我们通过这个结构体建立应用程序到内核之间操作的映射
static struct file_operations file_oprts = {
	.owner		= THIS_MODULE,
	.read		= aynkey_read,
	.write		= aynkey_write,
	.fasync		= aynkey_fasync,
};

#define DEVICE_NAME "aynkey" // "gamepad_input";// Device 名称，对应/dev下的目录名称
static struct miscdevice aynkey_misc_device = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= DEVICE_NAME,
	.fops		= &file_oprts
};
	
/*
static int aynkey_suspend(void)
{
	bright = 0;
	KERNEL_INFO("bright %d  gpio_get_value(88) = %d \n",bright,gpio_get_value(88));
	return 0;
}


static int aynkey_resume(void)
{
	bright = 1;
	KERNEL_INFO("bright %d  gpio_get_value(88) = %d \n",bright,gpio_get_value(88));
	return 0;
}
*/

int ayn_panel_power_on(void)
{
	bright = 1;
	KERNEL_INFO("ayn_panel_power_on %d\n",bright);	
	return 0;
}
EXPORT_SYMBOL(ayn_panel_power_on);


int ayn_panel_power_off(void)
{
	bright = 0;
	usleep_range(INTERVAL_RUNNING_ADCS, INTERVAL_RUNNING_ADCS+100);
	usleep_range(INTERVAL_RUNNING_ADCS, INTERVAL_RUNNING_ADCS+100);
	KERNEL_INFO("ayn_panel_power_off %d\n",bright);
	return 0;
}
EXPORT_SYMBOL(ayn_panel_power_off);

#if 0
static int aynkey_fb_notifier_cb(struct notifier_block *self,
		unsigned long event, void *data)
{
	int *transition;
	struct fb_event *evdata = data;
	struct aynkey_data *adc_data = container_of(self, struct aynkey_data, fb_notifier);
	if (evdata && evdata->data && adc_data) {
		transition = evdata->data;
		if ((event == MSM_DRM_EARLY_EVENT_BLANK) && (adc_data->fb_ready == false)) {
			/* Resume */
			if (*transition == MSM_DRM_BLANK_UNBLANK) {
				//aynkey_resume();
				adc_data->fb_ready = true;
			}
		} else if ((event == MSM_DRM_EVENT_BLANK)&& (adc_data->fb_ready == true)) {
			/* Suspend */
			if (*transition == MSM_DRM_BLANK_POWERDOWN) {
				//aynkey_suspend();
				adc_data->fb_ready = false;
			}
		}
	}

	return 0;
}
#endif
 
static int aynkey_probe(struct platform_device *pdev)
{
	int ret = 0;
	//struct aynkey_data *data;
	//KERNEL_ERR("aynkey_probe\n");

	//data = kzalloc(sizeof(struct aynkey_data), GFP_KERNEL);
	//if (!data)
	//{
	//	KERNEL_ERR("aynkey_probe kzalloc fail\n");
	//	return -ENOMEM;
	//}
	ret = misc_register(&aynkey_misc_device);
	if (ret) {
		KERNEL_ERR("aynkey: Unable to register misc device ret=%d\n", ret);
	}
	
	//data->fb_notifier.notifier_call = aynkey_fb_notifier_cb;
	//ret = msm_drm_register_client(&data->fb_notifier);

	
	aynkey_gpio_init();
	init_start_task();

	KERNEL_INFO("\n");
	return ret;
}


/*
static struct dev_pm_ops aynkey_pm_ops = {
	.suspend	= aynkey_suspend,
	.resume		= aynkey_resume,
};
*/
static int __exit aynkey_remove(struct platform_device *pdev)
{
	KERNEL_INFO("\n");
	destroy_task();
	aynkey_gpio_free();

	misc_deregister(&aynkey_misc_device);
	return 0;
}

static struct platform_driver aynkey_driver = {

	.driver = {
		.owner	= THIS_MODULE,
		.name	= "aynkey_dev",
		//.pm		= &aynkey_pm_ops,
	},
	
	.remove		= aynkey_remove,
	.probe		= aynkey_probe,
};

static struct platform_device aynkey_dev = {
	.name		= "aynkey_dev",
	.id 		= -1,
};

int __init aynkey_init(void)
{
	platform_device_register(&aynkey_dev);
	platform_driver_register(&aynkey_driver);
	KERNEL_INFO("aynkey driver init success!!\n");
	return 0;
}



void __exit aynkey_exit(void)
{
	platform_device_unregister(&aynkey_dev);
	platform_driver_unregister(&aynkey_driver);
	KERNEL_INFO("aynkey driver %s removed\n", DEVICE_NAME);
}

//module_init(aynkey_init);
late_initcall(aynkey_init);

module_exit(aynkey_exit);

MODULE_AUTHOR("kevin");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AYN ODIN key and ADC driver");
