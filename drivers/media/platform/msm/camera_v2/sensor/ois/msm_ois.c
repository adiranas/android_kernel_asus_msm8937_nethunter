/* Copyright (c) 2014 - 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include "msm_sd.h"
#include "msm_ois.h"
#include "msm_cci.h"


DEFINE_MSM_MUTEX(msm_ois_mutex);
//#define MSM_OIS_DEBUG
#undef CDBG
#ifdef MSM_OIS_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

static struct v4l2_file_operations msm_ois_v4l2_subdev_fops;
static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *o_ctrl);
static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *o_ctrl);

static struct i2c_driver msm_ois_i2c_driver;

static int32_t ois_power_up(struct msm_ois_ctrl_t *o_ctrl);
static int32_t ois_power_down(struct msm_ois_ctrl_t *o_ctrl);
struct msm_ois_ctrl_t *OIS_ctrl;
static uint ois_power_state=0;
static uint32_t ois_reg_address=0;
static uint8_t ois_solo_up=0;
#define	OIS_PROC_REGISTER_FILE	"driver/OisRegister"
#define OIS_PROC_POWER_FILE "driver/OisPower"
#define OIS_PROC_GYRO_XY_FILE "driver/OisGyroXY"
#define OIS_PROC_GYRO_CAL_FILE "driver/OisGyroCal"

#define OIS_PROC_ATD_STATUS "driver/OisATDStatus"

#define DBG_TXT_BUF_SIZE 256

static int ATD_status=0;

struct clk *clk_ptr[2];
#define CAM_SENSOR_PINCTRL_STATE_SLEEP "cam_suspend"
#define CAM_SENSOR_PINCTRL_STATE_DEFAULT "cam_default"
static struct msm_pinctrl_info sensor_pctrl;
static struct msm_cam_clk_info cam_8937_clk_info[] = {
	[SENSOR_CAM_MCLK] = {"cam_src_clk", 19200000},
	[SENSOR_CAM_CLK] = {"cam_clk", 0},
};
int16_t H2D( uint16_t u16_inpdat ){
	int16_t s16_temp;
	s16_temp = u16_inpdat;
	if( u16_inpdat > 32767 ){
		s16_temp = (int16_t)(u16_inpdat - 65536L);
	}
	return s16_temp;
}
uint16_t D2H( int16_t s16_inpdat){
	uint16_t u16_temp;
	if( s16_inpdat < 0 ){
		u16_temp = (uint16_t)(s16_inpdat + 65536L);
	}
	else{
		u16_temp = s16_inpdat;
	}
	return u16_temp;
}
extern int isSensorPowerup;
int sensor_read_reg(struct msm_ois_ctrl_t *OIS_ctrl, u16 addr, u16 *val)
{
	int err;
	err = msm_camera_cci_i2c_read(&OIS_ctrl->i2c_client,addr,val,MSM_CAMERA_I2C_WORD_DATA);
	CDBG("sensor_read_reg 0x%x\n",*val);
	if(err <0)
		return -EINVAL;
	else return 0;
}
int sensor_write_reg_byte(struct msm_ois_ctrl_t *OIS_ctrl, u16 addr, u16 val)
{
	int err;
	int retry = 0;
	do {
		err =msm_camera_cci_i2c_write(&OIS_ctrl->i2c_client,addr,val,MSM_CAMERA_I2C_BYTE_DATA);
		if (err == 0)
			return 0;
		retry++;
		pr_err("Stimber : i2c transfer failed, retrying %x %x\n",
		       addr, val);
		msleep(1);
	} while (retry <= 10);

	if(err == 0) {
		pr_err("%s(%d): i2c_transfer error, but return 0!?\n", __FUNCTION__, __LINE__);
		err = 0xAAAA;
	}

	return err;
}
int sensor_write_reg(struct msm_ois_ctrl_t *OIS_ctrl, u16 addr, u16 val)
{
	int err;
	int retry = 0;
	do {
		err =msm_camera_cci_i2c_write(&OIS_ctrl->i2c_client,addr,val,MSM_CAMERA_I2C_WORD_DATA);
		if (err == 0)
			return 0;
		retry++;
		pr_err("Stimber : i2c transfer failed, retrying %x %x\n",
		       addr, val);
		msleep(1);
	} while (retry <= 10);

	if(err == 0) {
		pr_err("%s(%d): i2c_transfer error, but return 0!?\n", __FUNCTION__, __LINE__);
		err = 0xAAAA;
	}

	return err;
}

int sensor_write_reg_dword(struct msm_ois_ctrl_t *OIS_ctrl, u16 addr, u32 val)
{
	int32_t rc = -EFAULT;
	struct msm_camera_i2c_seq_reg_array *reg_setting;

	reg_setting =kzalloc(sizeof(struct msm_camera_i2c_seq_reg_array),GFP_KERNEL);
	if (!reg_setting)
		return -ENOMEM;

	reg_setting->reg_addr = addr;
	reg_setting->reg_data[0] = (uint8_t)((val & 0xFF000000) >> 24);
	reg_setting->reg_data[1] = (uint8_t)((val & 0x00FF0000) >> 16);
	reg_setting->reg_data[2] = (uint8_t)((val & 0x0000FF00) >> 8);
	reg_setting->reg_data[3] = (uint8_t)(val & 0x000000FF);
	reg_setting->reg_data_size = 4;

	rc = OIS_ctrl->i2c_client.i2c_func_tbl->
		i2c_write_seq(&OIS_ctrl->i2c_client,
		reg_setting->reg_addr,
		reg_setting->reg_data,
		reg_setting->reg_data_size);
	kfree(reg_setting);
	reg_setting = NULL;

	return rc;
}

static int read_offset_signed_data(uint32_t reg, int16_t* val)
{
	int rc;
	int retry = 0;
	uint16_t offset_raw;
	int16_t offset_val;

	do
	{
		rc = msm_camera_cci_i2c_read(&OIS_ctrl->i2c_client, reg, &offset_raw, MSM_CAMERA_I2C_WORD_DATA);
		if(rc!=0)
		{
			retry++;
			pr_err("%s(),cci_read failed! retry %d\n",__func__,retry);
			msleep(5);
		}
		else
		{
			offset_val=H2D(offset_raw);
			if((offset_val>2162)||(offset_val<-2162))
			{
				pr_err("%s(), offset_val %d not in range!\n",__func__,offset_val);
				return -2;
			}
			else
			{
				*val = offset_val;
				return 0;
			}
		}

	} while(retry<=3);

	pr_err("%s(),cci_read failed!\n",__func__);
	return -1;
}
static void swap_data(uint16_t* register_data)
{
	*register_data = ((*register_data >> 8) | ((*register_data & 0xff) << 8)) ;
}
static ssize_t ois_register_proc_write(struct file *dev, const char *buf, size_t count, loff_t *ppos)
{
	char debugTxtBuf[DBG_TXT_BUF_SIZE];

	int rc, len;
	uint32_t reg = -1,value = -1,type=-1;
	int param_count;

	len=(count > DBG_TXT_BUF_SIZE-1)?(DBG_TXT_BUF_SIZE-1):(count);

	if (copy_from_user(debugTxtBuf,buf,len))
			return -EFAULT;
	debugTxtBuf[len]=0; //add string end

	param_count = sscanf(debugTxtBuf, "%x %x %d", &reg, &value,&type);

	*ppos=len;

	if(param_count == 1)//read
	{
		pr_info("OIS going to read reg=0x%x\n",reg);
		ois_reg_address = reg;
		ATD_status = 1;
	}
	else if(param_count == 2)//swap word write
	{
		pr_info("OIS write reg=0x%04x value=0x%04x\n", reg, value);
		ois_reg_address = reg;
		swap_data((uint16_t*)&value);
		rc = sensor_write_reg(OIS_ctrl, reg, value);
		if (rc < 0) {
			pr_err("%s(), failed to write 0x%x = 0x%x\n",
				 __func__, reg, value);
			ATD_status = 0;
			return rc;
		}
		ATD_status = 1;
	}
	else if(param_count == 3)//normal write with TYPE option
	{
		ois_reg_address = reg;
		if(type == 0)//word write
		{
			//pr_info("OIS write reg=0x%04x value=0x%04x type=%d, WORD\n", reg, value,type);
			rc = sensor_write_reg(OIS_ctrl, reg, value);
			if (rc < 0) {
				pr_err("%s(), failed to write 0x%x = 0x%x\n",
					 __func__, reg, value);
				ATD_status = 0;
				return rc;
			}
		}
		else if(type == 1)//double word write
		{
			//pr_info("OIS write reg=0x%04x value=0x%04x type=%d, DWORD\n", reg, value,type);
			rc = sensor_write_reg_dword(OIS_ctrl, reg, value);
			if (rc < 0) {
				pr_err("%s(), failed to write 0x%x = 0x%x\n",
					 __func__, reg, value);
				ATD_status = 0;
				return rc;
			}
		}
		else if(type == 2)//byte
		{
			//pr_info("OIS write reg=0x%04x value=0x%04x type=%d, BYTE\n", reg, value,type);
			rc = sensor_write_reg_byte(OIS_ctrl, reg, value);
			if (rc < 0) {
				pr_err("%s(), failed to write 0x%x = 0x%x\n",
					 __func__, reg, value);
				ATD_status = 0;
				return rc;
			}
		}
		ATD_status = 1;
	}

	else
	{
		ATD_status = 0;
		pr_err("%s(), invalid parameters, count = %d\n",__func__,param_count);
	}

	return count;
}
#if 0
static int ois_register_proc_read(struct seq_file *buf, void *v)
{
		uint16_t chipid = 0;
		int rc = 0,i;
		int retry= 0;
		uint16_t offset_X=0,offset_Y=0;
		int16_t sum_x=0,sum_y=0,dat_x=0,dat_y=0;
		rc=msm_camera_cci_i2c_read(
		&OIS_ctrl->i2c_client, 0x8200,
		&chipid, MSM_CAMERA_I2C_WORD_DATA);
		pr_err("check status:0x%x\n", chipid);
		if(rc<0)
		{
			return 0;
		}
        for(i=0;i<16;i++)
        {
			msleep(5);
			do{
				rc=msm_camera_cci_i2c_read(
				&OIS_ctrl->i2c_client, 0x8455,
				&offset_X, MSM_CAMERA_I2C_WORD_DATA);
				dat_x=H2D(offset_X);
				if(rc!=0){
					if(retry==2){
						pr_err("cci_read failed!\n");
						seq_printf(buf, "-1\n");
						return 0;
						}
					retry++;
					msleep(5);
				}else {
					if((dat_x>2162)|(dat_x<-2162)){
						pr_err("offset_x not in range!\n");
						seq_printf(buf, "-2\n");
						return 0;
						}
					break;
				}
			}while(retry<=3);
			retry=0;
			do{
				rc=msm_camera_cci_i2c_read(
				&OIS_ctrl->i2c_client, 0x8456,
				&offset_Y, MSM_CAMERA_I2C_WORD_DATA);
				dat_y=H2D(offset_Y);
				if(rc!=0){
					if(retry==2){
						pr_err("cci_read failed!\n");
						seq_printf(buf, "-1\n");
						return 0;
						}
					retry++;
					msleep(5);
					}else{
						if((dat_y>2162)|(dat_y<-2162)){
						    pr_err("offset_y not in range!\n");
						    seq_printf(buf, "-2\n");
							return 0;
						}
					break;
					}
			}while(retry<=3);
			retry=0;
			sum_x+=dat_x;
			sum_y+=dat_y;
		}
		sum_x=sum_x/i;
		sum_y=sum_y/i;
		offset_X=D2H(sum_x);
		offset_Y=D2H(sum_y);
		seq_printf(buf, "0x%x 0x%x\n", offset_X,offset_Y);
        return 0;
}
#else

static int ois_register_proc_read(struct seq_file *buf, void *v)
{
	uint16_t value;
	int rc;
	if(ois_power_state)
	{
		rc = sensor_read_reg(OIS_ctrl, ois_reg_address, &value);
		if (rc < 0) {
			pr_err("%s(), failed to read 0x%x\n",__func__, ois_reg_address);
			ATD_status = 0;
			seq_printf(buf,"-9999\n");
			return rc;
		}
		seq_printf(buf,"0x%x 0x%x\n",ois_reg_address,value);
		ATD_status = 1;
	}
	else
	{
		seq_printf(buf,"ois not power up!\n");
		ATD_status = 0;
	}
	return 0;
}
#endif
static int ois_register_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, ois_register_proc_read, NULL);
}

static const struct file_operations ois_register_fops = {
	.owner = THIS_MODULE,
	.open = ois_register_proc_open,
	.read = seq_read,
	.write = ois_register_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};


static int ois_gyro_xy_proc_read(struct seq_file *buf, void *v)
{
	int16_t offset_X=-9999,offset_Y=-9999;

	int rc = -1;

	if(ois_power_state)
	{
		rc = read_offset_signed_data(0x8455,&offset_X);
		if(rc < 0)
		{
			ATD_status = 0;
			seq_printf(buf, "%d,%d\n", offset_X,offset_Y);
			return 0;
		}
		rc = read_offset_signed_data(0x8456,&offset_Y);
		if(rc < 0)
		{
			ATD_status = 0;
			seq_printf(buf, "%d,%d\n", offset_X,offset_Y);
			return 0;
		}

		ATD_status = 1;
		seq_printf(buf, "%d,%d\n", offset_X,offset_Y);
	}
	else
	{
		seq_printf(buf, "ois not powered up!\n");
		ATD_status = 0;
	}

	return 0;
}

static int ois_gyro_xy_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, ois_gyro_xy_proc_read, NULL);
}

static ssize_t ois_gyro_xy_proc_write(struct file *dev, const char *buf, size_t count, loff_t *ppos)
{
	return count;
}

static const struct file_operations ois_gyro_xy_fops = {
	.owner = THIS_MODULE,
	.open = ois_gyro_xy_proc_open,
	.read = seq_read,
	.write = ois_gyro_xy_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ois_gyro_cal_proc_read(struct seq_file *buf, void *v)
{
	int rc,i;

	int16_t read_x,read_y;
	int16_t sum_x,sum_y,mean_x,mean_y,min_x,min_y,max_x,max_y;

	min_x = min_y = max_x = max_y = -2162;
	sum_x = sum_y = 0;

	if(ois_power_state)
	{
        for(i=0;i<16;i++)
        {
			rc = read_offset_signed_data(0x8455,&read_x);
			if(rc < 0)
			{
				seq_printf(buf, "%d\n",rc);
				ATD_status = 0;
				return 0;
			}

			rc = read_offset_signed_data(0x8456,&read_y);
			if(rc < 0)
			{
				seq_printf(buf, "%d\n",rc);
				ATD_status = 0;
				return 0;
			}

			if(read_x < min_x)
				min_x = read_x;
			if(read_y < min_y)
				min_y = read_y;

			if(read_x > max_x)
				max_x = read_x;
			if(read_y > max_y)
				max_y = read_y;

			sum_x += read_x;
			sum_y += read_y;
		}

		mean_x = sum_x/i;
		mean_y = sum_y/i;
		//D2H
#if 0
		seq_printf(buf, "0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
					D2H(mean_x),D2H(mean_y),
					D2H(max_x),D2H(max_y),
					D2H(min_x),D2H(min_y)
				  );
#else
		seq_printf(buf, "%d %d %d %d %d %d\n",
					mean_x,mean_y,
					max_x,max_y,
					min_x,min_y
				  );
#endif
		ATD_status = 1;
        return 0;
	}
	else
	{
		seq_printf(buf, "ois not powered up!\n");
		ATD_status = 0;
		return 0;
	}
}

static int ois_gyro_cal_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, ois_gyro_cal_proc_read, NULL);
}

static ssize_t ois_gyro_cal_proc_write(struct file *dev, const char *buf, size_t count, loff_t *ppos)
{
	return count;
}

static const struct file_operations ois_gyro_cal_fops = {
	.owner = THIS_MODULE,
	.open = ois_gyro_cal_proc_open,
	.read = seq_read,
	.write = ois_gyro_cal_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};


static int ois_ATD_status_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n",ATD_status);
	ATD_status = 0;//auto reset
	return 0;
}

static int ois_ATD_status_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, ois_ATD_status_proc_read, NULL);
}

static ssize_t ois_ATD_status_proc_write(struct file *dev, const char *buf, size_t count, loff_t *ppos)
{
	char kbuf[8];
	int  set_val = -1;
	int  rc = 0;

	rc = count;

	if(count > 8)
		count = 8;

	if(copy_from_user(kbuf, buf, count))
	{
		pr_err("%s(): copy_from_user fail !\n",__func__);
		return -EFAULT;
	}

	sscanf(kbuf, "%d", &set_val);

	ATD_status = set_val;

	pr_info("OIS ATD_status set to %d\n",ATD_status);

	return rc;

}

static const struct file_operations ois_ATD_status_fops = {
	.owner = THIS_MODULE,
	.open = ois_ATD_status_proc_open,
	.read = seq_read,
	.write = ois_ATD_status_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};


static ssize_t ois_power_proc_write(struct file *dev, const char *buf, size_t count, loff_t *ppos)
{
	int rc, len;
	uint value=-1;
	char string[DBG_TXT_BUF_SIZE];
	len=(count > DBG_TXT_BUF_SIZE-1)?(DBG_TXT_BUF_SIZE-1):(count);

	if (copy_from_user(string,buf,len))
			return -EFAULT;
	string[len]=0; //add string end

	sscanf(string, "%d", &value);
	*ppos=len;

	if(value)
	{
		rc=ois_power_up(OIS_ctrl);
		if (rc < 0)
		{
			pr_err("%s():%d OIS Power up failed\n",__func__, __LINE__);
			ATD_status = 0;
		}
		else
		{
			ATD_status = 1;
		}
	}
	else
	{
		rc=ois_power_down(OIS_ctrl);
		if (rc < 0)
		{
			pr_err("%s():%d OIS Power down failed\n",__func__, __LINE__);
			ATD_status = 0;
		}
		else
		{
			ATD_status = 1;
		}
	}

	return count;
}
static int ois_power_proc_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n",ois_power_state);
	return 0;
}

static int ois_power_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, ois_power_proc_read, NULL);
}

static const struct file_operations ois_power_fops = {
	.owner = THIS_MODULE,
	.open = ois_power_proc_open,
	.read = seq_read,
	.write = ois_power_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};
static void create_proc_file(const char *PATH,const struct file_operations* f_ops)
{
	struct proc_dir_entry *fd;

	fd = proc_create(PATH, 0666, NULL, f_ops);
	if(fd)
	{
		//pr_info("%s(%s) succeeded!\n", __func__,PATH);
	}
	else
	{
		pr_err("%s(%s) failed!\n", __func__,PATH);
	}
}
static void create_ois_proc_files(void)
{
	static uint8_t has_created = 0;

	if(!has_created)
	{
		create_proc_file(OIS_PROC_POWER_FILE,&ois_power_fops);
		create_proc_file(OIS_PROC_REGISTER_FILE,&ois_register_fops);
		create_proc_file(OIS_PROC_GYRO_XY_FILE,&ois_gyro_xy_fops);
		create_proc_file(OIS_PROC_GYRO_CAL_FILE,&ois_gyro_cal_fops);
		create_proc_file(OIS_PROC_ATD_STATUS,&ois_ATD_status_fops);
		has_created = 1;
	}
	else
	{
		pr_err("%s(), OIS proc files have already created!\n",__func__);
	}
}

static int32_t msm_ois_write_settings(struct msm_ois_ctrl_t *o_ctrl,
	uint16_t size, struct reg_settings_ois_t *settings)
{
	int32_t rc = -EFAULT;
	int32_t i = 0;
	struct msm_camera_i2c_seq_reg_array *reg_setting;
	CDBG("Enter\n");

	for (i = 0; i < size; i++) {

		switch (settings[i].i2c_operation) {
		case MSM_OIS_WRITE: {
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_BYTE_DATA:
			case MSM_CAMERA_I2C_WORD_DATA:
				rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_write(
					&o_ctrl->i2c_client,
					settings[i].reg_addr,
					settings[i].reg_data,
					settings[i].data_type);
				break;
			case MSM_CAMERA_I2C_DWORD_DATA:
			reg_setting =
			kzalloc(sizeof(struct msm_camera_i2c_seq_reg_array),
				GFP_KERNEL);
				if (!reg_setting)
					return -ENOMEM;

				reg_setting->reg_addr = settings[i].reg_addr;
				reg_setting->reg_data[0] = (uint8_t)
					((settings[i].reg_data &
					0xFF000000) >> 24);
				reg_setting->reg_data[1] = (uint8_t)
					((settings[i].reg_data &
					0x00FF0000) >> 16);
				reg_setting->reg_data[2] = (uint8_t)
					((settings[i].reg_data &
					0x0000FF00) >> 8);
				reg_setting->reg_data[3] = (uint8_t)
					(settings[i].reg_data & 0x000000FF);
				reg_setting->reg_data_size = 4;
				rc = o_ctrl->i2c_client.i2c_func_tbl->
					i2c_write_seq(&o_ctrl->i2c_client,
					reg_setting->reg_addr,
					reg_setting->reg_data,
					reg_setting->reg_data_size);
				kfree(reg_setting);
				reg_setting = NULL;
				if (rc < 0)
					return rc;
				break;

			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
			if (settings[i].delay > 20)
				msleep(settings[i].delay);
			else if (0 != settings[i].delay)
				usleep_range(settings[i].delay * 1000,
					(settings[i].delay * 1000) + 1000);
		}
			break;

		case MSM_OIS_POLL: {
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_BYTE_DATA:
			case MSM_CAMERA_I2C_WORD_DATA:

				rc = o_ctrl->i2c_client.i2c_func_tbl
					->i2c_poll(&o_ctrl->i2c_client,
					settings[i].reg_addr,
					settings[i].reg_data,
					settings[i].data_type,
					settings[i].delay);
				break;

			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
		}
		}

		if (rc < 0)
			break;
	}

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_vreg_control(struct msm_ois_ctrl_t *o_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_ois_vreg *vreg_cfg;

	vreg_cfg = &o_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_OIS_MAX_VREGS) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(&(o_ctrl->pdev->dev),
			&vreg_cfg->cam_vreg[i],
			(struct regulator **)&vreg_cfg->data[i],
			config);
	}
	return rc;
}

void msm_cam_clk_disable(void)
{
	int i;
	for(i=1;i>=0;i--)
	 {
		if (clk_ptr[i] != NULL){
		clk_disable(clk_ptr[i]);
		clk_unprepare(clk_ptr[i]);
		clk_put(clk_ptr[i]);
	  }
   }
}
static int32_t ois_power_down(struct msm_ois_ctrl_t *o_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	CDBG("Enter\n");

	if(ois_power_state && !isSensorPowerup){//camera not open, solo up

		//camera open, solo up & down, not need constraint, should, solo up will skip...
		rc = msm_ois_vreg_control(o_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		if(!isSensorPowerup)
		{
			pr_info("OIS, sensor Not opened, disable clk and pins\n");
			rc=msm_sensor_cci_i2c_util(&OIS_ctrl->i2c_client,MSM_CCI_RELEASE);
			o_ctrl->i2c_tbl_index = 0;
			pr_err("%s disable cam_src_clk\n", __func__);
			msm_cam_clk_disable();
			rc = pinctrl_select_state(sensor_pctrl.pinctrl,
				sensor_pctrl.gpio_state_suspend);
			if (rc)
				pr_err("%s:%d cannot set pin to suspend state\n",
					__func__, __LINE__);
			devm_pinctrl_put(sensor_pctrl.pinctrl);
		}
		else
		{
			pr_info("OIS, sensor opened, NOT disable clk and pins\n");
		}
		rc=msm_camera_cci_i2c_read(
			&OIS_ctrl->i2c_client, 0x8200,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
		if(rc==0)
		{
			pr_err("register 0x8200 :0x%x\n", chipid);
			pr_err("ois_power_down: ois NOT really power down, sensor is up\n");
		}
		else
		{
			ois_power_state = 0;
			ois_solo_up = 0;
			pr_err("ois_power_down: ois really power down!\n");
			rc = 0;//for proc write, identify success
		}

	}
	else if(!ois_power_state)
	{
		pr_err("ois already powered down!\n");
	}
	else
	{
		pr_err("camera running! not solo down\n");
	}
	CDBG("Exit\n");
	return rc;
}
static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *o_ctrl)
{
	int32_t rc = 0;
	//uint16_t chipid = 0;
	enum msm_sensor_power_seq_gpio_t gpio;

	CDBG("Enter\n");
	if (o_ctrl->ois_state != OIS_DISABLE_STATE) {

		if(ois_solo_up)
		{
			pr_info("OIS solo up, set pinctrl to suspend\n");
			rc = pinctrl_select_state(sensor_pctrl.pinctrl,
				sensor_pctrl.gpio_state_suspend);
			if (rc)
				pr_err("%s:%d cannot set pin to suspend state\n",
					__func__, __LINE__);
			devm_pinctrl_put(sensor_pctrl.pinctrl);
			if (rc)
				pr_err("%s:%d cannot set pin to active state\n",
				__func__, __LINE__);
		}
		//call this in pair, solo -> should call ois_power_down; not solo, OK
		rc = msm_ois_vreg_control(o_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		for (gpio = SENSOR_GPIO_AF_PWDM; gpio < SENSOR_GPIO_MAX;
			gpio++) {
			if (o_ctrl->gconf &&
				o_ctrl->gconf->gpio_num_info &&
				o_ctrl->gconf->
					gpio_num_info->valid[gpio] == 1) {
				gpio_set_value_cansleep(
					o_ctrl->gconf->gpio_num_info
						->gpio_num[gpio],
					GPIOF_OUT_INIT_LOW);
				pr_err("msm_ois_power_down request GPIO index %d\n",gpio);
				if (o_ctrl->cam_pinctrl_status) {
					rc = pinctrl_select_state(
						o_ctrl->pinctrl_info.pinctrl,
						o_ctrl->pinctrl_info.
							gpio_state_suspend);
					if (rc < 0)
						pr_err("ERR:%s:%d cannot set pin to suspend state: %d\n",
							__func__, __LINE__, rc);
					devm_pinctrl_put(
						o_ctrl->pinctrl_info.pinctrl);
				}
				o_ctrl->cam_pinctrl_status = 0;
				rc = msm_camera_request_gpio_table(
					o_ctrl->gconf->cam_gpio_req_tbl,
					o_ctrl->gconf->cam_gpio_req_tbl_size,
					0);
				if (rc < 0)
					pr_err("ERR:%s:Failed in selecting state in ois power down: %d\n",
						__func__, rc);
			}
		}
#if 0
		rc=msm_camera_cci_i2c_read(
			&OIS_ctrl->i2c_client, 0x8200,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
		if(rc==0)
		{
			pr_err("register 0x8200 :0x%x\n", chipid);
			pr_err("msm_ois_power_down: ois NOT really powered down? not likely..\n");
		}
		else
		{
			o_ctrl->i2c_tbl_index = 0;
			o_ctrl->ois_state = OIS_OPS_INACTIVE;
			ois_power_state=0;
			pr_err("msm_ois_power_down: ois really powered down!\n");
		}
#else
		o_ctrl->i2c_tbl_index = 0;
		o_ctrl->ois_state = OIS_OPS_INACTIVE;
		ois_power_state=0;
		pr_err("OIS POWER DOWN EXIT\n");
		//not check i2c status, since regulator will close later in sensor_close
		//
#endif

	}
	CDBG("Exit\n");
	return rc;
}

static int msm_ois_init(struct msm_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	CDBG("Enter\n");

	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}

	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	o_ctrl->ois_state = OIS_OPS_ACTIVE;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_control(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info)
{
	struct reg_settings_ois_t *settings = NULL;
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	CDBG("Enter\n");

	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = o_ctrl->i2c_client.cci_client;
		cci_client->sid =
			set_info->ois_params.i2c_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->cci_i2c_master = o_ctrl->cci_master;
		cci_client->i2c_freq_mode = set_info->ois_params.i2c_freq_mode;
	} else {
		o_ctrl->i2c_client.client->addr =
			set_info->ois_params.i2c_addr;
	}
	o_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;


	if (set_info->ois_params.setting_size > 0 &&
		set_info->ois_params.setting_size
		< MAX_OIS_REG_SETTINGS) {
		settings = kmalloc(
			sizeof(struct reg_settings_ois_t) *
			(set_info->ois_params.setting_size),
			GFP_KERNEL);
		if (settings == NULL) {
			pr_err("Error allocating memory\n");
			return -EFAULT;
		}
		if (copy_from_user(settings,
			(void *)set_info->ois_params.settings,
			set_info->ois_params.setting_size *
			sizeof(struct reg_settings_ois_t))) {
			kfree(settings);
			pr_err("Error copying\n");
			return -EFAULT;
		}

		rc = msm_ois_write_settings(o_ctrl,
			set_info->ois_params.setting_size,
			settings);
		kfree(settings);
		if (rc < 0) {
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");

	return rc;
}


static int32_t msm_ois_config(struct msm_ois_ctrl_t *o_ctrl,
	void __user *argp)
{
	struct msm_ois_cfg_data *cdata =
		(struct msm_ois_cfg_data *)argp;
	int32_t rc = 0;
	mutex_lock(o_ctrl->ois_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_OIS_INIT:
		rc = msm_ois_init(o_ctrl);
		if (rc < 0)
			pr_err("msm_ois_init failed %d\n", rc);
		break;
	case CFG_OIS_POWERDOWN:
		rc = msm_ois_power_down(o_ctrl);
		if (rc < 0)
			pr_err("msm_ois_power_down failed %d\n", rc);
		break;
	case CFG_OIS_POWERUP:
		rc = msm_ois_power_up(o_ctrl);
		if (rc < 0)
			pr_err("Failed ois power up%d\n", rc);
		break;
	case CFG_OIS_CONTROL:
		rc = msm_ois_control(o_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("Failed ois control%d\n", rc);
		break;
	case CFG_OIS_I2C_WRITE_SEQ_TABLE: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

#ifdef CONFIG_COMPAT
		if (is_compat_task()) {
			memcpy(&conf_array,
				(void *)cdata->cfg.settings,
				sizeof(struct msm_camera_i2c_seq_reg_setting));
		} else
#endif
		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.settings,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = o_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_seq_table(&o_ctrl->i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}
	default:
		break;
	}
	mutex_unlock(o_ctrl->ois_mutex);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_get_subdev_id(struct msm_ois_ctrl_t *o_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = o_ctrl->pdev->id;
	else
		*subdev_id = o_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq = msm_camera_qup_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static int msm_ois_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_ois_ctrl_t *o_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	mutex_lock(o_ctrl->ois_mutex);
	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE &&
		o_ctrl->ois_state != OIS_DISABLE_STATE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	o_ctrl->ois_state = OIS_DISABLE_STATE;
	mutex_unlock(o_ctrl->ois_mutex);
	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_ois_internal_ops = {
	.close = msm_ois_close,
};

static long msm_ois_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc;
	struct msm_ois_ctrl_t *o_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d o_ctrl %pK argp %pK\n", __func__, __LINE__, o_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_ois_get_subdev_id(o_ctrl, argp);
	case VIDIOC_MSM_OIS_CFG:
		return msm_ois_config(o_ctrl, argp);
	case MSM_SD_SHUTDOWN:
		if (!o_ctrl->i2c_client.i2c_func_tbl) {
			pr_err("o_ctrl->i2c_client.i2c_func_tbl NULL\n");
			return -EINVAL;
		}
		rc = msm_ois_power_down(o_ctrl);
		if (rc < 0) {
			pr_err("%s:%d OIS Power down failed\n",
				__func__, __LINE__);
		}
		return msm_ois_close(sd, NULL);
	default:
		return -ENOIOCTLCMD;
	}
}

static int msm_ois_pinctrl_init(struct msm_ois_ctrl_t *o_ctrl)
{
		sensor_pctrl.pinctrl = devm_pinctrl_get(&(o_ctrl->pdev->dev));
		if (IS_ERR_OR_NULL(sensor_pctrl.pinctrl)) {
				pr_err("%s:%d Getting pinctrl handle failed\n",
				__func__, __LINE__);
				return -EINVAL;
		}
		sensor_pctrl.gpio_state_active =pinctrl_lookup_state(sensor_pctrl.pinctrl,
			CAM_SENSOR_PINCTRL_STATE_DEFAULT);
		if (IS_ERR_OR_NULL(sensor_pctrl.gpio_state_active)) {
				pr_err("%s:%d Failed to get the active state pinctrl handle\n",__func__, __LINE__);
				return -EINVAL;
		}
		sensor_pctrl.gpio_state_suspend= pinctrl_lookup_state(sensor_pctrl.pinctrl,
			CAM_SENSOR_PINCTRL_STATE_SLEEP);
		if (IS_ERR_OR_NULL(sensor_pctrl.gpio_state_suspend)) {
				pr_err("%s:%d Failed to get the suspend state pinctrl handle\n",__func__, __LINE__);
				return -EINVAL;
		}
		return 0;
}
int msm_ois_clk_enable(struct msm_ois_ctrl_t *o_ctrl)
{
		int rc=0,i;
		long clk_rate;
		for (i = 0; i < 2; i++) {
			pr_err("%s ois enable %s\n", __func__, cam_8937_clk_info[i].clk_name);
			clk_ptr[i] = clk_get(&(o_ctrl->pdev->dev), cam_8937_clk_info[i].clk_name);
			if (IS_ERR(clk_ptr[i])) {
				pr_err("%s get failed\n", cam_8937_clk_info[i].clk_name);
			}
			if (cam_8937_clk_info[i].clk_rate > 0) {
				pr_err(" clk_rate=%ld\n",cam_8937_clk_info[i].clk_rate);
				clk_rate = clk_round_rate(clk_ptr[i],
				cam_8937_clk_info[i].clk_rate);
				if (clk_rate < 0) {
					pr_err("%s round failed\n",
					cam_8937_clk_info[i].clk_name);
					return -1;
				}
				rc = clk_set_rate(clk_ptr[i],clk_rate);
				if (rc < 0) {
					pr_err("%s set failed\n",
					cam_8937_clk_info[i].clk_name);
					return -1;
				}
			} else if (cam_8937_clk_info[i].clk_rate == INIT_RATE) {
					pr_err(" INIT_RATE=%ld\n",cam_8937_clk_info[i].clk_rate);
					clk_rate = clk_get_rate(clk_ptr[i]);
				if (clk_rate == 0) {
					clk_rate =clk_round_rate(clk_ptr[i], 0);
					if (clk_rate < 0) {
						pr_err("%s round rate failed\n",
						cam_8937_clk_info[i].clk_name);
						return -1;
						}
					rc = clk_set_rate(clk_ptr[i],clk_rate);
					if (rc < 0) {
						pr_err("%s set rate failed\n",
						cam_8937_clk_info[i].clk_name);
						return -1;
					}
				}
			}
			pr_err(" clk_rate1111=%ld\n",cam_8937_clk_info[i].clk_rate);
			rc = clk_prepare(clk_ptr[i]);
			if (rc < 0) {
				pr_err("%s prepare failed\n",
				cam_8937_clk_info[i].clk_name);
				return -1;
			}
			rc = clk_enable(clk_ptr[i]);
			if (rc < 0) {
				pr_err("%s enable failed\n",
				cam_8937_clk_info[i].clk_name);
				return -1;
			}
	}
		msleep(1);
		return rc;
}

static int32_t ois_power_up(struct msm_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	uint16_t chipid = 0;
	pr_err("%s called\n", __func__);

	if(!ois_power_state){// camera not open ...

		rc = msm_ois_vreg_control(o_ctrl, 1);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}
		if(!isSensorPowerup)
		{
			pr_info("OIS, sensor Not opened, enable pins and clk\n");
			rc=msm_ois_pinctrl_init(o_ctrl);
			rc= pinctrl_select_state(sensor_pctrl.pinctrl,
			sensor_pctrl.gpio_state_active);
			if (rc)
				pr_err("%s:%d cannot set pin to active state\n",
				__func__, __LINE__);
			msm_ois_clk_enable(o_ctrl);
			rc=msm_sensor_cci_i2c_util(&OIS_ctrl->i2c_client,MSM_CCI_INIT);
			if (rc < 0) {
				pr_err("%s cci_init failed\n", __func__);
				return -1;
			}
		}
		else
		{
			pr_info("OIS, sensor opened, no need enable pins and clk again!\n");
		}
		rc=msm_camera_cci_i2c_read(
			&OIS_ctrl->i2c_client, 0x8200,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
		pr_err("register  0x8200 :0x%x\n", chipid);

		if(rc==0)
		{
			ois_power_state = 1;
			ois_solo_up = 1;
			pr_err("ois_power_up succeed!\n");
		}
		else
		{
			pr_err("ois_power_up failed!\n");
		}

	}else{
		pr_err("%s(), ois already powered up!\n",__func__);
	}
	pr_err("ois_power_up Exit\n");
	return rc;
}
static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	uint16_t chipid = 0;
	enum msm_sensor_power_seq_gpio_t gpio;

	CDBG("%s called\n", __func__);

	if(!ois_power_state)//equal to !solo up
	{
		//call this in pair, not solo
		rc = msm_ois_vreg_control(o_ctrl, 1);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		for (gpio = SENSOR_GPIO_AF_PWDM;
			gpio < SENSOR_GPIO_MAX; gpio++) {
			if (o_ctrl->gconf && o_ctrl->gconf->gpio_num_info &&
				o_ctrl->gconf->gpio_num_info->valid[gpio] == 1) {
				pr_err("msm_ois_power_up request GPIO index %d\n",gpio);
				rc = msm_camera_request_gpio_table(
					o_ctrl->gconf->cam_gpio_req_tbl,
					o_ctrl->gconf->cam_gpio_req_tbl_size, 1);
				if (rc < 0) {
					pr_err("ERR:%s:Failed in selecting state for ois: %d\n",
						__func__, rc);
					return rc;
				}
				if (o_ctrl->cam_pinctrl_status) {
					rc = pinctrl_select_state(
						o_ctrl->pinctrl_info.pinctrl,
						o_ctrl->pinctrl_info.gpio_state_active);
					if (rc < 0)
						pr_err("ERR:%s:%d cannot set pin to active state: %d",
							__func__, __LINE__, rc);
				}

				gpio_set_value_cansleep(
					o_ctrl->gconf->gpio_num_info->gpio_num[gpio],
					1);
			}
		}

		rc=msm_camera_cci_i2c_read(
			&OIS_ctrl->i2c_client, 0x8200,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
		pr_err("register  0x8200 :0x%x\n", chipid);

		if(rc==0)
		{
			o_ctrl->ois_state = OIS_ENABLE_STATE;
			ois_power_state=1;
			pr_err("msm_ois_power_up succeed!\n");
			pr_err("OIS POWER UP EXIT\n");
		}
		else
		{
			pr_err("msm_ois_power_up failed, i2c can not read!\n");
		}
	}
	else
	{
		pr_err("ois already powered up!\n");
	}

	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_ois_subdev_core_ops = {
	.ioctl = msm_ois_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_ois_subdev_ops = {
	.core = &msm_ois_subdev_core_ops,
};

static const struct i2c_device_id msm_ois_i2c_id[] = {
	{"qcom,ois", (kernel_ulong_t)NULL},
	{ }
};

static int32_t msm_ois_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_ois_ctrl_t *ois_ctrl_t = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("msm_ois_i2c_probe: client is null\n");
		return -EINVAL;
	}

	ois_ctrl_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
		GFP_KERNEL);
	if (!ois_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		rc = -EINVAL;
		goto probe_failure;
	}

	CDBG("client = 0x%pK\n",  client);

	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&ois_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", ois_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto probe_failure;
	}

	ois_ctrl_t->i2c_driver = &msm_ois_i2c_driver;
	ois_ctrl_t->i2c_client.client = client;
	/* Set device type as I2C */
	ois_ctrl_t->ois_device_type = MSM_CAMERA_I2C_DEVICE;
	ois_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
	ois_ctrl_t->ois_mutex = &msm_ois_mutex;

	/* Assign name for sub device */
	snprintf(ois_ctrl_t->msm_sd.sd.name, sizeof(ois_ctrl_t->msm_sd.sd.name),
		"%s", ois_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&ois_ctrl_t->msm_sd.sd,
		ois_ctrl_t->i2c_client.client,
		ois_ctrl_t->ois_v4l2_subdev_ops);
	v4l2_set_subdevdata(&ois_ctrl_t->msm_sd.sd, ois_ctrl_t);
	ois_ctrl_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
	ois_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&ois_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	ois_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ois_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
	ois_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&ois_ctrl_t->msm_sd);
	ois_ctrl_t->ois_state = OIS_DISABLE_STATE;
	pr_info("msm_ois_i2c_probe: succeeded\n");
	CDBG("Exit\n");

probe_failure:
	kfree(ois_ctrl_t);
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_ois_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	long rc = 0;
	struct video_device *vdev;
	struct v4l2_subdev *sd;
	struct msm_ois_cfg_data32 *u32;
	struct msm_ois_cfg_data ois_data;
	void *parg;
	struct msm_camera_i2c_seq_reg_setting settings;
	struct msm_camera_i2c_seq_reg_setting32 settings32;

	if (!file || !arg) {
		pr_err("%s:failed NULL parameter\n", __func__);
		return -EINVAL;
	}
	vdev = video_devdata(file);
	sd = vdev_to_v4l2_subdev(vdev);
	u32 = (struct msm_ois_cfg_data32 *)arg;
	parg = arg;

	ois_data.cfgtype = u32->cfgtype;

	switch (cmd) {
	case VIDIOC_MSM_OIS_CFG32:
		cmd = VIDIOC_MSM_OIS_CFG;

		switch (u32->cfgtype) {
		case CFG_OIS_CONTROL:
			ois_data.cfg.set_info.ois_params.setting_size =
				u32->cfg.set_info.ois_params.setting_size;
			ois_data.cfg.set_info.ois_params.i2c_addr =
				u32->cfg.set_info.ois_params.i2c_addr;
			ois_data.cfg.set_info.ois_params.i2c_freq_mode =
				u32->cfg.set_info.ois_params.i2c_freq_mode;
			ois_data.cfg.set_info.ois_params.i2c_addr_type =
				u32->cfg.set_info.ois_params.i2c_addr_type;
			ois_data.cfg.set_info.ois_params.i2c_data_type =
				u32->cfg.set_info.ois_params.i2c_data_type;
			ois_data.cfg.set_info.ois_params.settings =
				compat_ptr(u32->cfg.set_info.ois_params.
				settings);
			parg = &ois_data;
			break;
		case CFG_OIS_I2C_WRITE_SEQ_TABLE:
			if (copy_from_user(&settings32,
				(void *)compat_ptr(u32->cfg.settings),
				sizeof(
				struct msm_camera_i2c_seq_reg_setting32))) {
				pr_err("copy_from_user failed\n");
				return -EFAULT;
			}

			settings.addr_type = settings32.addr_type;
			settings.delay = settings32.delay;
			settings.size = settings32.size;
			settings.reg_setting =
				compat_ptr(settings32.reg_setting);

			ois_data.cfgtype = u32->cfgtype;
			ois_data.cfg.settings = &settings;
			parg = &ois_data;
			break;
		default:
			parg = &ois_data;
			break;
		}
	}
	rc = msm_ois_subdev_ioctl(sd, cmd, parg);

	return rc;
}

static long msm_ois_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_ois_subdev_do_ioctl);
}
#endif

static int32_t msm_ois_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_ois_ctrl_t *msm_ois_t = NULL;
	struct msm_ois_vreg *vreg_cfg;
	CDBG("Enter\n");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_ois_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
		GFP_KERNEL);
	if (!msm_ois_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		kfree(msm_ois_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_ois_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_ois_t->cci_master, rc);
	if (rc < 0 || msm_ois_t->cci_master >= MASTER_MAX) {
		kfree(msm_ois_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	if (of_find_property((&pdev->dev)->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &msm_ois_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(msm_ois_t);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}

	rc = msm_sensor_driver_get_gpio_data(&(msm_ois_t->gconf),
		(&pdev->dev)->of_node);
	if (rc < 0) {
		pr_err("%s: No/Error OIS GPIO\n", __func__);
	} else {
		msm_ois_t->cam_pinctrl_status = 1;
		rc = msm_camera_pinctrl_init(
			&(msm_ois_t->pinctrl_info), &(pdev->dev));
		if (rc < 0) {
			pr_err("ERR:%s: Error in reading OIS pinctrl\n",
				__func__);
			msm_ois_t->cam_pinctrl_status = 0;
		}
		devm_pinctrl_put(msm_ois_t->pinctrl_info.pinctrl);
	}

	msm_ois_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
	msm_ois_t->ois_mutex = &msm_ois_mutex;

	/* Set platform device handle */
	msm_ois_t->pdev = pdev;

	/* Set device type as platform device */
	msm_ois_t->ois_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_ois_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_ois_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_ois_t->i2c_client.cci_client) {
		kfree(msm_ois_t->vreg_cfg.cam_vreg);
		kfree(msm_ois_t);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = msm_ois_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = msm_ois_t->cci_master;
	v4l2_subdev_init(&msm_ois_t->msm_sd.sd,
		msm_ois_t->ois_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_ois_t->msm_sd.sd, msm_ois_t);
	msm_ois_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
	msm_ois_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_ois_t->msm_sd.sd.name,
		ARRAY_SIZE(msm_ois_t->msm_sd.sd.name), "msm_ois");
	media_entity_init(&msm_ois_t->msm_sd.sd.entity, 0, NULL, 0);
	msm_ois_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_ois_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
	msm_ois_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_ois_t->msm_sd);
	msm_ois_t->ois_state = OIS_DISABLE_STATE;
	msm_cam_copy_v4l2_subdev_fops(&msm_ois_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_ois_v4l2_subdev_fops.compat_ioctl32 =
		msm_ois_subdev_fops_ioctl;
#endif
	msm_ois_t->msm_sd.sd.devnode->fops =
		&msm_ois_v4l2_subdev_fops;

	OIS_ctrl=msm_ois_t;
	OIS_ctrl->i2c_client.cci_client->sid =0x1c >> 1;
	OIS_ctrl->i2c_client.cci_client->retries = 3;
	OIS_ctrl->i2c_client.cci_client->id_map = 0;
	OIS_ctrl->i2c_client.cci_client->cci_i2c_master = OIS_ctrl->cci_master;
	OIS_ctrl->i2c_client.cci_client->i2c_freq_mode =I2C_FAST_MODE ;
	OIS_ctrl->i2c_client.addr_type=MSM_CAMERA_I2C_WORD_ADDR;

	create_ois_proc_files();

	CDBG("Exit\n");
	return rc;
}

static const struct of_device_id msm_ois_i2c_dt_match[] = {
	{.compatible = "qcom,ois"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ois_i2c_dt_match);

static struct i2c_driver msm_ois_i2c_driver = {
	.id_table = msm_ois_i2c_id,
	.probe  = msm_ois_i2c_probe,
	.remove = __exit_p(msm_ois_i2c_remove),
	.driver = {
		.name = "qcom,ois",
		.owner = THIS_MODULE,
		.of_match_table = msm_ois_i2c_dt_match,
	},
};

static const struct of_device_id msm_ois_dt_match[] = {
	{.compatible = "qcom,ois", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ois_dt_match);

static struct platform_driver msm_ois_platform_driver = {
	.probe = msm_ois_platform_probe,
	.driver = {
		.name = "qcom,ois",
		.owner = THIS_MODULE,
		.of_match_table = msm_ois_dt_match,
	},
};

static int __init msm_ois_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_register(&msm_ois_platform_driver);
	if (!rc)
		return rc;
	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&msm_ois_i2c_driver);
}

static void __exit msm_ois_exit_module(void)
{
	platform_driver_unregister(&msm_ois_platform_driver);
	i2c_del_driver(&msm_ois_i2c_driver);
	return;
}

module_init(msm_ois_init_module);
module_exit(msm_ois_exit_module);
MODULE_DESCRIPTION("MSM OIS");
MODULE_LICENSE("GPL v2");
