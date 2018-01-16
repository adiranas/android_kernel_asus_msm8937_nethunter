/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *	Author:	Jheng-Siou, Cai
 *	Time:	2015-05
 */

#ifndef MSM_LASER_FOCUS_H
#define MSM_LASER_FOCUS_H

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_sd.h"
#include "msm_cci.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

struct msm_laser_focus_ctrl_t;

#include "laser_focus.h"



// If "export ASUS_FACTORY_BUILD=y" command is run before build image
#ifdef ASUS_FACTORY_BUILD
#endif

/* Laser focus state */
enum msm_laser_focus_state_t {
	LASER_FOCUS_POWER_UP,	/* Power up */
	LASER_FOCUS_POWER_DOWN,	/* Power down */
};

/* Laser focuse data type */
enum msm_laser_focus_data_type {
	MSM_LASER_FOCUS_BYTE_DATA = 1,	/* Byte */
	MSM_LASER_FOCUS_WORD_DATA,		/* Word */
};

/* Laser focus address type */
enum msm_laser_focus_addr_type {
	MSM_LASER_FOCUS_BYTE_ADDR = 1,	/* Byte */
	MSM_LASER_FOCUS_WORD_ADDR,		/* Word */
};

/* Laser focus status controller */
enum msm_laser_focus_atd_device_trun_on_type {
	MSM_LASER_FOCUS_DEVICE_OFF = 0,	/* Device power off */
	MSM_LASER_FOCUS_DEVICE_APPLY_CALIBRATION,	/* Device power on and apply calibration */
	MSM_LASER_FOCUS_DEVICE_NO_APPLY_CALIBRATION,	/* Device power but no apply calibration */
	MSM_LASER_FOCUS_DEVICE_INIT_CCI,	/* Device initialize CCI only */
	MSM_LASER_FOCUS_DEVICE_DEINIT_CCI,	/* Device deinitialize CCI only */
};

enum laser_product_family{
	PRODUCT_UNKNOWN,
	PRODUCT_LAURA,
	PRODUCT_OLIVIA,
};

/* Laser focus voltage information */
struct msm_laser_focus_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[CAM_VREG_MAX];
	int num_vreg;
};

/* Laser focus controller */
struct msm_laser_focus_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client *i2c_client;
	enum msm_camera_device_type_t act_device_type;
	struct msm_sd_subdev msm_sd;
	struct msm_camera_sensor_board_info *sensordata;
	
	struct mutex *laser_focus_mutex;
	enum msm_laser_focus_data_type i2c_data_type;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *act_v4l2_subdev_ops;

	int16_t device_state;
	
	/* For calibration */
	int32_t laser_focus_offset_value;
	uint32_t laser_focus_cross_talk_offset_value;

	
	uint16_t reg_tbl_size;
	
	void *user_data;
	
	uint16_t initial_code;
	struct msm_camera_i2c_reg_array *i2c_reg_tbl;
	uint16_t i2c_tbl_index;
	enum cci_i2c_master_t cci_master;
	uint32_t subdev_id;
	enum msm_laser_focus_state_t laser_focus_state;
	struct msm_laser_focus_vreg vreg_cfg;
	
	uint32_t max_code_size;
	
	//enum af_camera_name cam_name;
	//struct msm_laser_focus_func_tbl *func_tbl;
	//int16_t curr_step_pos;
	//uint16_t curr_region_index;
	//uint16_t *step_position_table;
	//struct region_params_t region_params[MAX_LASER_FOCUS_REGION];
	//struct msm_laser_focus_reg_params_t reg_tbl[MAX_LASER_FOCUS_REG_TBL_SIZE];
	//uint16_t region_size;
	//uint32_t total_steps;
	//uint16_t pwd_step;
	//struct park_lens_data_t park_lens;
};

#define        DEFAULT_AMBIENT                         1600
#define        DEFAULT_CONFIDENCE10                    1300
#define        DEFAULT_CONFIDENCE_THD          		   5600
#define        DEFAULT_IT                              5000
#define        DEFAULT_CONFIDENCE_FACTOR       			 16
#define        DEFAULT_DISTANCE_THD                    1500
#define        DEFAULT_NEAR_LIMIT                      100

#define        DEFAULT_TOF_NORMAL_0                    0xE100
#define        DEFAULT_TOF_NORMAL_1                    0x10FF
#define        DEFAULT_TOF_NORMAL_2                    0x07F0
#define        DEFAULT_TOF_NORMAL_3                    0x5010
#define        DEFAULT_TOF_NORMAL_4                    0xA081
#define        DEFAULT_TOF_NORMAL_5                    0x4594

#define        DEFAULT_TOF_K10_0                               0xE100
#define        DEFAULT_TOF_K10_1                               0x10FF
#define        DEFAULT_TOF_K10_2                               0x07D0
#define        DEFAULT_TOF_K10_3                               0x5008
#define        DEFAULT_TOF_K10_4                               0x1C81
#define        DEFAULT_TOF_K10_5                               0x4114

#define        DEFAULT_TOF_K40_0                               0xE100
#define        DEFAULT_TOF_K40_1                               0x10FF
#define        DEFAULT_TOF_K40_2                               0x07D0
#define        DEFAULT_TOF_K40_3                               0x5008
#define        DEFAULT_TOF_K40_4                               0xFC81
#define        DEFAULT_TOF_K40_5                               0x4114

enum HPTG_config_enum {
       AMBIENT = 0,
       CONFIDENCE10,
       CONFIDENCE_THD,
       IT,
       CONFIDENCE_FACTOR,
       DISTANCE_THD,
       NEAR_LIMIT,
       TOF_NORMAL_0,
       TOF_NORMAL_1,
       TOF_NORMAL_2,
       TOF_NORMAL_3,
       TOF_NORMAL_4,
       TOF_NORMAL_5,
       TOF_K10_0,
       TOF_K10_1,
       TOF_K10_2,
       TOF_K10_3,
       TOF_K10_4,
       TOF_K10_5,
       TOF_K40_0,
       TOF_K40_1,
       TOF_K40_2,
       TOF_K40_3,
       TOF_K40_4,
       TOF_K40_5,
       NUMBER_OF_SETTINGS,
};

struct HPTG_settings{
       uint16_t ambient;
       uint16_t confidenceIT[3];
       uint16_t confidenceDist[3];
};

#if 0
struct msm_laser_focus_func_tbl {
	int32_t (*laser_focus_i2c_write_b_af)(struct msm_laser_focus_ctrl_t *,
			uint8_t,
			uint8_t);
	int32_t (*laser_focus_init_step_table)(struct msm_laser_focus_ctrl_t *,
		struct msm_laser_focus_set_info_t *);
	int32_t (*laser_focus_init_focus)(struct msm_laser_focus_ctrl_t *,
		uint16_t, struct reg_settings_t *);
	int32_t (*laser_focus_set_default_focus) (struct msm_laser_focus_ctrl_t *,
			struct msm_laser_focus_move_params_t *);
	int32_t (*laser_focus_move_focus) (struct msm_laser_focus_ctrl_t *,
			struct msm_laser_focus_move_params_t *);
	void (*laser_focus_parse_i2c_params)(struct msm_laser_focus_ctrl_t *,
			int16_t, uint32_t, uint16_t);
	void (*laser_focus_write_focus)(struct msm_laser_focus_ctrl_t *,
			uint16_t,
			struct damping_params_t *,
			int8_t,
			int16_t);
	int32_t (*laser_focus_set_position)(struct msm_laser_focus_ctrl_t *,
		struct msm_laser_focus_set_position_t *);
	int32_t (*laser_focus_park_lens)(struct msm_laser_focus_ctrl_t *);
};

struct msm_laser_focus {
	enum laser_focus_type act_type;
	struct msm_laser_focus_func_tbl func_tbl;
};
#endif

/* Voltage control */
#define ENABLE_VREG 1
#define DISABLE_VREG 0

/* Calibration parameter */
#define MSM_LASER_FOCUS_APPLY_NORMAL_CALIBRATION		0
#define MSM_LASER_FOCUS_APPLY_OFFSET_CALIBRATION		10	/* Calibration 10cm*/
#define MSM_LASER_FOCUS_APPLY_CROSSTALK_CALIBRATION	40	/* Calibration 40cm*/
#define MSM_LASER_FOCUS_APPLY_INFINITY_CALIBRATION		70	/* Calibration infinity */

#define VL6180_CROSSTALK_CAL_RANGE		400	/* 400mm */
#define VL6180_OFFSET_CAL_RANGE			100	/* 100mm */
#define STMVL6180_RUNTIMES_OFFSET_CAL		20	/* Times of calibration collect range data */

#define REF_K_DATA_NUMBER	4
#define	DMAX_K_DATA_NUMBER 3


/* Laser focus control file path */
#define	STATUS_PROC_FILE		"driver/LaserFocus_Status"	/* Status */
#define	STATUS_PROC_FILE_FOR_CAMERA	"driver/LaserFocus_Status_For_Camera"	/* Status (check on prob only) */
#define	DEVICE_TURN_ON_FILE		"driver/LaserFocus_on"	/* Power on/off */
#define	DEVICE_GET_VALUE		"driver/LaserFocus_value"	/* Get range value */
#define	DEVICE_GET_VALUE_MORE_INFO	"driver/LaserFocus_value_more_info"	/* Get range value, DMax and error code*/
#define	DEVICE_SET_CALIBRATION		"driver/LaserFocus_CalStart" /* Calibration */
#define	DEVICE_DUMP_REGISTER_VALUE	"driver/LaserFocus_register_dump"	/* Dump register value right */
#define	DEVICE_DUMP_DEBUG_VALUE	"driver/LaserFocus_debug_dump"	/* Dump register value for vl6180x debug */
#define	DEVICE_ENFORCE_FILE	"driver/LaserFocus_enforce"	/* Disable laser focus value */
#define	DEVICE_LOG_CTRL_FILE	"driver/LaserFocus_log_ctrl"	/* Log contorl */
#define DEVICE_CALIBRATION_UPDATED	"driver/LaserFocus_updated" /*need load calibration data from file*/
#define DEVICE_MEASURE_MODE	"driver/LaserFocus_measure_mode" /*change measure mode*/
#define DEVICE_CSC_MODE "driver/LaserFocus_CSCmode" /* for CSC */
#define DEVICE_DEBUG_VALUE1	"driver/LaserFocus_log_value1" /* Debug value for CE collect data */
#define DEVICE_DEBUG_VALUE2	"driver/LaserFocus_log_value2" /* Debug value for CE collect data */
#define DEVICE_DEBUG_VALUE3	"driver/LaserFocus_log_value3" /* Debug value for CE collect data */
#define DEVICE_DEBUG_VALUE4	"driver/LaserFocus_log_value4" /* Debug value for CE collect data */
#define DEVICE_DEBUG_VALUE5	"driver/LaserFocus_log_value5" /* Debug value for CE collect data */
#define DEVICE_DEBUG_VALUE6	"driver/LaserFocus_log_value6"
#define DEVICE_DEBUG_VALUE7	"driver/LaserFocus_log_value7"
#define DEVICE_DEBUG_VALUE8	"driver/LaserFocus_log_value8"
#define DEVICE_DEBUG_VALUE9	"driver/LaserFocus_log_value9"
#define DEVICE_DEBUG_VALUE10	"driver/LaserFocus_log_value10"
#define DEVICE_VALUE_CHECK	"driver/LaserFocus_value_check" /* Check sensor value */
#define DEVICE_IOCTL_SET_K	"driver/LaserFocus_setK"
#define DEVICE_IOCTL_PRODUCT_FAMILY	"driver/LaserFocus_ProductFamily"

/* Right of laser focus control file*/
#ifdef ASUS_FACTORY_BUILD
#define	STATUS_PROC_FILE_MODE 0777	/* Status right */
#define	STATUS_PROC_FILE_FOR_CAMERA_MODE 0777	/* Status (check on prob only) right */
#define	DEVICE_TURN_ON_FILE_MODE 0777	/* Power on/off right */
#define	DEVICE_GET_VALUE_MODE 0777	/* Get value right */
#define	DEVICE_GET_VALUE_MODE_MORE_INFO 0777	/* Get value more info right */
#define	DEVICE_SET_CALIBRATION_MODE 0777	/* Calibration right */
#define	DEVICE_DUMP_REGISTER_VALUE_MODE 0777	/* Dump register value right */
#define	DEVICE_DUMP_DEBUG_VALUE_MODE 0777	/* Dump register value right for vl6180x debug */
#define	DEVICE_ENFORCE_MODE	0777	/* Laser focus disable right */
#define	DEVICE_LOG_CTRL_MODE	0777	/* Log contorl right */
#else
#define	STATUS_PROC_FILE_MODE 0660	/* Status right */
#define	STATUS_PROC_FILE_FOR_CAMERA_MODE 0660	/* Status (check on prob only) right */
#define	DEVICE_TURN_ON_FILE_MODE 0660	/* Power on/off right */
#define	DEVICE_GET_VALUE_MODE 0664	/* Get value right */
#define	DEVICE_GET_VALUE_MODE_MORE_INFO 0664	/* Get value more info right */
#define	DEVICE_SET_CALIBRATION_MODE 0660	/* Calibration right */
#define	DEVICE_DUMP_REGISTER_VALUE_MODE 0660	/* Dump register value right */
#define	DEVICE_DUMP_DEBUG_VALUE_MODE 0660	/* Dump register value right for vl6180x debug */
#define	DEVICE_ENFORCE_MODE	0660	/* Laser focus disable right */
#define	DEVICE_LOG_CTRL_MODE	0660	/* Log contorl right */
#endif

/* Delay time */
#define DEFAULT_DELAY_TIME 1000 /* us */
#define MCPU_DELAY_TIME 700	/* us */
#define READ_DELAY_TIME 500	/* us */
#define READY_DELAY_TIME 100 /* us */
/* Check device verify number */
int Laser_Match_ID(struct msm_laser_focus_ctrl_t *dev_t, int chip_id_size);
/* Voltage control */
int32_t vreg_control(struct msm_laser_focus_ctrl_t *dev_t, int config);
/* Power on component */
int32_t power_up(struct msm_laser_focus_ctrl_t *dev_t);
/* Power off component */
int32_t power_down(struct msm_laser_focus_ctrl_t *dev_t);
/* Initialize device */
int dev_init(struct msm_laser_focus_ctrl_t *dev_t);
/* Deinitialize device */
int dev_deinit(struct msm_laser_focus_ctrl_t *dev_t);

void Laser_Match_Module(struct msm_laser_focus_ctrl_t *dev_t);
/* Check device status */
int dev_I2C_status_check(struct msm_laser_focus_ctrl_t *dev_t, int chip_id_size);
/* Mutex controller handles mutex action */
int mutex_ctrl(struct msm_laser_focus_ctrl_t *dev_t, int ctrl);
/* Laser focus driver prob function */
int32_t Laser_Focus_platform_probe(struct platform_device *pdev, struct msm_laser_focus_ctrl_t *dev_t,
										struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl,
										const struct v4l2_subdev_internal_ops msm_laser_focus_internal_ops,
										struct v4l2_subdev_ops msm_laser_focus_subdev_ops,
										struct msm_camera_i2c_client msm_laser_focus_i2c_client,
										const struct of_device_id msm_laser_focus_dt_match[], int *ATD_status);

#endif
