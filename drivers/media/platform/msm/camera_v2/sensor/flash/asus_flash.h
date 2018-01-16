#ifndef __ASUS_FLASH_H
#define __ASUS_FLASH_H

#include <media/msm_cam_sensor.h>

typedef struct
{
	int (*asus_led_init)(void);
	int (*asus_led_off)(void);
	int (*asus_led_release)(void);

	int (*asus_led_flash)(void);
	int (*asus_led_torch)(void);


	//ledX is used only for ATD
	int (*asus_led1_torch)(void);
	int (*asus_led1_flash)(void);
	int (*asus_led2_torch)(void);
	int (*asus_led2_flash)(void);

	int (*asus_led_torch_current_setting)(int current1, int current2);
	int (*asus_led_flash_current_setting)(int current1, int current2);
	int (*asus_led_flash_duration_setting)(int duration1, int duration2);

	//right access, avoid flash torch mode mess...since right sequence should be close torch/flash then set flash/torch current
	int (*asus_led_dual_flash)(int current1, int current2);
	int (*asus_led_dual_torch)(int current1, int current2);

}asus_flash_function_table_t;

int init_asus_led(asus_flash_function_table_t * table);
void asus_flash_set_led_state(enum msm_camera_led_config_t state);
void asus_flash_set_camera_state(int in_use);
int  asus_flash_get_flash_light_state(void);
void asus_flash_lock(void);
void asus_flash_unlock(void);

#endif
