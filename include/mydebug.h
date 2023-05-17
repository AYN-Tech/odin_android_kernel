/*
 * mydebug.h
 *
 * ============================================================================
 * Copyright (c)  2011 Liuwen
 *
 * Use of this software is controlled by the terms and conditions found in the
 * license agreement under which this software has been supplied or provided.
 * ============================================================================
 */
#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

//GPIO的文件
#include <linux/io.h>
#include <linux/of_gpio.h>

// 音效ic

// 触摸


#define KERNEL_PRINTK 1
#define KERNEL_PROCESS 0
#define UNWIND_BACKTRACE 0


#ifdef KERNEL_GLOBALS
	unsigned int all_times;//no define static, static only at the some file.
	EXPORT_SYMBOL(all_times);
#else
	extern unsigned int all_times;
#endif // KERNEL_GLOBALS



#define MY_FILE (__FILE__ + 32)
//#define MY_FILE __FILE__


//	static unsigned int file_times;

#if UNWIND_BACKTRACE
	#ifdef KERNEL_MODULE
		#define MY_UNWIND_BACKTRACE()
	#else
		extern void my_unwind_backtrace(struct pt_regs *, struct task_struct *);//my_unwind_backtrace(NULL, NULL);
		#define MY_UNWIND_BACKTRACE() my_unwind_backtrace(NULL, NULL)
	#endif
#else
	#define MY_UNWIND_BACKTRACE()
#endif /*UNWIND_BACKTRACE*/





#if KERNEL_PROCESS
	#include <linux/sched.h>
	//#include <asm/current.h>
	#include <asm/thread_info.h>//arch/arm/include/asm/thread_info.h
	#define cpu_context  current_thread_info()->cpu_context

	#define process_info(level) \
			do{ \
				printk(level "state=%s, Process_num=%d, pid=%d, tgid=%d, syscall=%d, cpu=%d, \
cpu_context.pc=0x%x, r4=0x%x, r5=0x%x, r6=0x%x, r7=0x%x, r8=0x%x, r9=0x%x, sl=0x%x, fp=0x%x, sp=0x%x, extra[0]=0x%x, extra[1]=0x%x", \
					(current->state == -1)?"unrunnable":(current->state > -1 && current->state == 0)? "runnable":"stopped" ,\
					current->flags, current->pid, current->tgid, current_thread_info()->syscall,\
					current_thread_info()->cpu, cpu_context.pc, \
					cpu_context.r4, cpu_context.r5, \
					cpu_context.r6, cpu_context.r7, \
					cpu_context.r8, cpu_context.r9, \
					cpu_context.sl, cpu_context.fp, \
					cpu_context.sp, cpu_context.extra[0], cpu_context.extra[1]); \
			}while(0)
			//cpu_context.cpu_context.extra
#else
	#define process_info(level) do { } while(0)
#endif // KERNEL_PROCESS








#define driver_printk(level, format, args...)  \
			do{ \
				printk(level "Odin [%s:%s():%d]" format, \
							MY_FILE, __FUNCTION__, __LINE__, ##args); \
				process_info(level); \
				MY_UNWIND_BACKTRACE(); \
				printk(level "\n"); \
			}while(0)







#if KERNEL_PRINTK
#define KERNEL_DEBUG(format , args...)		driver_printk(KERN_DEBUG, format, ## args)
#else
#define KERNEL_DEBUG(format , args...)
#endif

#define KERNEL_INFO(format , args...)		driver_printk(KERN_INFO, format, ## args)
#define KERNEL_WARN(format , args...)		driver_printk(KERN_WARNING, format, ## args)
#define KERNEL_ERROR(format , args...)		driver_printk(KERN_ERR, format, ## args)

#endif// KERNEL_DEBUG_H

/*
详细了解printk后，发现还有更简便的方法。

%p：打印裸指针(raw pointer)

%pF可打印函数指针的函数名和偏移地址

%pf只打印函数指针的函数名，不打印偏移地址。

如
printk("%pf",func[0]->action); 结果：
my_Set

%pM打印冒号分隔的MAC地址

%pm打印MAC地址的16进制无分隔

如
printk("%pM %pm\n", mac, mac) willprint:

2c:00:1d:00:1b:00 2c001d001b00

%I4打印无前导0的IPv4地址，%i4打印冒号分隔的IPv4地址
%I6打印无前导0的IPv6地址，%i6打印冒号分隔的IPv6地址
如
printk("%pI4 %pi4\n", ip, ip) will print:
127.0.0.1 127:0:0:1



*/


//#define KERNEL_GLOBALS
//#include <mydebug.h>
//KERNEL_DEBUG("\n\n");
