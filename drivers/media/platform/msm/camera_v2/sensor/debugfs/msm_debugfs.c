/*
 * msm_debugfs.c
 *
 *  Created on: Jan 13, 2015
 *      Author: charles
 *      Email: Weiche_Tsai@asus.com
 */
#include <media/v4l2-subdev.h>
#include "msm_debugfs.h"
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#define MAX_CAMERA 2
//ASUS_BSP Surya_Xu +++
#include <media/v4l2-subdev.h>
#define SENSOR_MAX_RETRIES      50
#define REAR_MODULE_OTP_SIZE 482
#define FRONT_MODULE_OTP_SIZE 482
#define MODULE_OTP_SIZE 96
#define MODULE_OTP_BULK_SIZE 32
#define MODULE_OTP_BULK 3
//ASUS_BSP Surya_Xu ---
static int number_of_camera = 0;
static struct debugfs dbgfs[MAX_CAMERA];
//static struct otp_struct otp_data;
//static struct msm_actuator_ctrl_t *s_ctrl_vcm;
//static struct msm_ois_ctrl_t *o_ctrl_ois;
extern int g_ftm_mode;
#define OTP_SIZE 32
static unsigned char imx318_otp[OTP_SIZE];
static unsigned char ov8856_otp[OTP_SIZE];
static unsigned char imx214_otp[MODULE_OTP_SIZE];
//static int ois_mode_err;
//static int ois_accel_gain_err;
static char rear_module_otp[REAR_MODULE_OTP_SIZE];
static char front_module_otp[FRONT_MODULE_OTP_SIZE];
//ASUS_BSP Surya_Xu +++
unsigned char g_camera_status[4] = {1, 1, 1,1};
unsigned char g_vga_status[2] = {1, 1};
int g_camera_id = MAX_CAMERAS;
static unsigned char g_camera_status_created = 0;
static unsigned char g_front_camera_status_created = 0;
int g_rear_camera_ref_cnt = -1;
int g_front_camera_ref_cnt = -1;
static void create_rear_status_proc_file(void);
static struct proc_dir_entry *otp_proc_file;
static void create_front_status_proc_file(void);
void remove_proc_file(void);
static unsigned char g_rear_temp_created = 0;
static void create_rear_temp_proc_file(void);
extern int isSensorPowerup;

static unsigned char g_rear_resolution_created = 0;
char *g_rear_resolution = "";
static void create_rear_resolution_proc_file(void);

static unsigned char g_front_resolution_created = 0;
char *g_front_resolution = "";
static void create_front_resolution_proc_file(void);

static unsigned char g_rear_module_created = 0;
char *g_rear_module = "";
static void create_rear_module_proc_file(void);

static unsigned char g_front_module_created = 0;
char *g_front_module = "";
static void create_front_module_proc_file(void);
//ASUS_BSP Surya_Xu---

static unsigned char g_register_debug_created = 0;

/* Defines for OTP Data Registers */
#define T4K35_OTP_START_ADDR	0x3504
#define T4K35_OTP_PAGE_REG	0x3502
#define T4K35_OTP_ENABLE		0x3500
#define T4K35_OTP_PAGE_SIZE	32
#define T4K35_OTP_DATA_SIZE	512
#define T4K35_OTP_READ_ONETIME	32

#define DEFAULT_UID "000000000000000000000000"
#define DEFAULT_OTP "NO available OTP"

//ASUS_BSP Surya_Xu +++
static unsigned char g_rear_otp_created = 0;
static void create_rear_otp_proc_file(void);
static void imx214_otp_read(struct msm_sensor_ctrl_t *s_ctrl);
//ASUS_BSP Surya_Xu ---

static int dbg_dump_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}
#if 0 //remove imx214 for eeprom
static ssize_t dbg_dump_imx214_otp_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;
	int i;

	pr_info("%s: buf=%p, count=%d, ppos=%p; *ppos= %d\n",
		__func__, buf, (int)count, ppos, (int)*ppos);

	if (*ppos)
		return 0;	/* the end */

	len = 0;
	for (i = 0; i < OTP_SIZE; i++)
		len += scnprintf(debug_buf + len, sizeof(debug_buf) - len, "0x%02X ", imx214_otp[i]);
	pr_debug("OTP=%s\n", debug_buf);

#if 0
	len = snprintf(bp, dlen,
		"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		otp_data.af_inf[0], otp_data.af_inf[1],
		otp_data.af_30cm[0], otp_data.af_30cm[1],
		otp_data.af_5cm[0], otp_data.af_5cm[1],
		otp_data.start_current[0], otp_data.start_current[1],
		otp_data.module_id, otp_data.vendor_id);
#endif

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_dump_imx214_otp_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_dump_imx214_otp_read,
};


static ssize_t dbg_dump_imx214_uid_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;
	int i;

	pr_info("%s: buf=%p, count=%d, ppos=%p; *ppos= %d\n",
		__func__, buf, (int)count, ppos, (int)*ppos);

	if (*ppos)
		return 0;	/* the end */

	len = 0;
	for (i = 0; i < 12; i++)
		len += scnprintf(debug_buf + len, sizeof(debug_buf) - len, "%02X", imx214_otp[10 + i]);
	pr_debug("UID=%s\n", debug_buf);

#if 0
	len = snprintf(bp, dlen,
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		otp_data.dc[0], otp_data.dc[1],
		otp_data.dc[2], otp_data.dc[3],
		otp_data.sn[0], otp_data.sn[1],
		otp_data.sn[2], otp_data.sn[3],
		otp_data.pn[0], otp_data.pn[1],
		otp_data.pn[2], otp_data.pn[3]);
#endif

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_dump_imx214_uid_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_dump_imx214_uid_read,
};

#endif //remove imx214 for eeprom

static ssize_t dbg_dump_imx318_otp_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;
	int i;

	pr_info("%s: buf=%p, count=%d, ppos=%p; *ppos= %d\n",
		__func__, buf, (int)count, ppos, (int)*ppos);

	if (*ppos)
		return 0;	/* the end */

	len = 0;
	for (i = 0; i < OTP_SIZE; i++)
		len += scnprintf(debug_buf + len, sizeof(debug_buf) - len, "0x%02X ", imx318_otp[i]);
	pr_debug("OTP=%s\n", debug_buf);

#if 0
	len = snprintf(bp, dlen,
		"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		otp_data.af_inf[0], otp_data.af_inf[1],
		otp_data.af_30cm[0], otp_data.af_30cm[1],
		otp_data.af_5cm[0], otp_data.af_5cm[1],
		otp_data.start_current[0], otp_data.start_current[1],
		otp_data.module_id, otp_data.vendor_id);
#endif

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_dump_imx318_otp_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_dump_imx318_otp_read,
};

static ssize_t dbg_dump_imx318_uid_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;
	int i;

	pr_info("%s: buf=%p, count=%d, ppos=%p; *ppos= %d\n",
		__func__, buf, (int)count, ppos, (int)*ppos);

	if (*ppos)
		return 0;	/* the end */

	len = 0;
	for (i = 0; i < 12; i++)
		len += scnprintf(debug_buf + len, sizeof(debug_buf) - len, "%02X", imx318_otp[10 + i]);
	pr_debug("UID=%s\n", debug_buf);

#if 0
	len = snprintf(bp, dlen,
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		otp_data.dc[0], otp_data.dc[1],
		otp_data.dc[2], otp_data.dc[3],
		otp_data.sn[0], otp_data.sn[1],
		otp_data.sn[2], otp_data.sn[3],
		otp_data.pn[0], otp_data.pn[1],
		otp_data.pn[2], otp_data.pn[3]);
#endif

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_dump_imx318_uid_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_dump_imx318_uid_read,
};

static ssize_t dbg_dump_ov8856_otp_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;
	int i;

	pr_info("%s: buf=%p, count=%d, ppos=%p; *ppos= %d\n",
		__func__, buf, (int)count, ppos, (int)*ppos);

	if (*ppos)
		return 0;	/* the end */

	len = 0;
	for (i = 0; i < OTP_SIZE; i++)
		len += scnprintf(debug_buf + len, sizeof(debug_buf) - len, "0x%02X ", ov8856_otp[i]);
	pr_debug("OTP=%s\n", debug_buf);

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_dump_ov8856_otp_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_dump_ov8856_otp_read,
};

static ssize_t dbg_dump_ov8856_uid_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;
	int i;

	pr_info("%s: buf=%p, count=%d, ppos=%p; *ppos= %d\n",
		__func__, buf, (int)count, ppos, (int)*ppos);

	if (*ppos)
		return 0;	/* the end */

	len = 0;
	for (i = 0; i < 12; i++)
		len += scnprintf(debug_buf + len, sizeof(debug_buf) - len, "%02X", ov8856_otp[10 + i]);
	pr_debug("UID=%s\n", debug_buf);

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_dump_ov8856_uid_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_dump_ov8856_uid_read,
};

static ssize_t dbg_default_otp_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;

	pr_info("%s: buf=%p, count=%d, ppos=%p; *ppos= %d\n",
		__func__, buf, (int)count, ppos, (int)*ppos);

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(bp, dlen,"%s\n", DEFAULT_OTP);

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_default_otp_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_default_otp_read,
};

static ssize_t dbg_default_uid_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;

	pr_info("%s: buf=%p, count=%d, ppos=%p; *ppos= %d\n",
		__func__, buf, (int)count, ppos, (int)*ppos);

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(bp, dlen,"%s\n", DEFAULT_UID);

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_default_uid_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_default_uid_read,
};

#if 0 
static ssize_t dbg_ois_mode_write(
	struct file *file,
	const char __user *buf,
	size_t count,
	loff_t *ppos)
{
	char debug_buf[256];
	int cnt;
	unsigned int ois_mode = 0;

	struct msm_camera_i2c_client client = o_ctrl_ois->i2c_client;
	int (*i2c_write) (struct msm_camera_i2c_client *, uint32_t, uint16_t,
		enum msm_camera_i2c_data_type) = o_ctrl_ois->i2c_client.i2c_func_tbl->i2c_write;

	if (count > sizeof(debug_buf))
		return -EFAULT;
	if (copy_from_user(debug_buf, buf, count))
		return -EFAULT;
	debug_buf[count] = '\0';	/* end of string */

	cnt = sscanf(debug_buf, "%d", &ois_mode);
	pr_err("%s: set ois_mode to %d\n", __func__, ois_mode);

	if (o_ctrl_ois==NULL)
		pr_err("%s: o_ctrl_ois is null\n", __func__);
	if (i2c_write==NULL)
		pr_err("%s: i2c_write is null\n", __func__);

	switch (ois_mode) {
	case 0: /* turn off ois, only centering on */
		ois_mode_err |= i2c_write(&client, 0x847F, 0x0C0C, 2);
		if (ois_mode_err < 0)
			pr_err("ois_mode %d set fail\n", ois_mode);
		break;
	case 1: /* movie mode */
		ois_mode_err |= i2c_write(&client, 0x847F, 0x0C0C, 2);
		ois_mode_err |= i2c_write(&client, 0x8436, 0xF07F, 2);
		ois_mode_err |= i2c_write(&client, 0x8440, 0xF07F, 2);
		ois_mode_err |= i2c_write(&client, 0x8443, 0xB41E, 2);
		ois_mode_err |= i2c_write(&client, 0x841B, 0x0001, 2);
		ois_mode_err |= i2c_write(&client, 0x84B6, 0xF07F, 2);
		ois_mode_err |= i2c_write(&client, 0x84C0, 0xF07F, 2);
		ois_mode_err |= i2c_write(&client, 0x84C3, 0xB41E, 2);
		ois_mode_err |= i2c_write(&client, 0x849B, 0x0001, 2);
		ois_mode_err |= i2c_write(&client, 0x8438, 0xF212, 2);
		ois_mode_err |= i2c_write(&client, 0x84B8, 0xF212, 2);
		if (imx318_otp[9]==2) {
			pr_err("chicony module\n");
			ois_mode_err |= i2c_write(&client, 0x8447, 0x3B21, 2);
			ois_mode_err |= i2c_write(&client, 0x84C7, 0x3B21, 2);
		} else {
			pr_err("liteon module\n");
			ois_mode_err |= i2c_write(&client, 0x8447, 0xC320, 2);
			ois_mode_err |= i2c_write(&client, 0x84C7, 0xC320, 2);
		}
		ois_mode_err |= i2c_write(&client, 0x847F, 0x0D0D, 2);
		if (ois_mode_err < 0)
			pr_err("ois_mode %d set fail\n", ois_mode);
		break;
	case 2: /* still mode */
		ois_mode_err |= i2c_write(&client, 0x847F, 0x0C0C, 2);
		ois_mode_err |= i2c_write(&client, 0x8436, 0xF07F, 2);
		ois_mode_err |= i2c_write(&client, 0x8440, 0xF07F, 2);
		ois_mode_err |= i2c_write(&client, 0x8443, 0xB41E, 2);
		ois_mode_err |= i2c_write(&client, 0x841B, 0x0001, 2);
		ois_mode_err |= i2c_write(&client, 0x84B6, 0xF07F, 2);
		ois_mode_err |= i2c_write(&client, 0x84C0, 0xF07F, 2);
		ois_mode_err |= i2c_write(&client, 0x84C3, 0xB41E, 2);
		ois_mode_err |= i2c_write(&client, 0x849B, 0x0001, 2);
		ois_mode_err |= i2c_write(&client, 0x8438, 0xF212, 2);
		ois_mode_err |= i2c_write(&client, 0x84B8, 0xF212, 2);
		if (imx318_otp[9]==2) {
			pr_err("chicony module\n");
			ois_mode_err |= i2c_write(&client, 0x8447, 0x3B21, 2);
			ois_mode_err |= i2c_write(&client, 0x84C7, 0x3B21, 2);
		} else {
			pr_err("liteon module\n");
			ois_mode_err |= i2c_write(&client, 0x8447, 0xC320, 2);
			ois_mode_err |= i2c_write(&client, 0x84C7, 0xC320, 2);
		}
		ois_mode_err |= i2c_write(&client, 0x847F, 0x0D0D, 2);
		if (ois_mode_err < 0)
			pr_err("ois_mode %d set fail\n", ois_mode);
		break;
	case 3: /* test mode */
		ois_mode_err |= i2c_write(&client, 0x847F, 0x0C0C, 2);
		ois_mode_err |= i2c_write(&client, 0x8436, 0xFF7F, 2);
		ois_mode_err |= i2c_write(&client, 0x8440, 0xFF7F, 2);
		ois_mode_err |= i2c_write(&client, 0x8443, 0xFF7F, 2);
		ois_mode_err |= i2c_write(&client, 0x841B, 0x8000, 2);
		ois_mode_err |= i2c_write(&client, 0x84B6, 0xFF7F, 2);
		ois_mode_err |= i2c_write(&client, 0x84C0, 0xFF7F, 2);
		ois_mode_err |= i2c_write(&client, 0x84C3, 0xFF7F, 2);
		ois_mode_err |= i2c_write(&client, 0x849B, 0x8000, 2);
		ois_mode_err |= i2c_write(&client, 0x8438, 0xB209, 2);
		ois_mode_err |= i2c_write(&client, 0x84B8, 0xB209, 2);
		if (imx318_otp[9]==2) {
			ois_mode_err |= i2c_write(&client, 0x8447, 0xF240, 2);
			ois_mode_err |= i2c_write(&client, 0x84C7, 0xF240, 2);
		} else {
			ois_mode_err |= i2c_write(&client, 0x8447, 0x0740, 2);
			ois_mode_err |= i2c_write(&client, 0x84C7, 0x0740, 2);
		}
		ois_mode_err |= i2c_write(&client, 0x847F, 0x0D0D, 2);
		if (ois_mode_err < 0)
			pr_err("ois_mode %d set fail\n", ois_mode);
		break;
	default:
		pr_err("do not support this ois_mode %d\n", ois_mode);
		break;
	}

	return count;
}

static ssize_t dbg_ois_mode_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(bp, dlen,"%d\n", ois_mode_err);

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}


static const struct file_operations dbg_ois_mode_fops = {
	.open		= dbg_dump_open,
	.write		= dbg_ois_mode_write,
	.read		= dbg_ois_mode_read,
};

static ssize_t dbg_ois_gyro_x_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;
	int ret = 0;
	uint16_t gyro_x = 0;

	struct msm_camera_i2c_client client = o_ctrl_ois->i2c_client;
	int (*i2c_read) (struct msm_camera_i2c_client *, uint32_t, uint16_t *,
		enum msm_camera_i2c_data_type) = o_ctrl_ois->i2c_client.i2c_func_tbl->i2c_read;

	if (*ppos)
		return 0;	/* the end */

	ret |= i2c_read(&client, 0x8455, &gyro_x, 2);
	if (ret < 0)
		pr_err("%s: ret=%d\n", __func__, ret);

	len = snprintf(bp, dlen,"%d\n", gyro_x);

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_ois_gyro_x_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_ois_gyro_x_read,
};

static ssize_t dbg_ois_gyro_y_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;
	int ret = 0;
	uint16_t gyro_y = 0;

	struct msm_camera_i2c_client client = o_ctrl_ois->i2c_client;
	int (*i2c_read) (struct msm_camera_i2c_client *, uint32_t, uint16_t *,
		enum msm_camera_i2c_data_type) = o_ctrl_ois->i2c_client.i2c_func_tbl->i2c_read;

	if (*ppos)
		return 0;	/* the end */

	ret |= i2c_read(&client, 0x8456, &gyro_y, 2);
	if (ret < 0)
		pr_err("%s: ret=%d\n", __func__, ret);

	len = snprintf(bp, dlen,"%d\n", gyro_y);

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_ois_gyro_y_fops = {
	.open		= dbg_dump_open,
	.read		= dbg_ois_gyro_y_read,
};

static ssize_t dbg_ois_accel_gain_write(
	struct file *file,
	const char __user *buf,
	size_t count,
	loff_t *ppos)
{
	char debug_buf[256];
	int cnt;
	unsigned int accel_gain_x = 0, accel_gain_y = 0;
	unsigned int accel_gain_x_swap = 0, accel_gain_y_swap = 0;

	struct msm_camera_i2c_client client = o_ctrl_ois->i2c_client;
	int (*i2c_write) (struct msm_camera_i2c_client *, uint32_t, uint16_t,
		enum msm_camera_i2c_data_type) = o_ctrl_ois->i2c_client.i2c_func_tbl->i2c_write;

	if (count > sizeof(debug_buf))
		return -EFAULT;
	if (copy_from_user(debug_buf, buf, count))
		return -EFAULT;
	debug_buf[count] = '\0';	/* end of string */

	cnt = sscanf(debug_buf, "%d %d", &accel_gain_x, &accel_gain_y);
	pr_err("%s: set accel_gain_x to %d, accel_gain_y=%d\n", __func__, accel_gain_x, accel_gain_y);

	if (o_ctrl_ois==NULL)
		pr_err("%s: o_ctrl_ois is null\n", __func__);
	if (i2c_write==NULL)
		pr_err("%s: i2c_write is null\n", __func__);

	/* should write low byte first */
	accel_gain_x_swap = ((accel_gain_x & 0xFF) << 8) | ((accel_gain_x & 0xFF00) >> 8 );
	accel_gain_y_swap = ((accel_gain_y & 0xFF) << 8) | ((accel_gain_y & 0xFF00) >> 8 );

	ois_accel_gain_err |= i2c_write(&client, 0x828B, accel_gain_x_swap, 2);
	ois_accel_gain_err |= i2c_write(&client, 0x82CB, accel_gain_y_swap, 2);
	if (ois_accel_gain_err < 0)
		pr_err("ois_accel_gain set fail\n");

	return count;
}

static ssize_t dbg_ois_accel_gain_read(
	struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int len = 0;
	int tot = 0;
	char debug_buf[256];
	int dlen = sizeof(debug_buf);
	char *bp = debug_buf;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(bp, dlen,"%d\n", ois_accel_gain_err);

	tot += len; bp += len; dlen -= len;

	if (copy_to_user(buf, debug_buf, tot))
		return -EFAULT;

	if (tot < 0)
		return 0;
	*ppos += tot;	/* increase offset */
	return tot;
}

static const struct file_operations dbg_ois_accel_gain_fops = {
	.open		= dbg_dump_open,
	.write		= dbg_ois_accel_gain_write,
	.read		= dbg_ois_accel_gain_read,
};
#endif  //remove accel  gain



static int rear_otp_proc_read(struct seq_file *buf, void *v)
{
	/*int len = 0;
	char debug_buf[256];
	int i;

	for (i = 0; i < MODULE_OTP_SIZE; i++)
		len += scnprintf(debug_buf + len, sizeof(debug_buf) - len, "0x%02X ", imx214_otp[i]);
	pr_debug("OTP=%s\n", debug_buf);

	seq_printf(buf, "%s\n", debug_buf);*/
	seq_printf(buf, "%s\n", rear_module_otp);
	return 0;
}

static int rear_otp_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, rear_otp_proc_read, NULL);
}

static const struct file_operations rear_otp_proc_fops = {
	.owner = THIS_MODULE,
	.open = rear_otp_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_rear_otp_proc_file(void)
{
	if(!g_rear_otp_created) {
	    otp_proc_file = proc_create(REAR_OTP_PROC_FILE, 0666, NULL, &rear_otp_proc_fops);
	    if (otp_proc_file) {
			//pr_info("Stimber: %s sucessed!\n", __func__);
			g_rear_otp_created = 1;
	    } else {
			pr_info("Stimber: %s failed!\n", __func__);
			g_rear_otp_created = 0;
	    }
	} else {
        pr_info("File Exist!\n");
    }
}

static int front_otp_proc_read(struct seq_file *buf, void *v)
{
	/*int len = 0;
	char debug_buf[256];
	int i;

	for (i = 0; i < OTP_SIZE; i++)
		len += scnprintf(debug_buf + len, sizeof(debug_buf) - len, "0x%02X ", ov8856_otp[i]);
	pr_debug("OTP=%s\n", debug_buf);

	seq_printf(buf, "%s\n", debug_buf);*/
	seq_printf(buf, "%s\n", front_module_otp);
	return 0;
}

static int front_otp_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, front_otp_proc_read, NULL);
}

static const struct file_operations front_otp_proc_fops = {
	.owner = THIS_MODULE,
	.open = front_otp_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void msm_debugfs_set_status(unsigned int index, unsigned int status)
{
	if (index < MAX_CAMERA)
		dbgfs[index].status = status;
}

//ASUS_BSP Surya_Xu +++
int s_sensor_read_reg(struct msm_sensor_ctrl_t  *s_ctrl, u16 addr, u16 *val)
{
	int err;
	err = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,addr,val,MSM_CAMERA_I2C_BYTE_DATA);
	//pr_info("s_sensor_read_reg 0x%x\n",*val);
	if(err <0)
		return -EINVAL;
	else return 0;
}

int s_sensor_write_reg(struct msm_sensor_ctrl_t  *s_ctrl, u16 addr, u16 val)
{
	int err;
	int retry = 0;
	do {
		err =s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,addr,val,MSM_CAMERA_I2C_BYTE_DATA);
		if (err == 0)
			return 0;
		retry++;
		pr_err("Stimber : i2c transfer failed, retrying %x %x\n",
		       addr, val);
		msleep(1);
	} while (retry <= SENSOR_MAX_RETRIES);

	if(err == 0) {
		pr_err("%s(%d): i2c_transfer error, but return 0!?\n", __FUNCTION__, __LINE__);
		err = 0xAAAA;
	}

	return err;
}

static int __imx214_otp_read(struct msm_sensor_ctrl_t *s_ctrl){
	int i, check_count, ret = 0;
	int page_num = 3;
	u16 read_value[MODULE_OTP_BULK_SIZE];
	u8 page_cnt = 0;
	u16 bank_value[MODULE_OTP_SIZE];
	u16 check_status;
	struct msm_sensor_ctrl_t  *g_ctrl = s_ctrl;

	memset(bank_value, 0, sizeof(bank_value));
	memset(read_value, 0, sizeof(read_value));
	while(page_cnt<page_num){
		check_count = 0;
		ret = s_sensor_write_reg(g_ctrl, 0x0A02, page_cnt);
		if (ret) {
			pr_err("%s: i2c failed \n",
				 __func__);
		}

		ret = s_sensor_write_reg(g_ctrl, 0x0A00, 0x01);
		if (ret) {
			pr_err("%s: i2c failed \n",
				 __func__);
		}

		do {
			pr_debug("%s : check access status ...\n", __func__);
			s_sensor_read_reg(g_ctrl, 0x0A01, &check_status);
			usleep_range(300, 500);
			check_count++;
		} while (((check_status & 0x1d) == 0) && (check_count <= 10));
		if(check_status & 0x1d){
			for (i = 0; i < MODULE_OTP_BULK_SIZE; i++) {
				s_sensor_read_reg(g_ctrl, 0x0A04+i, &read_value[i]);
				bank_value[i+page_cnt*MODULE_OTP_BULK_SIZE] = read_value[i];
			}
		}
		page_cnt++;
	}

	snprintf(rear_module_otp, sizeof(rear_module_otp)
		, "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n"
			, bank_value[0]&0xFF, bank_value[1]&0xFF, bank_value[2]&0xFF, bank_value[3]&0xFF, bank_value[4]&0xFF
			, bank_value[5]&0xFF, bank_value[6]&0xFF, bank_value[7]&0xFF, bank_value[8]&0xFF, bank_value[9]&0xFF
			, bank_value[10]&0xFF, bank_value[11]&0xFF, bank_value[12]&0xFF, bank_value[13]&0xFF, bank_value[14]&0xFF
			, bank_value[15]&0xFF, bank_value[16]&0xFF, bank_value[17]&0xFF, bank_value[18]&0xFF, bank_value[19]&0xFF
			, bank_value[20]&0xFF, bank_value[21]&0xFF, bank_value[22]&0xFF, bank_value[23]&0xFF
			, bank_value[24]&0xFF, bank_value[25]&0xFF, bank_value[26]&0xFF, bank_value[27]&0xFF, bank_value[28]&0xFF
			, bank_value[29]&0xFF, bank_value[30]&0xFF, bank_value[31]&0xFF, bank_value[32]&0xFF, bank_value[33]&0xFF
			, bank_value[34]&0xFF, bank_value[35]&0xFF, bank_value[36]&0xFF, bank_value[37]&0xFF, bank_value[38]&0xFF
			, bank_value[39]&0xFF, bank_value[40]&0xFF, bank_value[41]&0xFF, bank_value[42]&0xFF, bank_value[43]&0xFF
			, bank_value[44]&0xFF, bank_value[45]&0xFF, bank_value[46]&0xFF, bank_value[47]&0xFF
			, bank_value[48]&0xFF, bank_value[49]&0xFF, bank_value[50]&0xFF, bank_value[51]&0xFF, bank_value[52]&0xFF
			, bank_value[53]&0xFF, bank_value[54]&0xFF, bank_value[55]&0xFF, bank_value[56]&0xFF, bank_value[57]&0xFF
			, bank_value[58]&0xFF, bank_value[59]&0xFF, bank_value[60]&0xFF, bank_value[61]&0xFF, bank_value[62]&0xFF
			, bank_value[63]&0xFF, bank_value[64]&0xFF, bank_value[65]&0xFF, bank_value[66]&0xFF, bank_value[67]&0xFF
			, bank_value[68]&0xFF, bank_value[69]&0xFF, bank_value[70]&0xFF, bank_value[71]&0xFF
			, bank_value[72]&0xFF, bank_value[73]&0xFF, bank_value[74]&0xFF, bank_value[75]&0xFF, bank_value[76]&0xFF
			, bank_value[77]&0xFF, bank_value[78]&0xFF, bank_value[79]&0xFF, bank_value[80]&0xFF, bank_value[81]&0xFF
			, bank_value[82]&0xFF, bank_value[83]&0xFF, bank_value[84]&0xFF, bank_value[85]&0xFF, bank_value[86]&0xFF
			, bank_value[87]&0xFF, bank_value[88]&0xFF, bank_value[89]&0xFF, bank_value[90]&0xFF, bank_value[91]&0xFF
			, bank_value[92]&0xFF, bank_value[93]&0xFF, bank_value[94]&0xFF, bank_value[95]&0xFF);
	pr_debug("Stimber: %s OTP value: %s\n", __func__, rear_module_otp);
	return 0;
}

static void imx214_otp_read(struct msm_sensor_ctrl_t *s_ctrl)
{
	int ret=0;

	ret = __imx214_otp_read(s_ctrl);
	if (ret) {
		pr_err("%s: sensor found no valid OTP data\n",
			  __func__);
	}
}

//BSP Surya_Xu ---

static int ov8856_read_otp(struct msm_sensor_ctrl_t *s_ctrl)
{
	int  i;
	u16 local_data;
	int package;
	u8 buf[32];
	u16 bank_value[MODULE_OTP_SIZE];
	u16 start_addr, end_addr;
	struct msm_camera_i2c_client *client = s_ctrl->sensor_i2c_client;
	int (*i2c_write) (struct msm_camera_i2c_client *, uint32_t, uint16_t,
		enum msm_camera_i2c_data_type) = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write;
	int (*i2c_read) (struct msm_camera_i2c_client *, uint32_t, uint16_t *,
		enum msm_camera_i2c_data_type) = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read;
	int32_t (*i2c_read_seq)(struct msm_camera_i2c_client *, uint32_t,
		uint8_t *, uint32_t) = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq;

	 /* make sure reset sensor as default */
	i2c_write(client, 0x0103, 0x01, 1);
	/*set 0x5001[3] to 0 */
	i2c_read(client, 0x5001, &local_data, 1);
	i2c_write(client, 0x5001, ((0x00 & 0x08) | (local_data & (~0x08))), 1);

	for (package = 2; package >= 0; package--) {
		if (package == 0) {
			start_addr = 0x7010;
			end_addr   = 0x702f;
		} else if (package == 1) {
			start_addr = 0x7030;
			end_addr   = 0x704f;
		} else if (package == 2) {
			start_addr = 0x7050;
			end_addr   = 0x706f;
		}
		/* [6] Manual mode(partial) */
		i2c_write(client, 0x3d84, 0xc0, 1);
		/* rst default:13 */
		i2c_write(client, 0x3d85, 0x06, 1);
		/*otp start addr*/
		i2c_write(client, 0x3d88, (start_addr >> 8) & 0xff, 1);
		i2c_write(client, 0x3d89, start_addr & 0xff, 1);
		/*otp end addr*/
		i2c_write(client, 0x3d8A, (end_addr >> 8) & 0xff, 1);
		i2c_write(client, 0x3d8B, end_addr & 0xff, 1);
		/* trigger auto_load */
		i2c_write(client, 0x0100, 0x01, 1);
		/*load otp into buffer*/
		i2c_write(client, 0x3d81, 0x01, 1);
		msleep(5);

		i2c_read_seq(client, start_addr, buf, 32);

		if (buf[8] != 0 || buf[9] != 0) {
			memcpy(&ov8856_otp, (u8 *)&buf, sizeof(buf));
			pr_info("ov8856 otp read success\n");
		}
		for (i = 0; i < MODULE_OTP_BULK_SIZE; i++) {
			i2c_read_seq(client, start_addr, buf, 32);
			bank_value[i+package*MODULE_OTP_BULK_SIZE] = buf[i];
		}
	}
		snprintf(front_module_otp, sizeof(front_module_otp)
		, "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n"
		, bank_value[0]&0xFF, bank_value[1]&0xFF, bank_value[2]&0xFF, bank_value[3]&0xFF, bank_value[4]&0xFF
		, bank_value[5]&0xFF, bank_value[6]&0xFF, bank_value[7]&0xFF, bank_value[8]&0xFF, bank_value[9]&0xFF
		, bank_value[10]&0xFF, bank_value[11]&0xFF, bank_value[12]&0xFF, bank_value[13]&0xFF, bank_value[14]&0xFF
		, bank_value[15]&0xFF, bank_value[16]&0xFF, bank_value[17]&0xFF, bank_value[18]&0xFF, bank_value[19]&0xFF
		, bank_value[20]&0xFF, bank_value[21]&0xFF, bank_value[22]&0xFF, bank_value[23]&0xFF
		, bank_value[24]&0xFF, bank_value[25]&0xFF, bank_value[26]&0xFF, bank_value[27]&0xFF, bank_value[28]&0xFF
		, bank_value[29]&0xFF, bank_value[30]&0xFF, bank_value[31]&0xFF, bank_value[32]&0xFF, bank_value[33]&0xFF
		, bank_value[34]&0xFF, bank_value[35]&0xFF, bank_value[36]&0xFF, bank_value[37]&0xFF, bank_value[38]&0xFF
		, bank_value[39]&0xFF, bank_value[40]&0xFF, bank_value[41]&0xFF, bank_value[42]&0xFF, bank_value[43]&0xFF
		, bank_value[44]&0xFF, bank_value[45]&0xFF, bank_value[46]&0xFF, bank_value[47]&0xFF
		, bank_value[48]&0xFF, bank_value[49]&0xFF, bank_value[50]&0xFF, bank_value[51]&0xFF, bank_value[52]&0xFF
		, bank_value[53]&0xFF, bank_value[54]&0xFF, bank_value[55]&0xFF, bank_value[56]&0xFF, bank_value[57]&0xFF
		, bank_value[58]&0xFF, bank_value[59]&0xFF, bank_value[60]&0xFF, bank_value[61]&0xFF, bank_value[62]&0xFF
		, bank_value[63]&0xFF, bank_value[64]&0xFF, bank_value[65]&0xFF, bank_value[66]&0xFF, bank_value[67]&0xFF
		, bank_value[68]&0xFF, bank_value[69]&0xFF, bank_value[70]&0xFF, bank_value[71]&0xFF
		, bank_value[72]&0xFF, bank_value[73]&0xFF, bank_value[74]&0xFF, bank_value[75]&0xFF, bank_value[76]&0xFF
		, bank_value[77]&0xFF, bank_value[78]&0xFF, bank_value[79]&0xFF, bank_value[80]&0xFF, bank_value[81]&0xFF
		, bank_value[82]&0xFF, bank_value[83]&0xFF, bank_value[84]&0xFF, bank_value[85]&0xFF, bank_value[86]&0xFF
		, bank_value[87]&0xFF, bank_value[88]&0xFF, bank_value[89]&0xFF, bank_value[90]&0xFF, bank_value[91]&0xFF
		, bank_value[92]&0xFF, bank_value[93]&0xFF, bank_value[94]&0xFF, bank_value[95]&0xFF);
		pr_debug("Stimber: %s OTP value: %s\n", __func__, front_module_otp);
		goto out;

	pr_err("ov8856 otp read failed\n");
out:
	for (i = start_addr ; i <= end_addr ; i++)
		i2c_write(client, i, 0x00, 1);
	i2c_read(client, 0x5001, &local_data, 1);
	i2c_write(client, 0x5001, (0x08 & 0x08) | (local_data & (~0x08)), 1);
	i2c_write(client, 0x0100, 0x00, 1);
	return 0;
}
#if 0
static int t4k35_read_otp(struct msm_sensor_ctrl_t *s_ctrl)
{
	int page = 0;
	u16 index;
	u8 buf[32];
	struct msm_camera_i2c_client *client = s_ctrl->sensor_i2c_client;
	int (*i2c_write) (struct msm_camera_i2c_client *, uint32_t, uint16_t,
		enum msm_camera_i2c_data_type) = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write;
	int32_t (*i2c_read_seq)(struct msm_camera_i2c_client *, uint32_t,
		uint8_t *, uint32_t) = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq;

	i2c_write(client, T4K35_OTP_ENABLE, 0x81, MSM_CAMERA_I2C_BYTE_DATA);

	for (page = 2; page >= 0; page--) {
		/*set page NO.*/
		i2c_write(client, T4K35_OTP_PAGE_REG, page, MSM_CAMERA_I2C_BYTE_DATA);
		for (index = 0 ; index + T4K35_OTP_READ_ONETIME <= T4K35_OTP_PAGE_SIZE ;
			index += T4K35_OTP_READ_ONETIME) {
			i2c_read_seq(client, T4K35_OTP_START_ADDR + index,
				&buf[index], T4K35_OTP_READ_ONETIME);
		}
		if ((buf[0] != 0 || buf[1] != 0) && (buf[0] != 0xff || buf[1] != 0xff)) {
			memcpy(&otp_data, &buf, sizeof(buf));
			pr_info("t4k35 otp read success\n");
			goto out;
		}
	}
	pr_err("t4k35 otp read failed\n");
out:
	return 0;
}
#endif
int msm_read_otp(struct msm_sensor_ctrl_t *s_ctrl, struct msm_camera_sensor_slave_info *slave_info)
{
	const char *sensor_name = s_ctrl->sensordata->sensor_name;

	if (!s_ctrl) {
		pr_err("%s:%d failed: %p\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}

	if (!strcmp(sensor_name,"imx214"))
		imx214_otp_read(s_ctrl);
	else if (!strcmp(sensor_name,"ov8856"))
		ov8856_read_otp(s_ctrl);
	return 0;
}
/*void msm_set_actuator_ctrl(struct msm_actuator_ctrl_t *s_ctrl)
{
	s_ctrl_vcm = s_ctrl;
}
void msm_set_ois_ctrl(struct msm_ois_ctrl_t *o_ctrl)
{
	o_ctrl_ois = o_ctrl;
}*/
void get_eeprom_OTP(struct msm_eeprom_memory_block_t *block)
{
	memcpy(imx214_otp, block->mapdata, OTP_SIZE);
}

static struct proc_dir_entry *status_proc_file;

static int rear_status_proc_read(struct seq_file *buf, void *v)
{
	unsigned char status = 0;
	int i=0;
	for(i=0; i<=g_rear_camera_ref_cnt; i++){
		//pr_info("Stimber proc read=%d\n",g_camera_status[i]);
		status |= g_camera_status[i];
	}
	
   	seq_printf(buf, "%d\n", status);				
    return 0;
}

static int front_status_proc_read(struct seq_file *buf, void *v)
{
	unsigned char status = 0;
	int i=0;
	for(i=0; i<=g_front_camera_ref_cnt; i++){
		//pr_info("Stimber front proc read=%d\n",g_vga_status[i]);
		status |= g_vga_status[i];
	}

	seq_printf(buf, "%d\n", status);
    return 0;
}

static int rear_status_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, rear_status_proc_read, NULL);
}

static int front_status_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, front_status_proc_read, NULL);
}

static const struct file_operations rear_status_fops = {
	.owner = THIS_MODULE,
	.open = rear_status_proc_open,
	.read = seq_read,
	//.write = status_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations front_status_fops = {
	.owner = THIS_MODULE,
	.open = front_status_proc_open,
	.read = seq_read,
	//.write = status_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_rear_status_proc_file(void)
{
    if(!g_camera_status_created) {   
        status_proc_file = proc_create(STATUS_REAR_PROC_FILE, 0666, NULL, &rear_status_fops);
		if(status_proc_file) {
			//pr_err("%s sucessed!\n", __func__);
			g_camera_status_created = 1;
	    } else {
			pr_err("%s failed!\n", __func__);
			g_camera_status_created = 0;
	    }  
    } else {  
        pr_info("File Exist!\n");  
    }  
	g_rear_camera_ref_cnt++;
}

static void create_front_status_proc_file(void)
{
	if(!g_front_camera_status_created) {
		status_proc_file = proc_create(STATUS_FRONT_PROC_FILE, 0666, NULL, &front_status_fops);
	
		if(status_proc_file) {
			//pr_err("%s sucessed!\n", __func__);
			g_front_camera_status_created = 1;
	    } else {
			pr_err("%s failed!\n", __func__);
			g_front_camera_status_created = 0;
	    }
	} else {
        pr_info("%s: File Exist!\n", __func__);
    }
	g_front_camera_ref_cnt++;
}


//ASUS_BSP Stimber_Hsueh +++
#define	TEMP_REAR_PROC_FILE	"driver/camera_temp"

static int rear_temp_proc_read(struct seq_file *buf, void *v)
{
	uint16_t temp = 0;
	int16_t s_temp = 0;
	s_sensor_read_temp(&temp);
	if( temp > 127 && temp < 256){
		s_temp = (int16_t)(temp - 256);
	}else{
		s_temp = temp;
	}
	seq_printf(buf, "%d\n", s_temp);
	//pr_err("Stimber: Temperature = %d\n", s_temp);

    return 0;
}

static int rear_temp_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, rear_temp_proc_read, NULL);
}

static const struct file_operations rear_temp_fops = {
	.owner = THIS_MODULE,
	.open = rear_temp_proc_open,
	.read = seq_read,
	//.write = status_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_rear_temp_proc_file(void)
{
	if(!g_rear_temp_created) {
        status_proc_file = proc_create(TEMP_REAR_PROC_FILE, 0666, NULL, &rear_temp_fops);
		if(status_proc_file) {
			//pr_err("Stimber: %s sucessed!\n", __func__);
			g_rear_temp_created = 1;
	    } else {
			pr_err("Stimber:%s failed!\n", __func__);
			g_rear_temp_created = 0;
	    }
    } else {
        pr_info("File Exist!\n");
    }
}
//ASUS_BSP Stimber_Hsueh ---


void remove_proc_file(void)
{
    extern struct proc_dir_entry proc_root;
    pr_info("remove_proc_file\n");	
    remove_proc_entry(STATUS_REAR_PROC_FILE, &proc_root);
	remove_proc_entry(STATUS_FRONT_PROC_FILE, &proc_root);
	remove_proc_entry(RESOLUTION_REAR_PROC_FILE, &proc_root);
	remove_proc_entry(RESOLUTION_FRONT_PROC_FILE, &proc_root);
	remove_proc_entry(MODULE_REAR_PROC_FILE, &proc_root);
	remove_proc_entry(MODULE_FRONT_PROC_FILE, &proc_root);
	remove_proc_entry(TEMP_REAR_PROC_FILE, &proc_root);
	memset(g_camera_status, 0, sizeof(g_camera_status));
	g_rear_camera_ref_cnt = -1;
	g_front_camera_ref_cnt = -1;
	g_camera_status_created = 0;
	g_front_camera_status_created = 0;
	g_rear_temp_created = 0;

	g_rear_resolution_created = 0;
	g_front_resolution_created = 0;
	g_rear_module_created = 0;
	g_front_module_created = 0;
}
//ASUS_BSP Surya_Xu

//ASUS_BSP Stimber_Hsueh +++
static int rear_resolution_proc_read(struct seq_file *buf, void *v)
{
    seq_printf(buf, "%s\n", g_rear_resolution);
				
    return 0;
}

static int rear_resolution_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, rear_resolution_proc_read, NULL);
}

static const struct file_operations rear_resolution_fops = {
	.owner = THIS_MODULE,
	.open = rear_resolution_proc_open,
	.read = seq_read,
	//.write = status_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_rear_resolution_proc_file(void)
{	
	if(!g_rear_resolution_created) {   
        status_proc_file = proc_create(RESOLUTION_REAR_PROC_FILE, 0666, NULL, &rear_resolution_fops);
		if(status_proc_file) {
			//pr_err("Stimber: %s sucessed!\n", __func__);
			g_rear_resolution_created = 1;
	    } else {
			pr_err("Stimber: %s failed!\n", __func__);
			g_rear_resolution_created = 0;
	    }  
    } else {  
        pr_info("File Exist!\n");  
    }  
}
//ASUS_BSP Stimber_Hsueh ---

//ASUS_BSP Stimber_Hsueh +++
static int front_resolution_proc_read(struct seq_file *buf, void *v)
{
    seq_printf(buf, "%s\n", g_front_resolution);
				
    return 0;
}

static int front_resolution_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, front_resolution_proc_read, NULL);
}

static const struct file_operations front_resolution_fops = {
	.owner = THIS_MODULE,
	.open = front_resolution_proc_open,
	.read = seq_read,
	//.write = status_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_front_resolution_proc_file(void)
{	
	if(!g_front_resolution_created) {   
        status_proc_file = proc_create(RESOLUTION_FRONT_PROC_FILE, 0666, NULL, &front_resolution_fops);
		if(status_proc_file) {
			//pr_err("Stimber: %s sucessed!\n", __func__);
			g_front_resolution_created = 1;
	    } else {
			pr_err("Stimber: %s failed!\n", __func__);
			g_front_resolution_created = 0;
	    }  
    } else {  
        pr_info("File Exist!\n");  
    }  
}
//ASUS_BSP Stimber_Hsueh ---

//ASUS_BSP Stimber_Hsueh +++
static int rear_module_proc_read(struct seq_file *buf, void *v)
{
    seq_printf(buf, "%s\n", g_rear_module);
				
    return 0;
}

static int rear_module_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, rear_module_proc_read, NULL);
}

static const struct file_operations rear_module_fops = {
	.owner = THIS_MODULE,
	.open = rear_module_proc_open,
	.read = seq_read,
	//.write = status_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_rear_module_proc_file(void)
{	
	if(!g_rear_module_created) {   
        status_proc_file = proc_create(MODULE_REAR_PROC_FILE, 0666, NULL, &rear_module_fops);
		if(status_proc_file) {
			//pr_err("Stimber: %s sucessed!\n", __func__);
			g_rear_module_created = 1;
	    } else {
			pr_err("Stimber:%s failed!\n", __func__);
			g_rear_module_created = 0;
	    }  
    } else {  
        pr_info("File Exist!\n");  
    }  
}

static int front_module_proc_read(struct seq_file *buf, void *v)
{
    seq_printf(buf, "%s\n", g_front_module);
				
    return 0;
}

static int front_module_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, front_module_proc_read, NULL);
}

static const struct file_operations front_module_fops = {
	.owner = THIS_MODULE,
	.open = front_module_proc_open,
	.read = seq_read,
	//.write = status_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_front_module_proc_file(void)
{	
	if(!g_front_module_created) {   
        status_proc_file = proc_create(MODULE_FRONT_PROC_FILE, 0666, NULL, &front_module_fops);
		if(status_proc_file) {
			//pr_err("Stimber: %s sucessed!\n", __func__);
			g_front_module_created = 1;
	    } else {
			pr_err("Stimber: %s failed!\n", __func__);
			g_front_module_created = 0;
	    }  
    } else {  
        pr_info("File Exist!\n");  
    }  
}
//ASUS_BSP Stimber_Hsueh ---
static struct msm_sensor_ctrl_t *g_ctrl;
static u16 g_reg_address,g_reg_val;
static int g_param_cnt=0;
void msm_debugfs_set_ctrl(struct msm_sensor_ctrl_t *s_ctrl)
{
	g_ctrl = s_ctrl;
	//pr_info("g_ctrl set to %p\n",g_ctrl);
}
static int sensor_register_read(struct seq_file *buf, void *v)
{
	int rc;
	if(g_ctrl==NULL)
	{
		seq_printf(buf, "sensor not power up!\n");
	}
	else if(g_param_cnt == 1)
	{
		seq_printf(buf, "sensor %s, read address 0x%X, val get is 0x%X\n",
					g_ctrl->sensordata->sensor_name,
					g_reg_address,
					g_reg_val
				  );
	}
	else if(g_param_cnt == 2)
	{
		rc = s_sensor_read_reg(g_ctrl, g_reg_address, &g_reg_val);
		if(rc < 0)
		{
			seq_printf(buf, "sensor %s, write address 0x%X, val read failed!\n",
						g_ctrl->sensordata->sensor_name,
						g_reg_address
					  );
		}
		else
		{
			seq_printf(buf, "sensor %s, write address 0x%X, val now is 0x%X\n",
						g_ctrl->sensordata->sensor_name,
						g_reg_address,
						g_reg_val
					  );
		}

	}
	else
	{
			seq_printf(buf, "sensor %s, echo address (and val) first!\n",
						g_ctrl->sensordata->sensor_name
					  );
	}

    return 0;
}
static int sensor_register_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, sensor_register_read, NULL);
}
#define DBG_TXT_BUF_SIZE 256
static ssize_t sensor_register_proc_write(struct file *dev, const char *buf, size_t count, loff_t *ppos)
{
	char debugTxtBuf[DBG_TXT_BUF_SIZE];
	uint reg = -1,value = -1;
	int rc, len;
	int param_count;

	if(g_ctrl == NULL)
	{
		pr_err("sensor not power up!\n");
		return count;
	}

	len=(count > DBG_TXT_BUF_SIZE-1)?(DBG_TXT_BUF_SIZE-1):(count);
	if (copy_from_user(debugTxtBuf,buf,len))
			return -EFAULT;

	debugTxtBuf[len]=0; //add string end
	param_count = sscanf(debugTxtBuf, "%x %x", &reg, &value);
	pr_info("sensor_register proc write input params cnt=%d, reg=0x%x value=0x%x\n", param_count,reg, value);
	*ppos=len;
	if (param_count == 2) {
		pr_info("sensor write reg=0x%04x value=0x%04x\n", reg, value);

		rc = s_sensor_write_reg(g_ctrl, reg, value);
		if (rc < 0) {
			pr_err("%s: failed to write 0x%x = 0x%x\n",
				 __func__, reg, value);
			return rc;
		}
		g_param_cnt = 2;
		g_reg_address = reg;

	}
	else if (param_count == 1) {
		rc = s_sensor_read_reg(g_ctrl, reg, (uint16_t*)&value);
		pr_info("sensor read reg=0x%x value=0x%x\n", reg, value);
		if (rc < 0) {
			pr_err("%s: failed to read 0x%x\n",
				 __func__, reg);
			return rc;
		}
		g_param_cnt = 1;
		g_reg_address = reg;
		g_reg_val = value;
	}
	return len;
}
static const struct file_operations sensor_register_debug_fops = {
	.owner = THIS_MODULE,
	.open = sensor_register_proc_open,
	.read = seq_read,
	.write = sensor_register_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};
static void create_sensor_register_debug_proc_file(void)
{
	if(!g_register_debug_created)
	{
		status_proc_file = proc_create(SENSOR_REGISTER_DEBUG, 0666, NULL, &sensor_register_debug_fops);
		if(status_proc_file)
		{
			//pr_err("Camera: %s sucessed!\n", __func__);
		}
		else
		{
			pr_err("Camera: %s failed!\n", __func__);
		}
		g_register_debug_created = 1;
	}
}


int msm_debugfs_init(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_camera_sensor_slave_info *slave_info)
{
	struct dentry *debugfs_dir;
	int camera_id = slave_info->camera_id;
	int chip_id = slave_info->sensor_id_info.sensor_id;
	const char *sensor_name = s_ctrl->sensordata->sensor_name;
	int index = number_of_camera++;
	char folder_name[10];

	if (index >= MAX_CAMERA) {
		pr_err("Invalid! number of camera (%d) > MAX Camera (%d)",
				number_of_camera, MAX_CAMERA);
		return -1;
	}
	sprintf(folder_name, "camera%d", index);
	debugfs_dir = debugfs_create_dir(folder_name, NULL);

	dbgfs[index].sensor_id = chip_id;

	debugfs_create_u8((camera_id ? "vga_status" : "camera_status"),
			0644, debugfs_dir, &dbgfs[index].status);
	debugfs_create_x16("sensor_id", 0644, debugfs_dir,
			&dbgfs[index].sensor_id);
	debugfs_create_u8("exposure_return0", 0666, debugfs_dir,
			&dbgfs[index].exposure_return0);
/*
	if (camera_id == 0) {
		(void) debugfs_create_file("vcm_test", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_vcm_test_fops);
		(void) debugfs_create_file("FT_vcm_test", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_FT_vcm_test_fops);
		(void) debugfs_create_file("ois_gyro_x", S_IRUGO,
			debugfs_dir, NULL, &dbg_ois_gyro_x_fops);
		(void) debugfs_create_file("ois_gyro_y", S_IRUGO,
			debugfs_dir, NULL, &dbg_ois_gyro_y_fops);
		if (g_ftm_mode) {
			(void) debugfs_create_file("ois_mode", S_IRUGO | S_IWUGO,
				debugfs_dir, NULL, &dbg_ois_mode_fops);
			(void) debugfs_create_file("ois_accel_gain", S_IRUGO | S_IWUGO,
				debugfs_dir, NULL, &dbg_ois_accel_gain_fops);
		} else {
			(void) debugfs_create_file("ois_mode", S_IRUGO,
				debugfs_dir, NULL, &dbg_ois_mode_fops);
			(void) debugfs_create_file("ois_accel_gain", S_IRUGO,
				debugfs_dir, NULL, &dbg_ois_accel_gain_fops);
		}
	}
*/
	if (!strcmp(sensor_name,"imx318")) {
		(void) debugfs_create_file("CameraOTP", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_imx318_otp_fops);
		(void) debugfs_create_file("Camera_Unique_ID", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_imx318_uid_fops);
		proc_create(REAR_OTP_PROC_FILE, 0664, NULL, &rear_otp_proc_fops);
	} else if (!strcmp(sensor_name,"ov8856")) {
		g_front_resolution = "8M";
		g_front_module = "OV8856";
		(void) debugfs_create_file("CameraOTP", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_ov8856_otp_fops);
		(void) debugfs_create_file("Camera_Unique_ID", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_ov8856_uid_fops);
		proc_create(FRONT_OTP_PROC_FILE, 0664, NULL, &front_otp_proc_fops);
		create_front_status_proc_file();
		create_front_resolution_proc_file();
		create_front_module_proc_file();
	} else if (!strcmp(sensor_name,"s5k3l8")) {
		g_rear_resolution = "13M";
		g_rear_module = "S5K3L8";
		/*(void) debugfs_create_file("CameraOTP", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_s5k3l8_otp_fops);
		(void) debugfs_create_file("Camera_Unique_ID", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_s5k3l8_uid_fops);
		proc_create(REAR_OTP_PROC_FILE, 0664, NULL, &rear_otp_proc_fops);*/
		create_rear_status_proc_file();
		create_rear_resolution_proc_file();
		create_rear_module_proc_file();
	} else if (!strcmp(sensor_name,"imx214")) {
		g_rear_resolution = "13M";
		g_rear_module = "IMX214";
		/*(void) debugfs_create_file("CameraOTP", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_imx214_otp_fops);
		(void) debugfs_create_file("Camera_Unique_ID", S_IRUGO,
			debugfs_dir, NULL, &dbg_dump_imx214_uid_fops);
		proc_create(REAR_OTP_PROC_FILE, 0664, NULL, &rear_otp_proc_fops);*/
		create_rear_otp_proc_file();
		create_rear_status_proc_file();
		create_rear_resolution_proc_file();
		create_rear_module_proc_file();
		create_rear_temp_proc_file();
		}
	create_sensor_register_debug_proc_file();

	return index;
}
