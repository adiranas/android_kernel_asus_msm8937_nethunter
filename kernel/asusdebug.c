/* //20100930 jack_wong for asus debug mechanisms +++++
 *  asusdebug.c
 * //20100930 jack_wong for asus debug mechanisms -----
 *
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>
#include <linux/list.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/gpio.h>
extern int g_user_dbg_mode;/* ASUS_BSP Freeman +++ */

//add dump_boot_reasons ++++
#include <soc/qcom/smem.h>
//add dump_boot_reasons ----

#include "locking/rtmutex_common.h"
// ASUS_BSP +++
char evtlog_bootup_reason[100];
char evtlog_poweroff_reason[100];
char evtlog_warm_reset_reason[100];
extern u16 warm_reset_value;
// ASUS_BSP ---

#ifdef CONFIG_HAS_EARLYSUSPEND
int entering_suspend = 0;
#endif
phys_addr_t PRINTK_BUFFER_PA = 0x8FE00000;
void *PRINTK_BUFFER_VA;
extern struct timezone sys_tz;
extern int g_gpio_asus_debug;
#define RT_MUTEX_HAS_WAITERS	1UL
#define RT_MUTEX_OWNER_MASKALL	1UL
struct mutex fake_mutex;
struct completion fake_completion;
struct rt_mutex fake_rtmutex;
int asus_rtc_read_time(struct rtc_time *tm)
{
    struct timespec ts;
    getnstimeofday(&ts);
    ts.tv_sec -= sys_tz.tz_minuteswest * 60;
    rtc_time_to_tm(ts.tv_sec, tm);
    printk("now %04d%02d%02d-%02d%02d%02d, tz=%d\r\n", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, sys_tz.tz_minuteswest);
    return 0;
}
EXPORT_SYMBOL(asus_rtc_read_time);
#if 1
//--------------------   debug message logger   ------------------------------------
struct workqueue_struct *ASUSDebugMsg_workQueue;
EXPORT_SYMBOL(ASUSDebugMsg_workQueue);
//--------------  phone hang log part  --------------------------------------------------
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//                             all thread information
///////////////////////////////////////////////////////////////////////////////////////////////////


/*
 * memset for non cached memory
 */
void *memset_nc(void *s, int c, size_t count)
{
	u8 *p = s;
	while (count--)
		*p++ = c;
	return s;
}
EXPORT_SYMBOL(memset_nc);

static char* g_phonehang_log;
static int g_iPtr = 0;
int save_log(const char *f, ...)
{
    va_list args;
    int len;

    if (g_iPtr < PHONE_HANG_LOG_SIZE)
    {
        va_start(args, f);
        len = vsnprintf(g_phonehang_log + g_iPtr, PHONE_HANG_LOG_SIZE - g_iPtr, f, args);
        va_end(args);
        //printk("%s", g_phonehang_log + g_iPtr);
        if (g_iPtr < PHONE_HANG_LOG_SIZE)
        {
            g_iPtr += len;
            return 0;
        }
    }
    g_iPtr = PHONE_HANG_LOG_SIZE;
    return -1;
}

static char *task_state_array[] = {
    "RUNNING",      /*  0 */
    "INTERRUPTIBLE",     /*  1 */
    "UNINTERRUPTIB",   /*  2 */
    "STOPPED",      /*  4 */
    "TRACED", /*  8 */
    "EXIT ZOMBIE",       /* 16 */
    "EXIT DEAD",      /* 32 */
    "DEAD",      /* 64 */
    "WAKEKILL",      /* 128 */
    "WAKING"      /* 256 */
};
struct thread_info_save;
struct thread_info_save
{
    struct task_struct *pts;
    pid_t pid;
    u64 sum_exec_runtime;
    u64 vruntime;
    struct thread_info_save* pnext;
};
static char * print_state(long state)
{
    int i;
    if(state == 0)
        return task_state_array[0];
    for(i = 1; i <= 16; i++)
    {
        if(1<<(i-1) & state)
            return task_state_array[i];
    }
    return "NOTFOUND";

}

/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
    if ((long long)nsec < 0) {
        nsec = -nsec;
        do_div(nsec, 1000000);
        return -nsec;
    }
    do_div(nsec, 1000000);

    return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
    unsigned long long nsec1;
    if ((long long)nsec < 0)
        nsec = -nsec;

    nsec1 =  do_div(nsec, 1000000);
    return do_div(nsec1, 1000000);
}
#define MAX_STACK_TRACE_DEPTH   64
struct stack_trace_data {
    struct stack_trace *trace;
    unsigned int no_sched_functions;
    unsigned int skip;
};

struct stackframe {
    unsigned long fp;
    unsigned long sp;
    unsigned long lr;
    unsigned long pc;
};
int unwind_frame(struct stackframe *frame);
void notrace walk_stackframe(struct stackframe *frame,
             int (*fn)(struct stackframe *, void *), void *data);

void save_stack_trace_asus(struct task_struct *tsk, struct stack_trace *trace);
void show_stack1(struct task_struct *p1, void *p2)
{
    struct stack_trace trace;
    unsigned long *entries;
    int i;

    entries = kmalloc(MAX_STACK_TRACE_DEPTH * sizeof(*entries), GFP_KERNEL);
    if (!entries)
    {
        printk("[ASDF]entries malloc failure\n");
        return;
    }
    trace.nr_entries    = 0;
    trace.max_entries   = MAX_STACK_TRACE_DEPTH;
    trace.entries       = entries;
    trace.skip      = 0;
    save_stack_trace_asus(p1, &trace);

    for (i = 0; i < trace.nr_entries; i++)
    {
        save_log("[<%p>] %pS\n", (void *)entries[i], (void *)entries[i]);
    }
    kfree(entries);
}


#define SPLIT_NS(x) nsec_high(x), nsec_low(x)
void print_all_thread_info(void)
{
    struct task_struct *pts;
    struct thread_info *pti;
    struct rtc_time tm;
    asus_rtc_read_time(&tm);

    #if 1
    g_phonehang_log = (char*)PHONE_HANG_LOG_BUFFER;//phys_to_virt(PHONE_HANG_LOG_BUFFER);
    g_iPtr = 0;
    //printk("%s %u  g_phonehang_log=%p\n", __FUNCTION__, __LINE__, g_phonehang_log);
    memset_nc(g_phonehang_log, 0, PHONE_HANG_LOG_SIZE);
    #endif

    save_log("PhoneHang-%04d%02d%02d-%02d%02d%02d.txt  ---  ASUS_SW_VER : %s----------------------------------------------\r\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ASUS_SW_VER);
    save_log(" pID----ppID----NAME----------------SumTime---vruntime--SPri-NPri-State----------PmpCnt-Binder----Waiting\r\n");

    for_each_process(pts)
    {
        pti = task_thread_info(pts);
        save_log("-----------------------------------------------------\r\n");
        save_log(" %-7d", pts->pid);

        if(pts->parent)
            save_log("%-8d", pts->parent->pid);
        else
            save_log("%-8d", 0);

        save_log("%-20s", pts->comm);
        save_log("%lld.%06ld", SPLIT_NS(pts->se.sum_exec_runtime));
        save_log("     %lld.%06ld     ", SPLIT_NS(pts->se.vruntime));
        save_log("%-5d", pts->static_prio);
        save_log("%-5d", pts->normal_prio);
        save_log("%-15s", print_state((pts->state & TASK_REPORT) | pts->exit_state));

#ifndef ASUS_SHIP_BUILD
//        save_log("%-6d", pts->binder_call_to_proc_pid);
#endif
        save_log("%-6d", pti->preempt_count);


        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("[ASDF]    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->name == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk("[ASDF] Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk("[ASDF] %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
		}

		if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
		{
			if (pti->pWaitingCompletion->name)
	            save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
	        else
				printk("[ASDF]pti->pWaitingCompletion->name == NULL\r\n");
		}

        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("[ASDF]pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->comm);
			else
				printk("[ASDF]pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}

	save_log("\r\n");
        show_stack1(pts, NULL);

        save_log("\r\n");

        if(!thread_group_empty(pts))
        {
            struct task_struct *p1 = next_thread(pts);
            do
            {
                pti = task_thread_info(p1);
                save_log(" %-7d", p1->pid);

                if(pts->parent)
                    save_log("%-8d", p1->parent->pid);
                else
                    save_log("%-8d", 0);

                save_log("%-20s", p1->comm);
                save_log("%lld.%06ld", SPLIT_NS(p1->se.sum_exec_runtime));
                save_log("     %lld.%06ld     ", SPLIT_NS(p1->se.vruntime));
                save_log("%-5d", p1->static_prio);
                save_log("%-5d", p1->normal_prio);
                save_log("%-15s", print_state((p1->state & TASK_REPORT) | p1->exit_state));
#ifndef ASUS_SHIP_BUILD
//                save_log("%-6d", pts->binder_call_to_proc_pid);
#endif
                save_log("%-6d", pti->preempt_count);

        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("[ASDF]    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->name == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk("[ASDF] Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk("[ASDF] %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
		}

		if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
		{
			if (pti->pWaitingCompletion->name)
	            save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
	        else
				printk("[ASDF]pti->pWaitingCompletion->name == NULL\r\n");
		}

        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("[ASDF]pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->comm);
			else
				printk("[ASDF]pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}
                save_log("\r\n");
                show_stack1(p1, NULL);

                save_log("\r\n");
                p1 = next_thread(p1);
            }while(p1 != pts);
        }
        save_log("-----------------------------------------------------\r\n\r\n\r\n");

    }
    save_log("\r\n\r\n\r\n\r\n");


    #if 1
    //iounmap(g_phonehang_log);
    #endif
}

struct thread_info_save *ptis_head = NULL;
int find_thread_info(struct task_struct *pts, int force)
{
    struct thread_info *pti;
    struct thread_info_save *ptis, *ptis_ptr;
    u64 vruntime = 0, sum_exec_runtime;

    if(ptis_head != NULL)
    {
        ptis = ptis_head->pnext;
        ptis_ptr = NULL;
        //printk("[ASDF]initial ptis %x,\n\r", ptis);
        while(ptis)
        {
            //printk("[ASDF]initial ptis->pts %x, pts=%x\n\r", ptis->pts, pts);
            if(ptis->pid == pts->pid && ptis->pts == pts)
            {
                //printk("[ASDF]found pts=%x\n\r", pts);
                ptis_ptr = ptis;
                break;
            }
            ptis = ptis->pnext;
        }
        //printk("[ASDF]ptis_ptr=%x\n\r", ptis_ptr);
        if(ptis_ptr)
        {
            //printk("[ASDF]found ptis->pid=%d, sum_exec_runtime  new:%lld.%06ld  old:%lld.%06ld \n\r", ptis->pid, SPLIT_NS(pts->se.sum_exec_runtime), SPLIT_NS(ptis->sum_exec_runtime));
            sum_exec_runtime = pts->se.sum_exec_runtime - ptis->sum_exec_runtime;
            //printk("[ASDF]difference=%lld.%06ld  \n\r", SPLIT_NS(sum_exec_runtime));
        }
        else
        {
            sum_exec_runtime = pts->se.sum_exec_runtime;
        }
        //printk("[ASDF]utime=%d, stime=%d\n\r", utime, stime);
        if(sum_exec_runtime > 0 || force)
        {
            pti = task_thread_info(pts);
            save_log(" %-7d", pts->pid);

            if(pts->parent)
                save_log("%-8d", pts->parent->pid);
            else
                save_log("%-8d", 0);

            save_log("%-20s", pts->comm);
            save_log("%lld.%06ld", SPLIT_NS(sum_exec_runtime));
            if(nsec_high(sum_exec_runtime) > 1000)
                save_log(" ******");
            save_log("     %lld.%06ld     ", SPLIT_NS(vruntime));
            save_log("%-5d", pts->static_prio);
            save_log("%-5d", pts->normal_prio);
            save_log("%-15s", print_state(pts->state));
            save_log("%-6d", pti->preempt_count);

        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("[ASDF]    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->name == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk("[ASDF] Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk("[ASDF] %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
		}

            if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
			{
				if (pti->pWaitingCompletion->name)
					save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
				else
					printk("[ASDF]pti->pWaitingCompletion->name == NULL\r\n");
			}

        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("[ASDF]pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->comm);
			else
				printk("[ASDF]pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}

            save_log("\r\n");

            show_stack1(pts, NULL);
            save_log("\r\n");
        }
        else
            return 0;
    }
    return 1;

}

void save_all_thread_info(void)
{
    struct task_struct *pts;
    struct thread_info *pti;
    struct thread_info_save *ptis = NULL, *ptis_ptr = NULL;
#ifndef ASUS_SHIP_BUILD
//    struct worker *pworker ;
#endif

    struct rtc_time tm;
    asus_rtc_read_time(&tm);
    #if 1
    g_phonehang_log = (char*)PHONE_HANG_LOG_BUFFER;//phys_to_virt(PHONE_HANG_LOG_BUFFER);
    g_iPtr = 0;
    //printk("%s %u  g_phonehang_log=%p\n", __FUNCTION__, __LINE__, g_phonehang_log);
    memset_nc(g_phonehang_log, 0, PHONE_HANG_LOG_SIZE);
    #endif

    save_log("ASUSSlowg-%04d%02d%02d-%02d%02d%02d.txt  ---  ASUS_SW_VER : %s----------------------------------------------\r\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ASUS_SW_VER);
    save_log(" pID----ppID----NAME----------------SumTime---vruntime--SPri-NPri-State----------PmpCnt-binder----Waiting\r\n");


    if(ptis_head != NULL)
    {
        struct thread_info_save *ptis_next = ptis_head->pnext;
        struct thread_info_save *ptis_next_next;
        while(ptis_next)
        {
            ptis_next_next = ptis_next->pnext;
            kfree(ptis_next);
            ptis_next = ptis_next_next;
        }
        kfree(ptis_head);
        ptis_head = NULL;
    }

    if(ptis_head == NULL)
    {
        ptis_ptr = ptis_head = kmalloc(sizeof( struct thread_info_save), GFP_KERNEL);
        if(!ptis_head)
        {
            printk("[ASDF]kmalloc ptis_head failure\n");
            return;
        }
        memset(ptis_head, 0, sizeof( struct thread_info_save));
    }

    for_each_process(pts)
    {
        pti = task_thread_info(pts);
        //printk("[ASDF]for pts %x, ptis_ptr=%x\n\r", pts, ptis_ptr);
        ptis = kmalloc(sizeof( struct thread_info_save), GFP_KERNEL);
        if(!ptis)
        {
            printk("[ASDF]kmalloc ptis failure\n");
            return;
        }
        memset(ptis, 0, sizeof( struct thread_info_save));

        save_log("-----------------------------------------------------\r\n");
        save_log(" %-7d", pts->pid);
        if(pts->parent)
            save_log("%-8d", pts->parent->pid);
        else
            save_log("%-8d", 0);

        save_log("%-20s", pts->comm);
        save_log("%lld.%06ld", SPLIT_NS(pts->se.sum_exec_runtime));
        save_log("     %lld.%06ld     ", SPLIT_NS(pts->se.vruntime));
        save_log("%-5d", pts->static_prio);
        save_log("%-5d", pts->normal_prio);
        save_log("%-15s", print_state((pts->state & TASK_REPORT) | pts->exit_state));
#ifndef ASUS_SHIP_BUILD
//        save_log("call_proc:%-6d  ", pts->binder_call_to_proc_pid);
 //       save_log("call_thread:%-6d  ", pts->binder_call_to_thread_pid);
 //       save_log("call_code:%-6d  ", pts->binder_call_code);
#endif
        save_log("%-6d", pti->preempt_count);

        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("[ASDF]    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->name == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk("[ASDF] Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk("[ASDF] %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
		}
	if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
	{
		if (pti->pWaitingCompletion->name)
			save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
		else
			printk("[ASDF]pti->pWaitingCompletion->name == NULL\r\n");
	}

        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("[ASDF]pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->comm);
			else
				printk("[ASDF]pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}


        save_log("\r\n");
        show_stack1(pts, NULL);

        save_log("\r\n");


        ptis->pid = pts->pid;
        ptis->pts = pts;
        ptis->sum_exec_runtime = pts->se.sum_exec_runtime;
        //printk("[ASDF]saving %d, sum_exec_runtime  %lld.%06ld  \n\r", ptis->pid, SPLIT_NS(ptis->sum_exec_runtime));
        ptis->vruntime = pts->se.vruntime;
        //printk("[ASDF]newing ptis %x utime=%d, stime=%d\n\r", ptis, pts->utime, pts->stime);

        ptis_ptr->pnext = ptis;
        ptis_ptr = ptis;

        if(!thread_group_empty(pts))
        {
            struct task_struct *p1 = next_thread(pts);
            do
            {
                pti = task_thread_info(p1);
                //printk("[ASDF]for pts %x, ptis_ptr=%x\n\r", pts, ptis_ptr);
                ptis = kmalloc(sizeof( struct thread_info_save), GFP_KERNEL);
                if(!ptis)
                {
                    printk("[ASDF]kmalloc ptis 2 failure\n");
                    return;
                }
                memset(ptis, 0, sizeof( struct thread_info_save));

                ptis->pid = p1->pid;
                ptis->pts = p1;
                ptis->sum_exec_runtime = p1->se.sum_exec_runtime;
                //printk("[ASDF]saving %d, sum_exec_runtime  %lld.%06ld  \n\r", ptis->pid, SPLIT_NS(ptis->sum_exec_runtime));
                ptis->vruntime = p1->se.vruntime;
                //printk("[ASDF]newing ptis %x utime=%d, stime=%d\n\r", ptis, pts->utime, pts->stime);

                ptis_ptr->pnext = ptis;
                ptis_ptr = ptis;
                save_log(" %-7d", p1->pid);

                if(pts->parent)
                    save_log("%-8d", p1->parent->pid);
                else
                    save_log("%-8d", 0);

                save_log("%-20s", p1->comm);
                save_log("%lld.%06ld", SPLIT_NS(p1->se.sum_exec_runtime));
                save_log("     %lld.%06ld     ", SPLIT_NS(p1->se.vruntime));
                save_log("%-5d", p1->static_prio);
                save_log("%-5d", p1->normal_prio);
                save_log("%-15s", print_state((p1->state & TASK_REPORT) | p1->exit_state));
#ifndef ASUS_SHIP_BUILD
//                save_log("call_proc:%-6d  ", pts->binder_call_to_proc_pid);
 //               save_log("call_thread:%-6d  ", pts->binder_call_to_thread_pid);
 //               save_log("call_code:%-6d  ", pts->binder_call_code);
#endif
                save_log("%-6d", pti->preempt_count);

        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("[ASDF]    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->name == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk("[ASDF] Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");

			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk("[ASDF] %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("[ASDF]pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
		}

		if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
		{
			if (pti->pWaitingCompletion->name)
	            save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
	        else
				printk("[ASDF]pti->pWaitingCompletion->name == NULL\r\n");
		}

        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("[ASDF]pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->comm);
			else
				printk("[ASDF]pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}
                save_log("\r\n");
                show_stack1(p1, NULL);

                save_log("\r\n");

                p1 = next_thread(p1);
            }while(p1 != pts);
        }


    }

}

EXPORT_SYMBOL(save_all_thread_info);

void delta_all_thread_info(void)
{
    struct task_struct *pts;
    int ret = 0, ret2 = 0;

    save_log("\r\nDELTA INFO----------------------------------------------------------------------------------------------\r\n");
    save_log(" pID----ppID----NAME----------------SumTime---vruntime--SPri-NPri-State----------PmpCnt----Waiting\r\n");
    for_each_process(pts)
    {
        //printk("[ASDF]for pts %x,\n\r", pts);
        ret = find_thread_info(pts, 0);
        if(!thread_group_empty(pts))
        {
            struct task_struct *p1 = next_thread(pts);
            ret2 = 0;
            do
            {
                ret2 += find_thread_info(p1, 0);
                p1 = next_thread(p1);
            }while(p1 != pts);
            if(ret2 && !ret)
                find_thread_info(pts, 1);
        }
        if(ret || ret2)
            save_log("-----------------------------------------------------\r\n\r\n-----------------------------------------------------\r\n");
    }
    save_log("\r\n\r\n\r\n\r\n");
}
EXPORT_SYMBOL(delta_all_thread_info);
///////////////////////////////////////////////////////////////////////////////////////////////////
void printk_buffer_rebase(void);
static mm_segment_t oldfs;
static void initKernelEnv(void)
{
    oldfs = get_fs();
    set_fs(KERNEL_DS);
}

static void deinitKernelEnv(void)
{
    set_fs(oldfs);
}


//ASUS_BSP+++  "[ZC550KL][TRACE][Na] add rule to trige trace"
int asus_trace_trige_state = 0;
struct switch_dev asus_trace_trige_switch;
static ssize_t asus_trace_trige_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "trace_trige\n");
}

static ssize_t asus_trace_trige_switch_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", asus_trace_trige_state);
}

static int AsusTraceTrigeInitialize(void)
{
	int ret = 0;
	printk("[TRACE] %s: register switch dev! %d\n", __FUNCTION__, ret);
	asus_trace_trige_switch.name = "tracetrige";
	asus_trace_trige_switch.print_state = asus_trace_trige_switch_state;
	asus_trace_trige_switch.print_name = asus_trace_trige_name;
	ret = switch_dev_register(&asus_trace_trige_switch);
	if (ret < 0) {
	    printk("[TRACE] %s: Unable to register switch dev! %d\n", __FUNCTION__, ret);
	    return -1;
	}
	return 0;
}
//ASUS_BSP--- "[ZC550KL][TRACE][Na] add rule to trige trace"
char messages[256];
void save_phone_hang_log(void)
{
    int file_handle;
    int ret;
    //---------------saving phone hang log if any -------------------------------
    g_phonehang_log = (char*)PHONE_HANG_LOG_BUFFER;
    printk("[ASDF]save_phone_hang_log PRINTK_BUFFER=%p, PHONE_HANG_LOG_BUFFER=%p\n", PRINTK_BUFFER_VA, PHONE_HANG_LOG_BUFFER);
    if(g_phonehang_log && ((strncmp(g_phonehang_log, "PhoneHang", 9) == 0) || (strncmp(g_phonehang_log, "ASUSSlowg", 9) == 0)) )
    {
        printk("[ASDF]save_phone_hang_log-1\n");
        initKernelEnv();
        memset(messages, 0, sizeof(messages));
        strcpy(messages, ASUS_ASDF_BASE_DIR);
        strncat(messages, g_phonehang_log, 29);
        file_handle = sys_open(messages, O_CREAT|O_WRONLY|O_SYNC, 0);
        printk("[ASDF]save_phone_hang_log-2 file_handle %d, name=%s\n", file_handle, messages);
        if(!IS_ERR((const void *)(ulong)file_handle))
        {
            ret = sys_write(file_handle, (unsigned char*)g_phonehang_log, strlen(g_phonehang_log));
            sys_close(file_handle);
        }
        deinitKernelEnv();

    }
    if(g_phonehang_log && file_handle >0 && ret >0)
    {
        g_phonehang_log[0] = 0;
        //iounmap(g_phonehang_log);
    }
	printk("triger trace %d\n", asus_trace_trige_state);
	asus_trace_trige_state = !asus_trace_trige_state;
	switch_set_state(&asus_trace_trige_switch, asus_trace_trige_state);
}
EXPORT_SYMBOL(save_phone_hang_log);
void save_last_shutdown_log(char* filename)
{
    char *last_shutdown_log;
    int file_handle;
    unsigned long long t;
    unsigned long nanosec_rem;
	// ASUS_BSP +++
    char buffer[] = {"Kernel panic"};
    int i;
    // ASUS_BSP ---

    t = cpu_clock(0);
	nanosec_rem = do_div(t, 1000000000);
    last_shutdown_log = (char*)PRINTK_BUFFER_VA;
	sprintf(messages, ASUS_ASDF_BASE_DIR"LastShutdown_%lu.%06lu.txt",
				(unsigned long) t,
				nanosec_rem / 1000);

    initKernelEnv();
    file_handle = sys_open(messages, O_CREAT|O_RDWR|O_SYNC, 0);
    if(!IS_ERR((const void *)(ulong)file_handle))
    {
        sys_write(file_handle, (unsigned char*)last_shutdown_log, PRINTK_BUFFER_SLOT_SIZE);
        sys_close(file_handle);
        // ASUS_BSP +++
        for(i=0; i<PRINTK_BUFFER_SLOT_SIZE; i++) {
            //
            // Check if it is kernel panic
            //
            if (strncmp((last_shutdown_log + i), buffer, strlen(buffer)) == 0) {
                ASUSEvtlog("[Reboot] Kernel panic\n");
                break;
            }
        }
        // ASUS_BSP ---
    } else {
		printk("[ASDF] save_last_shutdown_error: [%d]\n", file_handle);
	}
    deinitKernelEnv();

}

void get_last_shutdown_log(void)
{
    ulong *printk_buffer_slot2_addr;

    printk_buffer_slot2_addr = (ulong *)PRINTK_BUFFER_SLOT2;
    printk("[ASDF]get_last_shutdown_log: printk_buffer_slot2=%p, value=0x%lx\n", printk_buffer_slot2_addr, *printk_buffer_slot2_addr);
    if(*printk_buffer_slot2_addr == (ulong)PRINTK_BUFFER_MAGIC) {
        save_last_shutdown_log("LastShutdown");
    }
    printk_buffer_rebase();
}
EXPORT_SYMBOL(get_last_shutdown_log);

extern int nSuspendInProgress;
static struct workqueue_struct *ASUSEvtlog_workQueue;
static int g_hfileEvtlog = -MAX_ERRNO;
static int g_bEventlogEnable = 1;
static char g_Asus_Eventlog[ASUS_EVTLOG_MAX_ITEM][ASUS_EVTLOG_STR_MAXLEN];
static int g_Asus_Eventlog_read = 0;
static int g_Asus_Eventlog_write = 0;

static void do_write_event_worker(struct work_struct *work);
static DECLARE_WORK(eventLog_Work, do_write_event_worker);

//add dump_boot_reasons ++++
extern void asus_dump_bootup_reason(char *bootup_reason);

static void dump_boot_reasons(void)
{
	char buffer[256] = {0};
	unsigned smem_size = 0;
	unsigned char* pmic_boot_reasons = NULL;

	pmic_boot_reasons = (unsigned char *)(smem_get_entry(SMEM_POWER_ON_STATUS_INFO, &smem_size,false,true));
	if(NULL == pmic_boot_reasons || smem_size < 8)
	{
		printk(KERN_ERR "%s get boot reasons failed.", __func__);
		return;
	}

	snprintf(buffer, 255, "PMIC Boot Reasons:%02X %02X %02X %02X %02X %02X %02X %02X\n",
			 pmic_boot_reasons[0], pmic_boot_reasons[1], pmic_boot_reasons[2], pmic_boot_reasons[3],
			 pmic_boot_reasons[4], pmic_boot_reasons[5], pmic_boot_reasons[6], pmic_boot_reasons[7]);

	printk(KERN_NOTICE "%s", buffer);
	sys_write(g_hfileEvtlog, buffer, strlen(buffer));

	memset(buffer, 0, strlen(buffer));
	asus_dump_bootup_reason(buffer);
	sys_write(g_hfileEvtlog, buffer, strlen(buffer));
}
//add dump_boot_reasons ----

static struct mutex mA;
#define AID_SDCARD_RW 1015
extern int boot_delay_complete;
static void do_write_event_worker(struct work_struct *work)
{
	char buffer[256];
	bool open_file_fail;

	memset(buffer, 0, sizeof(char)*256);

	//printk("[ASDF] enter %s\n",__func__);

	if(boot_delay_complete == 0)
	{
		//printk(" boot_after_10sec  fail \n");
		goto write_event_out;
	}

	if (IS_ERR((const void *)(ulong)g_hfileEvtlog)) {
		long size;
		g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0444);
		printk("g_hfileEvtlog => %d \n",g_hfileEvtlog);
		if(g_hfileEvtlog < 0)
		{
			open_file_fail = true;
			goto write_event_out;
		}
		else
		{
			open_file_fail = false;
		}

		sys_chown(ASUS_EVTLOG_PATH".txt", AID_SDCARD_RW, AID_SDCARD_RW);

		size = sys_lseek(g_hfileEvtlog, 0, SEEK_END);

		if (size >= SZ_2M) {
			sys_close(g_hfileEvtlog);
			sys_rmdir(ASUS_EVTLOG_PATH"_old.txt");
			sys_rename(ASUS_EVTLOG_PATH".txt", ASUS_EVTLOG_PATH"_old.txt");
			g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0444);
		}
        // ASUS_BSP +++
        if (warm_reset_value) {
    		snprintf(buffer, sizeof(buffer),
                "\n\n---------------System Boot----%s---------\n"
                "[Reboot] Warm reset Reason: %s ###### \n"
                "###### Bootup Reason: %s ######\n",
                ASUS_SW_VER,
                evtlog_warm_reset_reason,
                evtlog_bootup_reason);

        } else {
    		snprintf(buffer, sizeof(buffer),
                "\n\n---------------System Boot----%s---------\n"
                "[Shutdown] Power off Reason: %s ###### \n"
                "###### Bootup Reason: %s ######\n",
                ASUS_SW_VER,
                evtlog_poweroff_reason,
                evtlog_bootup_reason);
        }
        // ASUS_BSP ---

		sys_write(g_hfileEvtlog, buffer, strlen(buffer));

		sys_fsync(g_hfileEvtlog);
		//add dump_boot_reasons ++++
		if(!IS_ERR((const void*)(ulong)g_hfileEvtlog))
		{
			dump_boot_reasons();
		}
		//add dump_boot_reasons ----
		sys_close(g_hfileEvtlog);
	}

	if(open_file_fail)
	{
		printk("open ASUS_EVTLOG_PATH file fail \n");
		goto write_event_out;
	}

	if (!IS_ERR((const void *)(ulong)g_hfileEvtlog)) {
		int str_len;
		char *pchar;
		long size;
		g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0444);
		sys_chown(ASUS_EVTLOG_PATH".txt", AID_SDCARD_RW, AID_SDCARD_RW);

		size = sys_lseek(g_hfileEvtlog, 0, SEEK_END);

		if (size >= SZ_2M) {
			sys_close(g_hfileEvtlog);
			sys_rmdir(ASUS_EVTLOG_PATH"_old.txt");
			sys_rename(ASUS_EVTLOG_PATH".txt", ASUS_EVTLOG_PATH"_old.txt");
			g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0444);
		}

		while (g_Asus_Eventlog_read != g_Asus_Eventlog_write) {
			mutex_lock(&mA);

			str_len = strlen(g_Asus_Eventlog[g_Asus_Eventlog_read]);
			pchar = g_Asus_Eventlog[g_Asus_Eventlog_read];

			g_Asus_Eventlog_read++;
			g_Asus_Eventlog_read %= ASUS_EVTLOG_MAX_ITEM;
			mutex_unlock(&mA);
			if (pchar[str_len - 1] != '\n' ) {
				if(str_len + 1 >= ASUS_EVTLOG_STR_MAXLEN)
					str_len = ASUS_EVTLOG_STR_MAXLEN - 2;
				pchar[str_len] = '\n';
				pchar[str_len + 1] = '\0';
			}

			sys_write(g_hfileEvtlog, pchar, strlen(pchar));
			sys_fsync(g_hfileEvtlog);
		}
		sys_close(g_hfileEvtlog);
	}

write_event_out:
	if(open_file_fail)
		printk("open ASUS_EVTLOG_PATH file fail \n");

}

extern struct timezone sys_tz;

void ASUSEvtlog(const char *fmt, ...)
{

	va_list args;
	char *buffer;

	if (g_bEventlogEnable == 0)
		return;
	if (!in_interrupt() && !in_atomic() && !irqs_disabled())
		mutex_lock(&mA);

	buffer = g_Asus_Eventlog[g_Asus_Eventlog_write];
	g_Asus_Eventlog_write++;
	g_Asus_Eventlog_write %= ASUS_EVTLOG_MAX_ITEM;

	if (!in_interrupt() && !in_atomic() && !irqs_disabled())
		mutex_unlock(&mA);

	memset(buffer, 0, ASUS_EVTLOG_STR_MAXLEN);
	if (buffer) {
		struct rtc_time tm;
		struct timespec ts;

		getnstimeofday(&ts);
		ts.tv_sec -= sys_tz.tz_minuteswest * 60;
		rtc_time_to_tm(ts.tv_sec, &tm);
		getrawmonotonic(&ts);
		sprintf(buffer, "(%ld)%04d-%02d-%02d %02d:%02d:%02d :", ts.tv_sec, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		/*printk(buffer);*/
		va_start(args, fmt);
		vscnprintf(buffer + strlen(buffer), ASUS_EVTLOG_STR_MAXLEN - strlen(buffer), fmt, args);
		va_end(args);
		/*printk(buffer);*/
		queue_work(ASUSEvtlog_workQueue, &eventLog_Work);
	} else {
		printk("[ASDF]ASUSEvtlog buffer cannot be allocated\n");
	}
}
EXPORT_SYMBOL(ASUSEvtlog);

static ssize_t evtlogswitch_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	if(strncmp(buf, "0", 1) == 0) {
		ASUSEvtlog("ASUSEvtlog disable !!");
		printk("ASUSEvtlog disable !!\n");
		flush_work(&eventLog_Work);
		g_bEventlogEnable = 0;
	}
	if (strncmp(buf, "1", 1) == 0) {
		g_bEventlogEnable = 1;
		ASUSEvtlog("ASUSEvtlog enable !!");
		printk("ASUSEvtlog enable !!\n");
	}

	return count;
}
static ssize_t asusevtlog_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	if (count > 256)
		count = 256;

	memset(messages, 0, sizeof(messages));
	if (copy_from_user(messages, buf, count))
		return -EFAULT;
	ASUSEvtlog("%s",messages);

	return count;
}

static int asusdebug_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int asusdebug_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t asusdebug_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	return 0;
}

extern int rtc_ready;
int asus_asdf_set = 0;
static ssize_t asusdebug_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	u8 messages[256] = {0};
	bool isBuildType_user =false;

	if (count > 256)
		count = 256;
	if (copy_from_user(messages, buf, count))
		return -EFAULT;

	if (strncmp(messages, "panic", strlen("panic")) == 0) {
		panic("panic test");
	} else if (strncmp(messages, "dbg", strlen("dbg")) == 0) {
	#ifdef ASUS_SHIP_BUILD
		isBuildType_user =true;
	#endif
		if(isBuildType_user && g_ASUS_PRJ_STAGE == STAGE_MP)
		{
			g_user_dbg_mode = 0;
			gpio_direction_output(g_gpio_asus_debug, 1); /* disable uart log, enable audio */
			printk("[ASDF] MP device user build set force Audio mode!!\n");
		}
		else{
			g_user_dbg_mode = 1;
			gpio_direction_output(g_gpio_asus_debug, 0); /* enable uart log, disable audio */
			printk("[ASDF]Kernel dbg mode = %d\n", g_user_dbg_mode);
			printk("[ASDF]g_user_dbg_mode has been set\n");
		}
	} else if(strncmp(messages, "ndbg", strlen("ndbg")) == 0) {
		g_user_dbg_mode = 0;
		gpio_direction_output(g_gpio_asus_debug, 1); /* disable uart log, enable audio */
		printk("[ASDF]Kernel dbg mode = %d\n", g_user_dbg_mode);
		printk("[ASDF]g_user_dbg_mode has been removed\n");
	} else if(strncmp(messages, "get_asdf_log",
				strlen("get_asdf_log")) == 0) {

		ulong *printk_buffer_slot2_addr;

		printk_buffer_slot2_addr = (ulong *)PRINTK_BUFFER_SLOT2;
		printk("[ASDF] printk_buffer_slot2_addr=%p, value=0x%lx\n", printk_buffer_slot2_addr, *printk_buffer_slot2_addr);

		if(!asus_asdf_set) {
			asus_asdf_set = 1;
			save_phone_hang_log();
			get_last_shutdown_log();
			printk("[ASDF] get_last_shutdown_log: printk_buffer_slot2_addr=%p, value=0x%lx\n", printk_buffer_slot2_addr, *printk_buffer_slot2_addr);
			(*printk_buffer_slot2_addr)=(ulong)PRINTK_BUFFER_MAGIC;
			//(*printk_buffer_slot2_addr)=0;
		}

	} else if(strncmp(messages, "slowlog", strlen("slowlog")) == 0) {
		printk("[ASDF]start to gi chk\n");
		save_all_thread_info();

		msleep(5 * 1000);

		printk("[ASDF]start to gi delta\n");
		delta_all_thread_info();
		save_phone_hang_log();
		return count;
	}

	return count;
}

static const struct file_operations proc_evtlogswitch_operations = {
	.write	  = evtlogswitch_write,
};
static const struct file_operations proc_asusevtlog_operations = {
	.write	  = asusevtlog_write,
};
static const struct file_operations proc_asusdebug_operations = {
	.read	   = asusdebug_read,
	.write	  = asusdebug_write,
	.open	   = asusdebug_open,
	.release	= asusdebug_release,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void asusdebug_early_suspend(struct early_suspend *h)
{
    entering_suspend = 1;
}

static void asusdebug_early_resume(struct early_suspend *h)
{
    entering_suspend = 0;
}
EXPORT_SYMBOL(entering_suspend);

struct early_suspend asusdebug_early_suspend_handler = {
    .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
    .suspend = asusdebug_early_suspend,
    .resume = asusdebug_early_resume,
};
#endif

unsigned int asusdebug_enable = 0;
unsigned int readflag = 0;
static ssize_t turnon_asusdebug_proc_read(struct file *filp, char __user *buff, size_t len, loff_t *off)
{
	char print_buf[32];
	unsigned int ret = 0,iret = 0;
	sprintf(print_buf, "asusdebug: %s\n", asusdebug_enable? "off":"on");
	ret = strlen(print_buf);
	iret = copy_to_user(buff, print_buf, ret);
	if (!readflag) {
		readflag = 1;
		return ret;
	}
	else {
		readflag = 0;
		return 0;
	}
}
static ssize_t turnon_asusdebug_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	char messages[256];
	memset(messages, 0, sizeof(messages));
	if (len > 256)
		len = 256;
	if (copy_from_user(messages, buff, len))
		return -EFAULT;
	if (strncmp(messages, "off", 3) == 0) {
		asusdebug_enable = 0x11223344;
	} else if(strncmp(messages, "on", 2) == 0) {
		asusdebug_enable = 0;
	}
	return len;
}
static struct file_operations turnon_asusdebug_proc_ops = {
	.read = turnon_asusdebug_proc_read,
	.write = turnon_asusdebug_proc_write,
 };
///////////////////////////////////////////////////////////////////////
//
// printk controller
//
///////////////////////////////////////////////////////////////////////
static int klog_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_user_klog_mode);
	return 0;
}

static int klog_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, klog_proc_show, NULL);
}


static ssize_t klog_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *pos)
{
	char lbuf[32];

	if (count >= sizeof(lbuf))
		count = sizeof(lbuf)-1;

	if (copy_from_user(lbuf, buf, count))
		return -EFAULT;
	lbuf[count] = 0;

	if(0 == strncmp(lbuf, "1", 1))
	{
		g_user_klog_mode = 1;
	}
	else
	{
		g_user_klog_mode = 0;
	}

	return count;
}

static const struct file_operations klog_proc_fops = {
	.open		= klog_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= klog_proc_write,
};
static int __init proc_asusdebug_init(void)
{
	proc_create("asusdebug", S_IALLUGO, NULL, &proc_asusdebug_operations);
	proc_create("asusevtlog", S_IRWXUGO, NULL, &proc_asusevtlog_operations);
	proc_create("asusevtlog-switch", S_IRWXUGO, NULL, &proc_evtlogswitch_operations);
	proc_create("asusdebug-switch", S_IRWXUGO, NULL, &turnon_asusdebug_proc_ops);
	proc_create_data("asusklog", S_IRWXUGO, NULL, &klog_proc_fops, NULL);
	PRINTK_BUFFER_VA = ioremap(PRINTK_BUFFER_PA, PRINTK_BUFFER_SIZE);
//printk("PRINTK_BUFFER_VA=%p\n", PRINTK_BUFFER_VA);
	mutex_init(&mA);
	fake_mutex.owner = current;
	fake_mutex.mutex_owner_asusdebug = current;
	fake_mutex.name = " fake_mutex";
	strcpy(fake_completion.name," fake_completion");
	fake_rtmutex.owner = current;
	ASUSEvtlog_workQueue  = create_singlethread_workqueue("ASUSEVTLOG_WORKQUEUE");

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&asusdebug_early_suspend_handler);
#endif

	AsusTraceTrigeInitialize();
	return 0;
}
module_init(proc_asusdebug_init);


