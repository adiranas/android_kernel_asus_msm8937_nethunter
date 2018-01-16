/*
 * Copyright (C) 2015 ASUSTek Inc.
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
 *
 *	Author:	Jheng-Siou, Cai
 *	Time:	2015-05
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include "show_sysfs.h"
#include "show_log.h"

/** @brief Read one integer from file
*
*	@param filename the file to read
*	@param size the size to read
*
*/
int Sysfs_read_int(char *filename, size_t size)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];
	int cal_val = 0;
	int readlen = 0;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* open file */
	fp = filp_open(filename, O_RDONLY, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		LOG_Handler(LOG_ERR, "%s: File open (%s) fail\n", __func__, filename);
		return -ENOENT;	/*No such file or directory*/
	}

	/*For purpose that can use read/write system call*/

	/* Save addr_limit of the current process */
	old_fs = get_fs();
	/* Set addr_limit of the current process to that of kernel */
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL && fp->f_op->read != NULL) {
		pos_lsts = 0;
		readlen = fp->f_op->read(fp, buf, size, &pos_lsts);
		buf[readlen] = '\0';
	} else {
		LOG_Handler(LOG_ERR, "%s: File (%s) strlen f_op=NULL or op->read=NULL\n", __func__, filename);
		return -ENXIO;	/*No such device or address*/
	}
	/* Set addr_limit of the current process back to its own */
	set_fs(old_fs);

	/* close file */
	filp_close(fp, NULL);

	sscanf(buf, "%d", &cal_val);

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return cal_val;
}

/** @brief read many word(two bytes) from file
*
*	@param filename the file to write
*	@param value the word which will store the calibration data from read file
*	@param size the size of write data
*
*/
int Sysfs_read_word_seq(char *filename, int *value, int size)
{
	int i = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[size][5];

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* open file */
	fp = filp_open(filename, O_RDONLY, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		LOG_Handler(LOG_ERR, "%s: File open (%s) fail\n", __func__, filename);
		return -ENOENT;	/*No such file or directory*/
	}

	/*For purpose that can use read/write system call*/

	/* Save addr_limit of the current process */
	old_fs = get_fs();
	/* Set addr_limit of the current process to that of kernel */
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL && fp->f_op->read != NULL) {
		pos_lsts = 0;
		for(i = 0; i < size; i++){
			fp->f_op->read(fp, buf[i], 5, &pos_lsts);
			buf[i][4]='\0';
			sscanf(buf[i], "%x", &value[i]);
			//LOG_Handler(LOG_DBG, "%s: 0x%s\n",__func__, buf[i]);
		}
	} else {
		LOG_Handler(LOG_ERR, "%s: File (%s) strlen f_op=NULL or op->read=NULL\n", __func__, filename);
		return -ENXIO;	/*No such device or address*/
	}
	/* Set addr_limit of the current process back to its own */
	set_fs(old_fs);

	/* close file */
	filp_close(fp, NULL);

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return 0;
}

int Sysfs_read_Dword_seq(char *filename, int *value, int size)
{
	int i = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[size][9];

	////LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* open file */
	fp = filp_open(filename, O_RDONLY, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		//LOG_Handler(LOG_ERR, "%s: File open (%s) fail\n", __func__, filename);
		return -ENOENT;	/*No such file or directory*/
	}

	/*For purpose that can use read/write system call*/

	/* Save addr_limit of the current process */
	old_fs = get_fs();
	/* Set addr_limit of the current process to that of kernel */
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL && fp->f_op->read != NULL) {
		pos_lsts = 0;
		for(i = 0; i < size; i++){
			fp->f_op->read(fp, buf[i], 9, &pos_lsts);
			buf[i][8]='\0';
			sscanf(buf[i], "%x", &value[i]);
			//LOG_Handler(LOG_DBG, "%s: 0x%s\n",__func__, buf[i]);
		}
	} else {
		LOG_Handler(LOG_ERR, "%s: File (%s) strlen f_op=NULL or op->read=NULL\n", __func__, filename);
		return -ENXIO;	/*No such device or address*/
	}
	/* Set addr_limit of the current process back to its own */
	set_fs(old_fs);

	/* close file */
	filp_close(fp, NULL);

	////LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return 0;
}



/** @brief write one integer to file
*
*	@param filename the file to write
*	@param value the integer which will be written to file
*
*/
bool Sysfs_write_int(char *filename, int value/*, umode_t mode*/)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8];

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	sprintf(buf, "%d", value);

	/* Open file */
	fp = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		LOG_Handler(LOG_ERR, "%s: File open (%s) fail\n", __func__, filename);
		return false;
	}

	/*For purpose that can use read/write system call*/

	/* Save addr_limit of the current process */
	old_fs = get_fs();
	/* Set addr_limit of the current process to that of kernel */
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL && fp->f_op->write != NULL) {
		pos_lsts = 0;
		fp->f_op->write(fp, buf, strlen(buf), &fp->f_pos);
	} else {
		LOG_Handler(LOG_ERR, "%s: File (%s) strlen: f_op=NULL or op->write=NULL\n", __func__, filename);
		return false;
	}
	/* Set addr_limit of the current process back to its own */
	set_fs(old_fs);

	/* Close file */
	filp_close(fp, NULL);

	LOG_Handler(LOG_DBG, "%s: Write %s to %s\n", __func__, buf, filename);

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return true;
}

/** @brief write many words(two bytes)  to file
*
*	@param filename the file to write
*	@param value the word which will be written to file
*	@param size the size of write data
*
*/
bool Sysfs_write_word_seq(char *filename, uint16_t *value, uint32_t size)
{
	struct file *fp = NULL;
	int i = 0;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[size][5];

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	for(i=0; i<size; i++){
		sprintf(buf[i], "%04x", value[i]);
		buf[i][4] = ' ';
	}

	/* Open file */
	fp = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		LOG_Handler(LOG_ERR, "%s: File open (%s) fail\n", __func__, filename);
		return false;
	}

	/*For purpose that can use read/write system call*/

	/* Save addr_limit of the current process */
	old_fs = get_fs();
	/* Set addr_limit of the current process to that of kernel */
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL && fp->f_op->write != NULL) {
		pos_lsts = 0;
		for(i = 0; i < size; i++){
			fp->f_op->write(fp, buf[i], 5, &fp->f_pos);
			//LOG_Handler(LOG_DBG, "%s: 0x%s\n",__func__, buf[i]);
		}
		fp->f_op->write(fp, "\n", 1, &fp->f_pos);
		LOG_Handler(LOG_DBG, "%s: Write to %s Done\n", __func__,filename);
	} else {
		LOG_Handler(LOG_ERR, "%s: File (%s) strlen: f_op=NULL or op->write=NULL\n", __func__, filename);
		return false;
	}
	/* Set addr_limit of the current process back to its own */
	set_fs(old_fs);

	/* Close file */
	filp_close(fp, NULL);

	LOG_Handler(LOG_CDBG, "%s: Write %s to %s\n", __func__, buf, filename);

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return true;
}


bool Sysfs_write_Dword_seq(char *filename, uint32_t *value, uint32_t size)
{
	struct file *fp = NULL;
	int i = 0;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[size][9];

	//LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	for(i=0; i<size; i++){
		sprintf(buf[i], "%08x", value[i]);
		buf[i][8] = ' ';
	}

	/* Open file */
	fp = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (IS_ERR_OR_NULL(fp)) {
		LOG_Handler(LOG_ERR, "%s: File open (%s) fail\n", __func__, filename);
		return false;
	}

	/*For purpose that can use read/write system call*/

	/* Save addr_limit of the current process */
	old_fs = get_fs();
	/* Set addr_limit of the current process to that of kernel */
	set_fs(KERNEL_DS);

	if (fp->f_op != NULL && fp->f_op->write != NULL) {
		pos_lsts = 0;
		for(i = 0; i < size; i++){
			fp->f_op->write(fp, buf[i], 9, &fp->f_pos);
			//LOG_Handler(LOG_DBG, "%s: 0x%s\n",__func__, buf[i]);
		}
	} else {
		LOG_Handler(LOG_ERR, "%s: File (%s) strlen: f_op=NULL or op->write=NULL\n", __func__, filename);
		return false;
	}
	/* Set addr_limit of the current process back to its own */
	set_fs(old_fs);

	/* Close file */
	filp_close(fp, NULL);

	LOG_Handler(LOG_DBG, "%s: Write %s to %s\n", __func__, buf, filename);

	//LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return true;
}

/** @brief Read offset calibration value
*
*/
int Laser_sysfs_read_offset(int* cal_val){

	/* Read offset value from file */
	*cal_val = Sysfs_read_int(LASERFOCUS_SENSOR_OFFSET_CALIBRATION_FACTORY_FILE, 6);

	if(*cal_val ==(-5000) ) {
		LOG_Handler(LOG_ERR, "%s: Offset Calibration read fail. (%d)\n", __func__, *cal_val);
		return -EINVAL;
	}
	else
		LOG_Handler(LOG_DBG, "%s Read Offset Calibration value: %d\n", __func__, *cal_val);

	return 0;
}

/** @brief Write offset calibration value to file
*
*	@param calvalue the offset value
*
*/
bool Laser_sysfs_write_offset(int calvalue)
{
	bool rc = false;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* Write offset value to file */
#ifdef ASUS_FACTORY_BUILD
	rc = Sysfs_write_int(LASERFOCUS_SENSOR_OFFSET_CALIBRATION_FACTORY_FILE, calvalue);
#else
	rc = Sysfs_write_int(LASERFOCUS_SENSOR_OFFSET_CALIBRATION_FACTORY_FILE, calvalue);
#endif
	if(!rc){
		LOG_Handler(LOG_ERR, "%s: Write Offset Calibration file fail\n", __func__);
		return false;
	}

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return rc;
}

/** @brief Read cross talk calibration value
*
*/
int Laser_sysfs_read_xtalk(int* cal_val)
{
	*cal_val = Sysfs_read_int(LASERFOCUS_SENSOR_CROSS_TALK_CALIBRATION_FACTORY_FILE, 6);

	if(*cal_val == (-5000)) {
		LOG_Handler(LOG_ERR, "%s: Xtalk Offset read fail. (%d)\n", __func__, *cal_val);
		return -EINVAL;
	}
	else
		LOG_Handler(LOG_DBG, "%s: Xtalk Offset  value: %d\n", __func__, *cal_val);

	return 0;
}

/** @brief Write cross talk calibration value to file
*
*	@param calvalue the cross talk value
*
*/
bool Laser_sysfs_write_xtalk(int calvalue)
{
	bool rc = false;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* Write cross talk value to file */
#ifdef ASUS_FACTORY_BUILD
	rc = Sysfs_write_int(LASERFOCUS_SENSOR_CROSS_TALK_CALIBRATION_FACTORY_FILE, calvalue);
#else
	rc = Sysfs_write_int(LASERFOCUS_SENSOR_CROSS_TALK_CALIBRATION_FACTORY_FILE, calvalue);
#endif
	if(!rc){
		LOG_Handler(LOG_ERR, "%s: Write Cross-talk Offset Calibration file fail\n", __func__);
		return false;
	}

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return rc;
}
