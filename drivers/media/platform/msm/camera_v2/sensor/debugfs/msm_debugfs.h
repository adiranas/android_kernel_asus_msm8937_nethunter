/*
 * msm_debugfs.h
 *
 *  Created on: Jan 13, 2015
 *      Author: charles
 */

#ifndef DRIVER_SENSOR_DEBUGFS_MSM_DEBUGFS_H_
#define DRIVER_SENSOR_DEBUGFS_MSM_DEBUGFS_H_
#include "../msm_sensor.h"
#include "../cci/msm_cci.h"
//#include "../ois/msm_ois.h"

#define	REAR_OTP_PROC_FILE		"driver/rear_otp"
#define	FRONT_OTP_PROC_FILE		"driver/front_otp"

#define	STATUS_REAR_PROC_FILE	"driver/camera_status"
#define	STATUS_FRONT_PROC_FILE	"driver/vga_status"

#define	RESOLUTION_REAR_PROC_FILE	"driver/GetRearCameraResolution"
#define	RESOLUTION_FRONT_PROC_FILE	"driver/GetFrontCameraResolution"

#define	MODULE_REAR_PROC_FILE	"driver/RearModule"
#define	MODULE_FRONT_PROC_FILE	"driver/FrontModule"

#define SENSOR_REGISTER_DEBUG "driver/sensor_register_debug"

struct otp_struct {
	u8 af_inf[2];
	u8 af_30cm[2];
	u8 af_5cm[2];
	u8 start_current[2];
	u8 module_id;
	u8 vendor_id;
	u8 dc[4];
	u8 sn[4];
	u8 pn[4];
	u8 code[2];
	u8 zero[6];
	u8 cks[2];
};

struct debugfs {
	u8 status;
	u8 exposure_return0;
	u16 sensor_id;
};
extern int msm_debugfs_init(struct msm_sensor_ctrl_t *s_ctrl, struct msm_camera_sensor_slave_info *slave_info);
extern void msm_debugfs_set_status(unsigned int camera_id, unsigned int status);
extern int msm_read_otp(struct msm_sensor_ctrl_t *s_ctrl, struct msm_camera_sensor_slave_info *slave_info);
//extern void msm_set_actuator_ctrl(struct msm_actuator_ctrl_t *s_ctrl);
//extern void msm_set_ois_ctrl(struct msm_ois_ctrl_t *o_ctrl);
extern void get_eeprom_OTP(struct msm_eeprom_memory_block_t *block);
extern void remove_proc_file(void);
extern void msm_debugfs_set_ctrl(struct msm_sensor_ctrl_t *s_ctrl);
#endif /* DRIVER_SENSOR_DEBUGFS_MSM_DEBUGFS_H_ */
