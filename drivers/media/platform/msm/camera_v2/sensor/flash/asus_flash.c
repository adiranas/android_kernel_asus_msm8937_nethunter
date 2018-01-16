#include "asus_flash.h"
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#define PROC_STATUS "driver/flash_status"
#define PROC_ATD_FLASH1 "driver/asus_flash"
#define PROC_ATD_FLASH2 "driver/asus_flash2"
#define PROC_ATD_FLASH3 "driver/asus_flash3"
#define PROC_FLASH_LIGHT "driver/asus_flash_brightness"
#define PROC_ZENFLASH "driver/asus_flash_trigger_time"
static int ATD_status;
static int last_brightness_value;

static struct mutex brightness_lock;
static enum msm_camera_led_config_t led_state;
static int camera_in_use;
static int flash_light_running;
static asus_flash_function_table_t * led_func_tbl = NULL;
int zenflash_fired = 0;
int zenflash_off = 0;
EXPORT_SYMBOL_GPL(zenflash_fired);
EXPORT_SYMBOL_GPL(zenflash_off);
static const char * get_led_state_string(enum msm_camera_led_config_t state)
{
	switch(state)
	{
		case MSM_CAMERA_LED_OFF:
			return "LED_OFF";
		case MSM_CAMERA_LED_LOW:
			return "LED_LOW";
		case MSM_CAMERA_LED_HIGH:
			return "LED_HIGH";
		case MSM_CAMERA_LED_INIT:
			return "LED_INIT";
		case MSM_CAMERA_LED_RELEASE:
			return "LED_RELEASE";
		default:
			return "INVALID_LED_STATE";
	}
}


static ssize_t status_show(struct file *dev, char *buffer, size_t count, loff_t *ppos)
{
	int n;
	char kbuf[8];

	if(*ppos == 0)
	{
		n = snprintf(kbuf, sizeof(kbuf),"%d\n%c", ATD_status,'\0');
		if(copy_to_user(buffer,kbuf,n))
		{
			pr_err("%s(): copy_to_user fail !\n",__func__);
			return -EFAULT;
		}
		*ppos += n;
		return n;
	}
	ATD_status = 0;
	pr_info("[ASUS FLASH] ATD_status set to 0\n");

	return 0;
}

static ssize_t flash1_store(struct file *dev, const char *buf, size_t count, loff_t *loff)
{
	char kbuf[8];
	int  mode = -1;
	int  on = -1;
	int  rc = 0;

	rc = count;

	if(count > 8)
		count = 8;

	if(copy_from_user(kbuf, buf, count))
	{
		pr_err("%s(): copy_from_user fail !\n",__func__);
		return -EFAULT;
	}

	led_func_tbl->asus_led_init();

	sscanf(kbuf, "%d %d", &mode, &on);

	if(mode == 0)//torch
	{
		if(on == 1)
		{
			led_func_tbl->asus_led_release();
			msleep(1);//must delay
			led_func_tbl->asus_led1_torch();
			ATD_status = 1;
		}
		else if(on == 0)
		{
			led_func_tbl->asus_led_release();
			ATD_status = 1;
		}
		else
		{
			rc = -1;
		}
	}
	else if(mode == 1)//flash
	{
		if(on == 1)
		{
			led_func_tbl->asus_led_release();
			msleep(1);//must delay
			led_func_tbl->asus_led1_flash();//set twice will not flash
			ATD_status = 1;
		}
		else if(on == 0)
		{
			led_func_tbl->asus_led_release();
			ATD_status = 1;
		}
		else
		{
			rc = -1;
		}
	}
	else
	{
		rc = -1;
	}

	if(rc < 0)
			led_func_tbl->asus_led_release();
	return rc;
}

static ssize_t flash2_store(struct file *dev, const char *buf, size_t count, loff_t *loff)
{
	char kbuf[8];
	int  mode = -1;
	int  on = -1;
	int  rc = 0;

	rc = count;

	if(count > 8)
		count = 8;

	if(copy_from_user(kbuf, buf, count))
	{
		pr_err("%s(): copy_from_user fail !\n",__func__);
		return -EFAULT;
	}

	led_func_tbl->asus_led_init();

	sscanf(kbuf, "%d %d", &mode, &on);

	if(mode == 0)//torch
	{
		if(on == 1)
		{
			led_func_tbl->asus_led_release();
			msleep(1);//must delay
			led_func_tbl->asus_led2_torch();
			ATD_status = 1;
		}
		else if(on == 0)
		{
			led_func_tbl->asus_led_release();
			ATD_status = 1;
		}
		else
		{
			rc = -1;
		}
	}
	else if(mode == 1)//flash
	{
		if(on == 1)
		{
			led_func_tbl->asus_led_release();
			msleep(1);//must delay
			led_func_tbl->asus_led2_flash();//set twice will not flash
			ATD_status = 1;
		}
		else if(on == 0)
		{
			led_func_tbl->asus_led_release();
			ATD_status = 1;
		}
		else
		{
			rc = -1;
		}
	}
	else
	{
		rc = -1;
	}
	if(rc < 0)
		led_func_tbl->asus_led_release();
	return rc;
}

static ssize_t flash3_store(struct file *dev, const char *buf, size_t count, loff_t *loff)
{
	char kbuf[8];
	int  mode = -1;
	int  on = -1;
	int  rc = 0;

	rc = count;

	if(count > 8)
		count = 8;

	if(copy_from_user(kbuf, buf, count))
	{
		pr_err("%s(): copy_from_user fail !\n",__func__);
		return -EFAULT;
	}

	led_func_tbl->asus_led_init();

	sscanf(kbuf, "%d %d", &mode, &on);

	if(mode == 0)//torch
	{
		if(on == 1)
		{
			led_func_tbl->asus_led_release();
			msleep(1);//must delay
			led_func_tbl->asus_led_dual_torch(200,200);
			ATD_status = 1;
		}
		else if(on == 0)
		{
			led_func_tbl->asus_led_release();
			ATD_status = 1;
		}
		else
		{
			rc = -1;
		}
	}
	else if(mode == 1)//flash
	{
		if(on == 1)
		{
			led_func_tbl->asus_led_release();
			msleep(1);//must delay
			led_func_tbl->asus_led_dual_flash(800,800);
			ATD_status = 1;
		}
		else if(on == 0)
		{
			led_func_tbl->asus_led_release();
			ATD_status = 1;
		}
		else
		{
			rc = -1;
		}
	}
	else
	{
		rc = -1;
	}
	if(rc < 0)
		led_func_tbl->asus_led_release();
	return rc;
}

static ssize_t flash_light_show(struct file *dev, char *buffer, size_t count, loff_t *ppos)
{
	int n;
	char kbuf[8];

	if(*ppos == 0)
	{
		n = snprintf(kbuf, sizeof(kbuf),"%d\n%c", last_brightness_value,'\0');
		if(copy_to_user(buffer,kbuf,n))
		{
			pr_err("%s(): copy_to_user fail !\n",__func__);
			return -EFAULT;
		}
		*ppos += n;
		return n;
	}
	return 0;
}
#if 1
static unsigned long diff_time_us(struct timeval *t1, struct timeval *t2 )
{
	return (((t1->tv_sec*1000000)+t1->tv_usec)-((t2->tv_sec*1000000)+t2->tv_usec));
}
#endif
static ssize_t zenflash_store(struct file *dev, const char *buff, size_t count, loff_t *loff)
{
	int set_current = -1;
	int set_time = -1;
	int MAX_ZENFLASH_CURRENT = 800;
	//int PMIC_INIT_WAIT_TIME_US = 1000+3000+2000;//just switch do in another thread
	//int PMIC_OFF_DONE_WAIT_TIME_MS = (2160+3000)/1000+1;//just switch do in another thread
	int rc = 0;
	char kbuf[16];
	int n;
	struct timeval t1,t2;

	if(count > 16)
		count = 16;

	if(copy_from_user(kbuf, buff, count))
	{
		pr_err("%s(): copy_from_user fail !\n",__func__);
		return -EFAULT;
	}

	n=sscanf(kbuf, "%d %d ",&set_time,&set_current);

	pr_info("[ASUS ZENFLASH] get param num is %d\n",n);

	if(n == 1)
	{
		set_current = MAX_ZENFLASH_CURRENT;//800mA
	}
	else
	{
		if(set_current < 0 || set_current > MAX_ZENFLASH_CURRENT)
			set_current = MAX_ZENFLASH_CURRENT;
	}

	if(set_time < 0 || set_time > 1000)
		set_time = 80;

	mutex_lock(&brightness_lock);

	pr_info("[ASUS ZENFLASH] ##### set current %d mA, time %d ms #####, led_state %s\n",
			set_current,set_time,get_led_state_string(led_state));

	if(led_state == MSM_CAMERA_LED_RELEASE)
	{
		rc = led_func_tbl->asus_led_init();
		if(rc < 0)
		{
			pr_err("[ASUS ZENFLASH] flash init failed !\n");
			led_func_tbl->asus_led_release();
			mutex_unlock(&brightness_lock);
			return -1;
		}
	}
	else if(led_state == MSM_CAMERA_LED_HIGH || led_state == MSM_CAMERA_LED_LOW)
	{
		pr_info("[ASUS ZENFLASH] LED state is %s, turn it off to do a light pulse for Zenflash\n",get_led_state_string(led_state));
		led_func_tbl->asus_led_off();
		msleep(1);//must delay....hardware limitation
	}

	zenflash_fired = 0;
	do_gettimeofday(&t1);
	led_func_tbl->asus_led_dual_flash(set_current,0);
	while(zenflash_fired == 0)
	{
		usleep_range(500,501);
	}
	do_gettimeofday(&t2);
	pr_info("[ASUS ZENFLASH] flash fired init cost %lu us, %lu ms\n",diff_time_us(&t2,&t1),diff_time_us(&t2,&t1)/1000);

	usleep_range(set_time*1000,set_time*1000+1);

	zenflash_off = 0;
	do_gettimeofday(&t1);
	led_func_tbl->asus_led_off();
	while(zenflash_off == 0)
	{
		usleep_range(500,501);
	}
	do_gettimeofday(&t2);
	pr_info("[ASUS ZENFLASH] flash turn off cost %lu us, %lu ms\n",diff_time_us(&t2,&t1),diff_time_us(&t2,&t1)/1000);

	mutex_unlock(&brightness_lock);
	return count;
}


static ssize_t flash_light_store(struct file *dev, const char *buff, size_t count, loff_t *loff)
{
	int set_val = -1;
	int real_set_brightness = -1;
	int MAX_FLASHLIGHT_CURRENT = 135;
	int rc = 0;
	char kbuf[8];

	if(count > 8)
		count = 8;

	if (copy_from_user(kbuf, buff, count))
	{
		pr_err("%s(): copy_from_user fail !\n",__func__);
		return -EFAULT;
	}

	sscanf(kbuf, "%d",&set_val);

	if(set_val < 0)
		real_set_brightness = 0;
	else if(set_val > 100)
		real_set_brightness = 100;
	else
		real_set_brightness = set_val;

	//led_state  should be a status indicator inside real structure
	//turn on and switch to third-party app, turn on, turn off, then switch back(from bar), val is same 99, etc, should not block this case.
	if(last_brightness_value == real_set_brightness && led_state != MSM_CAMERA_LED_RELEASE)
	{
		pr_info("[ASUS FLASH], real set val %d equal to last, ignore\n",real_set_brightness);
		return count;
	}

	mutex_lock(&brightness_lock);

	pr_info("[ASUS FLASH] ##### real set val %d #####, led_state %s\n",real_set_brightness,get_led_state_string(led_state));

	last_brightness_value = real_set_brightness;

	if(led_state == MSM_CAMERA_LED_RELEASE)
	{
		rc = led_func_tbl->asus_led_init();//will it be called every time?
		if(rc < 0)
		{
			pr_err("[ASUS FLASH] flash init failed !\n");
			led_func_tbl->asus_led_release();
			mutex_unlock(&brightness_lock);
			return -1;
		}
	}

	if(real_set_brightness == 0)
	{
		flash_light_running = 0;
		if(camera_in_use)
		{
			led_func_tbl->asus_led_off();//Camera is OPEN, just turn off LED, not power down
		}
		else
		{
			led_func_tbl->asus_led_release();
		}
		pr_info("[ASUS FLASH] #### FlashLight OFF #####");
	}
	else
	{
		flash_light_running = 1;
		if(led_state == MSM_CAMERA_LED_LOW)//not consider HIGH case
		{
			//change brightness value without set 0 first
			//hardware limitation
			led_func_tbl->asus_led_off();//PMIC off set current to 0
			msleep(1);//must delay
			led_func_tbl->asus_led_init();
			pr_info("[ASUS FLASH] led state is %d -> %s, already on, turn off and turn on again to set new current\n",led_state,get_led_state_string(led_state));
		}
		else if(led_state == MSM_CAMERA_LED_OFF || led_state == MSM_CAMERA_LED_RELEASE)
		{
			//torch recording switch to FlashLight, just turned off by camera, should delay to turn on for hardware limitation
			msleep(1);//must delay
		}
		led_func_tbl->asus_led_dual_torch(MAX_FLASHLIGHT_CURRENT*real_set_brightness/100,0);
		pr_info("[ASUS FLASH] #### FlashLight ON ####");
	}

	mutex_unlock(&brightness_lock);
	return count;
}
#if 0
static ssize_t dummy_show(struct file *dev, char *buffer, size_t count, loff_t *ppos)
{
       return 0;
}
static ssize_t dummy_store(struct file *dev, char *buffer, size_t count, loff_t *ppos)
{
       return 0;
}
#endif
static const struct file_operations flash1_proc_fops = {
       .read = NULL,
       .write = flash1_store,
};

static const struct file_operations flash2_proc_fops = {
       .read = NULL,
       .write = flash2_store,
};
static const struct file_operations flash3_proc_fops = {
       .read = NULL,
       .write = flash3_store,
};

static const struct file_operations flash_light_proc_fops = {
       .read = flash_light_show,
       .write = flash_light_store,
};
static const struct file_operations zenflash_proc_fops = {
       .read = NULL,
       .write = zenflash_store,
};

static const struct file_operations status_proc_fops = {
       .read = status_show,
       .write = NULL,
};

static void asus_flash_create_proc_files(void)
{
	struct proc_dir_entry* proc_entry_status;
	struct proc_dir_entry* proc_entry_flash1;
	struct proc_dir_entry* proc_entry_flash2;
	struct proc_dir_entry* proc_entry_flash3;
	struct proc_dir_entry* proc_entry_brightness;
	struct proc_dir_entry* proc_entry_zenflash;

	proc_entry_status = proc_create(PROC_STATUS, 0666, NULL, &status_proc_fops);
	proc_set_user(proc_entry_status, (kuid_t){1000}, (kgid_t){1000});

	proc_entry_flash1 = proc_create(PROC_ATD_FLASH1, 0666, NULL, &flash1_proc_fops);
	proc_set_user(proc_entry_flash1, (kuid_t){1000}, (kgid_t){1000});

	proc_entry_flash2 = proc_create(PROC_ATD_FLASH2, 0666, NULL, &flash2_proc_fops);
	proc_set_user(proc_entry_flash2, (kuid_t){1000}, (kgid_t){1000});

	proc_entry_flash3 = proc_create(PROC_ATD_FLASH3, 0666, NULL, &flash3_proc_fops);
	proc_set_user(proc_entry_flash3, (kuid_t){1000}, (kgid_t){1000});

	proc_entry_brightness = proc_create(PROC_FLASH_LIGHT, 0666, NULL, &flash_light_proc_fops);
	proc_set_user(proc_entry_brightness, (kuid_t){1000}, (kgid_t){1000});

	proc_entry_zenflash = proc_create(PROC_ZENFLASH, 0666, NULL, &zenflash_proc_fops);
	proc_set_user(proc_entry_zenflash, (kuid_t){1000}, (kgid_t){1000});
}

void asus_flash_set_led_state(enum msm_camera_led_config_t state)
{
	led_state = state;
	//pr_info("[ASUS FLASH] brightness state set to  %d -> %s,\n",led_state,get_led_state_string(led_state));
}

void asus_flash_set_camera_state(int in_use)
{
	camera_in_use = in_use;
	//pr_info("[ASUS FLASH] camera_in_use change to %d\n",camera_in_use);
}
int asus_flash_get_flash_light_state(void)
{
	return flash_light_running;
}
void asus_flash_lock(void)
{
	mutex_lock(&brightness_lock);
}
void asus_flash_unlock(void)
{
	mutex_unlock(&brightness_lock);
}
int init_asus_led(asus_flash_function_table_t * table)
{
	if(table)
		led_func_tbl = table;
	else
	{
		pr_err("[ASUS FLASH] function table is NULL!\n");
		return -1;
	}
	mutex_init(&brightness_lock);
	ATD_status = 0;
	last_brightness_value = 0;
	camera_in_use = 0;
	led_state = MSM_CAMERA_LED_RELEASE;
	asus_flash_create_proc_files();
	return 0;
}
