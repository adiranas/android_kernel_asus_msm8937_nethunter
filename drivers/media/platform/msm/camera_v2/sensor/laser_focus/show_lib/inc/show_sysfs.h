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

#ifndef __LINUX_LASER_FORCUS_SENSOR_SYSFS_H
#define __LINUX_LASER_FORCUS_SENSOR_SYSFS_H

/* Path of offset file */
/* factory image */

#define	LASER_K_REF_FILE	"/factory/LaserFocus_CalibrationRef.txt"

#define	LASER_K_DMAX_FILE	"/factory/LaserFocus_CalibrationDmax.txt"

#define LASERFOCUS_SENSOR_OFFSET_CALIBRATION_FACTORY_FILE		"/factory/LaserFocus_Calibration10.txt"
/* Path of cross talk file */
#define LASERFOCUS_SENSOR_CROSS_TALK_CALIBRATION_FACTORY_FILE		"/factory/LaserFocus_Calibration40.txt"
/* Path of Laura calibration data file */
#define LAURA_CALIBRATION_FACTORY_FILE		"/factory/laura_cal_data.txt"
/* Path of CSC Laura calibration data file */
#define LAURA_CALIBRATION_CSC_FILE		"/mnt/sdcard/laura_cal_data.txt"
/* shipping image */
#define LASERFOCUS_SENSOR_OFFSET_CALIBRATION_FILE		"/mnt/sdcard/LaserFocus_Calibration10.txt"
/* Path of cross talk file */
#define LASERFOCUS_SENSOR_CROSS_TALK_CALIBRATION_FILE		"/mnt/sdcard/LaserFocus_Calibration40.txt"
/* Path of Laura calibration data file */
#define LAURA_CALIBRATION_FILE		"/mnt/sdcard/laura_cal_data.txt"


/* Read one integer from file */
int Sysfs_read_int(char *filename, size_t size);
/* Read many words(two bytes one time) from file */
int Sysfs_read_word_seq(char *filename, int *value, int size);
int Sysfs_read_Dword_seq(char *filename, int *value, int size);
/* write one integer to file */
bool Sysfs_write_int(char *filename, int value);
/* Write many words(two bytes one time) to file */
bool Sysfs_write_word_seq(char *filename, uint16_t *value, uint32_t size);
bool Sysfs_write_Dword_seq(char *filename, uint32_t *value, uint32_t size);
/* Read offset calibration value */
int Laser_sysfs_read_offset(int* cal_val);
/* Write offset calibration value to file */
bool Laser_sysfs_write_offset(int calvalue);
/* Read cross talk calibration value */
int Laser_sysfs_read_xtalk(int* cal_val);
/* Write cross talk calibration value to file */
bool Laser_sysfs_write_xtalk(int calvalue);

#endif

