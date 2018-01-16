/*
*
*	Author:	Jheng-Siou, Cai
*	Time:	2015-07
*
*/

#include "laura_shipping_func.h"
#include "show_log.h"

/* Log count */
static int read_range_log_count = 0; // Read range log count

/* Module id */
static bool module_id_flag = false;
uint16_t module_id[MODULE_ID_LEN];
uint16_t fw_version[FW_VERSION_LEN];
uint16_t f0_data[CAL_MSG_F0*2];
bool needLoadCalibrationData = true;
bool CSCTryGoldenRunning = false;
bool CSCTryGoldenSucceed = false;
extern int ErrCode;


static uint16_t debug_raw_range = 0;
static uint16_t debug_raw_confidence = 0;


extern bool continuous_measure;
extern int  Range_Cached;
extern bool Disable_Device;
extern bool measure_cached_range_updated;
extern uint16_t Settings[NUMBER_OF_SETTINGS];

extern int proc_read_value_cnt;
extern int ioctrl_read_value_cnt;
static void init_debug_raw_data(void){
		debug_raw_range = 0;
		debug_raw_confidence = 0;
}

uint16_t get_debug_raw_range(void){
	return debug_raw_range;
}

uint16_t get_debug_raw_confidence(void){
	return debug_raw_confidence;
}


/** @brief Swap high and low of the data (e.g 0x1234 => 0x3412)
*
*	@param register_data the data which will be swap
*
*/
void swap_data(uint16_t* register_data){
	*register_data = ((*register_data >> 8) | ((*register_data & 0xff) << 8)) ;
}
uint16_t swapped_data(uint16_t register_data){
	return ((register_data >> 8) | ((register_data & 0xff) << 8)) ;
}
static const char* getMCPUState(uint16_t status)
{
	if(status == STATUS_MEASURE_ON)
		return "MEASURE_ON";

	switch(status & STATUS_MASK)
	{
		case STATUS_STANDBY:
			return "STANDBY";
		case STATUS_MCPU_OFF:
			return "OFF";
		case STATUS_MCPU_ON:
			return "ON";

		default:
			return "OTHER STATE";
	}
}
static int format_hex_string(char *output_buf,int len, uint16_t* data,int count)
{
	int i;
	int offset;

	if(len<count*5+1+1)
		return -1;

	offset=0;

	for(i=0;i<count;i++)
	{
		offset+=sprintf(output_buf+offset,"%04x ",data[i]);
	}
	offset+=sprintf(output_buf+offset,"\n");

	return offset;
}

/** @brief Mailbox: create calibration data
*		  This mailbox command is used to retrieve data to be used for the computation of calibration parameters.
*		  This is a singleentry MBX command with MBX message response with Msg_len = 6
*
*	@param dev_t the laser focus controller
*	@param cal_data the calibration ouput data
*
*/
int Mailbox_Command(struct msm_laser_focus_ctrl_t *dev_t, int16_t cal_data[]){
	int status = 0, msg_len = 0, M2H_Msg_Len = 0;
	uint16_t i2c_read_data, i2c_read_data2;
	struct timeval start;//, now;

	start = get_current_time();
	while(1){
		status = CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
		if (status < 0){
			return status;
		}

		if((i2c_read_data&NEW_DATA_IN_MBX) == 0x00){
			break;
		}

		/* Busy pending MCPU Msg */
		status = CCI_I2C_RdWord(dev_t, M2H_MBX, &i2c_read_data2);
		if (status < 0){
			return status;
		}
		LOG_Handler(LOG_ERR, "%s: register(0x00, 0x10): (0x%x, 0x%x)\n", __func__,  i2c_read_data, i2c_read_data2);
#if 0
		/* Check if time out */
		now = get_current_time();
              if(is_timeout(start,now,TIMEOUT_VAL)){
			LOG_Handler(LOG_ERR, "%s: Verify ICSR(2:1) time out - register(0x10): 0x%x\n", __func__, i2c_read_data);
					return -TIMEOUT_VAL;
              }
#endif
		//usleep(DEFAULT_DELAY_TIME);
	}

	status = CCI_I2C_WrWord(dev_t, H2M_MBX, 0x0004);
       if (status < 0){
               return status;
       }

	while(1){
		status = CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
		if (status < 0){
			return status;
		}

		if((i2c_read_data&0x20) == 0x20){
			break;
		}

#if 0
		/* Check if time out */
		now = get_current_time();
              if(is_timeout(start,now,TIMEOUT_VAL)){
			LOG_Handler(LOG_ERR, "%s: Verify ICSR(2:1) time out - register(0x10): 0x%x\n", __func__, i2c_read_data);
					return -TIMEOUT_VAL;
              }
#endif
		//usleep(DEFAULT_DELAY_TIME);
	}


	status = CCI_I2C_RdWord(dev_t, M2H_MBX, &i2c_read_data);
	LOG_Handler(LOG_DBG, "%s: Verify M2H_MBX(1)  register(0x12): 0x%x\n", __func__, i2c_read_data);
	if (status < 0){
		return status;
       }
	M2H_Msg_Len = (i2c_read_data & CMD_LEN_MASK)>>8;
	LOG_Handler(LOG_DBG, "%s: Verify M2H_MBX(1) M2H_Msg_Len: %d\n", __func__, M2H_Msg_Len);

	if(((i2c_read_data&0xFF) == 0xCC) && (M2H_Msg_Len == CAL_MSG_LEN)){
		for(msg_len=0; msg_len<M2H_Msg_Len; msg_len++){
			start = get_current_time();
			while(1){
				status = CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
				if (status < 0){
					return status;
				}
				//LOG_Handler(LOG_ERR, "%s: Verify ICSR(1)  register(0x00): 0x%x\n", __func__, i2c_read_data);

				if((i2c_read_data&NEW_DATA_IN_MBX)){
					break;
				}
#if 0
				/* Check if time out */
				now = get_current_time();
					if(is_timeout(start,now,TIMEOUT_VAL)){
					LOG_Handler(LOG_ERR, "%s: Verify ICSR(1) time out - register(0x00): 0x%x\n", __func__, i2c_read_data);
							return -TIMEOUT_VAL;
					}
#endif
				//usleep(DEFAULT_DELAY_TIME);
			}

			status = CCI_I2C_RdWord(dev_t, M2H_MBX, &i2c_read_data);
			if (status < 0){
					return status;
			}

			/* Append to previosly saved data */
			cal_data[msg_len] = i2c_read_data;
			LOG_Handler(LOG_DBG, "%s: Calibration data[%d]: 0x%x\n", __func__, msg_len, cal_data[msg_len]);
		}
	}
	else{
		LOG_Handler(LOG_ERR, "%s: M2H_MBX(7:0): 0x%x, Msg_Len: %d\n", __func__, i2c_read_data&0xFF, M2H_Msg_Len);
		return -1;
	}
	return status;
}


int Check_Ready_via_ICSR(struct msm_laser_focus_ctrl_t *dev_t){
	int status =0;
	uint16_t i2c_read_data, i2c_read_data2;
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);
	while(1){
		status = CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
		if (status < 0)
			break;

		if((i2c_read_data&(NEW_DATA_IN_MBX)) == GO_AHEAD)
		{
			status = 0;
			LOG_Handler(LOG_FUN, "Check_Ready_via_ICSR -> GO_AHEAD\n");
			break;
		}

		/* Busy pending MCPU Msg */
		status = CCI_I2C_RdWord(dev_t, M2H_MBX, &i2c_read_data2);
		if (status < 0)
			break;
		//usleep_range(100,100);
		usleep_range(READY_DELAY_TIME,READY_DELAY_TIME);
		//usleep(100);
		LOG_Handler(LOG_ERR, "%s: register(0x00, 0x10): (0x%x, 0x%x)\n", __func__,  i2c_read_data, i2c_read_data2);

	}
	LOG_Handler(LOG_FUN, "%s: Exit, status=%d\n", __func__,status);
	return status;

}

int Wait_for_Notification_from_ICSR(struct msm_laser_focus_ctrl_t *dev_t){
	int status =0;
	uint16_t i2c_read_data;
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);
	while(1){//ToDo delay
		status = CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
		if (status < 0 || (i2c_read_data&NEW_DATA_IN_MBX))
			break;
		//usleep_range(READY_DELAY_TIME,READY_DELAY_TIME);
	}
	LOG_Handler(LOG_FUN, "%s: Exit, with status %d\n", __func__,status);
	return status;
}

int Olivia_Mailbox_Command(struct msm_laser_focus_ctrl_t *dev_t, uint16_t cal_data[]){
	int status = 0, msg_index = 0, M2H_Msg_Len = 0;
	uint16_t i2c_read_data;
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);
	status = Check_Ready_via_ICSR(dev_t);
	if(status<0)
	{
		pr_err("LASER, Check_Ready_via_ICSR return error %d\n",status);
		return status;
	}
	//if(status !=0)

	//single entry message? -> Cmd_len=0
	status = CCI_I2C_WrWord(dev_t, H2M_MBX, GET_CALIBRATION);
       if (status < 0){
               return status;
       }

	status = Wait_for_Notification_from_ICSR(dev_t);
	//if(status !=0)


	status = CCI_I2C_RdWord(dev_t, M2H_MBX, &i2c_read_data);
	if (status < 0){
		return status;
       }

	M2H_Msg_Len = (i2c_read_data & CMD_LEN_MASK)>>8;
	if(M2H_Msg_Len != CAL_MSG_LEN)
		LOG_Handler(LOG_ERR,"Message length is not in expect\n");

	if((i2c_read_data&MESSAGE_ID_MASK) == 0xCC) {
		for(msg_index=0; msg_index<CAL_MSG_LEN; msg_index++){

			status = Wait_for_Notification_from_ICSR(dev_t);
			//if(status !=0)
			status = CCI_I2C_RdWord(dev_t, M2H_MBX, &i2c_read_data);
			if (status < 0){
					LOG_Handler(LOG_ERR,"CCI_I2C_RdWord M2H_MBX Failed!\n");
					return status;
			}
			/* Append to previosly saved data */
			cal_data[msg_index] = i2c_read_data;
			//LOG_Handler(LOG_DBG, "%s: Calibration data[%d]: 0x%x\n", __func__, msg_index, cal_data[msg_index]);
		}
	}
	else{
		LOG_Handler(LOG_ERR, "%s: M2H_MBX(7:0): 0x%x, Msg_Len: %d\n", __func__, i2c_read_data&0xFF, M2H_Msg_Len);
		return -1;
	}
	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);
	return status;
}

int ambient_setting(struct msm_laser_focus_ctrl_t *dev_t)
{

       int status=0;
       uint16_t i2c_read_data;
       int cnt=0;
/*
       int buf[1]={0x0640}; //dec 1600

#ifdef ASUS_FACTORY_BUILD
	   Sysfs_read_word_seq("/factory/Olivia_ambient.txt",buf,1);
	   LOG_Handler(LOG_DBG, "%s: ambient setting(%04x)\n",__func__,buf[0]);
#endif
*/
	   LOG_Handler(LOG_CDBG, "%s: ambient setting(%d)\n",__func__,Settings[AMBIENT]);
       status = CCI_I2C_WrWord(dev_t, H2M_MBX, 0x81C7);

       while(1){
               CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
               if(i2c_read_data&MCPU_HAVE_READ_MBX){
                       cnt=0;
                       break;
               }
               if(++cnt > 100){
                       LOG_Handler(LOG_ERR, "%s: timeout 1\n",__func__);
                       return -1;
               }
               msleep(1);
       }

       status = CCI_I2C_WrWord(dev_t, H2M_MBX, Settings[AMBIENT]);

       while(1){
               CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
               if(i2c_read_data&MCPU_HAVE_READ_MBX){
                       cnt=0;
                       break;
               }
               if(++cnt > 50){
                       LOG_Handler(LOG_ERR, "%s: timeout 2\n",__func__);
                       return -1;
               }
               msleep(1);
       }

       return status;
}


/** @brief Load calibration data
*
*	@param dev_t the laser focus controller
*
*/
int Laura_device_Load_Calibration_Value(struct msm_laser_focus_ctrl_t *dev_t){
	int status = 0;
	uint16_t indirect_addr, data[SIZE_OF_LAURA_CALIBRATION_DATA];
#if DEBUG_LOG_FLAG
	int i = 0;
	uint16_t data_verify;
#endif
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);


	/* Read Calibration data, addr is swapped */
	indirect_addr = 0x10C0;

	status = Laura_Read_Calibration_Data_From_File(data, SIZE_OF_LAURA_CALIBRATION_DATA);
	if(status < 0){
		LOG_Handler(LOG_ERR, "%s: Load calibration fail!!\n", __func__);
		return status;
	}

	Laura_device_indirect_addr_write(dev_t, 0x18, 0x19, indirect_addr, I2C_DATA_PORT, data, 10);

	/* Check patch memory write */
	CCI_I2C_WrByte(dev_t, 0x18, 0x10);
	CCI_I2C_WrByte(dev_t, 0x19, 0xC0);
#if DEBUG_LOG_FLAG
	for(i = 0; i < 20; i++){
		CCI_I2C_RdByte(dev_t, I2C_DATA_PORT, &data_verify);
		LOG_Handler(LOG_DBG, "%s: 0x1A: 0x%x\n", __func__, data_verify);
	}
#endif
	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return status;
}
//data_len is count
static void getRegisterData(struct msm_laser_focus_ctrl_t *dev_t,uint16_t address, uint16_t *data, uint32_t data_len)
{
	int status;
	uint16_t low_address;
	uint16_t high_address;
	uint32_t i;

	low_address = address&0x00FF;
	high_address = (address&0xFF00)>>8;

	status = CCI_I2C_WrByte(dev_t, 0x18, low_address);
	status = CCI_I2C_WrByte(dev_t, 0x19, high_address);

	for(i = 0; i < data_len; i++)
	{
		//one word
		CCI_I2C_RdByte(dev_t, I2C_DATA_PORT, &data[i]);
    }
}

static int control_signal_setting(struct msm_laser_focus_ctrl_t *dev_t)
{
	int status = 0;
	int i;

	static uint16_t commands[5][2] =
	{
		{0x0710,0xD61A},
		{0x0712,0xD586},
		{0x0730,0xD564},
		{0x0732,0xE500},
		{0x0700,0x0003}
	};
	uint16_t write_settings[5];

	uint16_t verify_bytes[2];
	uint16_t verify_word;

	for(i=0;i<5;i++)
	{
		write_settings[i]=swapped_data(commands[i][1]);
		Laura_device_indirect_addr_write(dev_t, 0x18, 0x19,swapped_data(commands[i][0]),I2C_DATA_PORT,&write_settings[i],1);
	}

	for(i=0;i<5;i++)
	{
		getRegisterData(dev_t,commands[i][0],verify_bytes,2);
		verify_word = verify_bytes[0]|verify_bytes[1]<<8;
		LOG_Handler(LOG_CDBG,"verfiy signal setting, register 0x%x, setting 0x%x, read back 0x%x\n",
			commands[i][0],commands[i][1],verify_word
		);
		if(commands[i][1]!=verify_word)
		{
			status = -1;
			LOG_Handler(LOG_ERR,"verfiy signal setting, register 0x%x, setting 0x%x, read back 0x%x, ERROR!\n",
				commands[i][0],commands[i][1],verify_word
			);
		}
	}
	return status;
}
bool compareConfidence(uint16_t confidence, uint16_t Range, uint16_t thd, uint16_t limit)
{
	return  		confidence < thd*limit/Range;
}

uint16_t thd = 16;
uint16_t limit = 1500;
uint8_t thd_near_mm = 100;

int Olivia_device_Load_Calibration_Value(struct msm_laser_focus_ctrl_t *dev_t){
	int status = 0;
	int i = 0;
	uint16_t indirect_addr;
	static uint16_t data[SIZE_OF_OLIVIA_CALIBRATION_DATA+CONFIDENCE_LENGTH];

    uint16_t header_verify0,header_verify1;
    uint16_t header_verify[3];

#if DEBUG_LOG_FLAG
	uint16_t data_verify;
#endif
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* Read Calibration data, addr is swapped */
	indirect_addr = 0x10C0;
	if(needLoadCalibrationData || CSCTryGoldenRunning)
	{
		status = Laura_Read_Calibration_Data_From_File(data, SIZE_OF_OLIVIA_CALIBRATION_DATA+CONFIDENCE_LENGTH);
		if(status < 0){
			LOG_Handler(LOG_ERR, "%s: Load calibration fail!!\n", __func__);
			return status;
		}
		else
		{
			LOG_Handler(LOG_CDBG, "%s: Load calibration file succeed!\n", __func__);
		}
		//swap confidence data back
		swap_data(&data[SIZE_OF_OLIVIA_CALIBRATION_DATA]);
		swap_data(&data[SIZE_OF_OLIVIA_CALIBRATION_DATA+1]);
	   if( 500<=data[SIZE_OF_OLIVIA_CALIBRATION_DATA]&&1843>=data[SIZE_OF_OLIVIA_CALIBRATION_DATA]&&
		   2167<=data[SIZE_OF_OLIVIA_CALIBRATION_DATA+1]&& 7987>=data[SIZE_OF_OLIVIA_CALIBRATION_DATA+1]
		 )
	   {
				  Settings[CONFIDENCE10] =  data[SIZE_OF_OLIVIA_CALIBRATION_DATA];
				  Settings[CONFIDENCE_THD] = data[SIZE_OF_OLIVIA_CALIBRATION_DATA+1];
	   }
	   else
	   {
			   LOG_Handler(LOG_ERR, "%s: ConfA %d, ConfC %d in Calibration Text invalid!\n", __func__,data[SIZE_OF_OLIVIA_CALIBRATION_DATA],data[SIZE_OF_OLIVIA_CALIBRATION_DATA+1]);
	   }
		//Settings[CONFIDENCE10] =  data[SIZE_OF_OLIVIA_CALIBRATION_DATA];
		//Settings[CONFIDENCE_THD] = data[SIZE_OF_OLIVIA_CALIBRATION_DATA+1];


		#ifdef ASUS_FACTORY_BUILD
			needLoadCalibrationData = true;
		#else
			needLoadCalibrationData = false;
		#endif
	}

	Laura_device_indirect_addr_write(dev_t, 0x18, 0x19, indirect_addr, I2C_DATA_PORT, data, SIZE_OF_OLIVIA_CALIBRATION_DATA);

	/* Check patch memory write */
	CCI_I2C_WrByte(dev_t, 0x18, 0x10);
	CCI_I2C_WrByte(dev_t, 0x19, 0xC0);

#if DEBUG_LOG_FLAG
	for(i=0;i<SIZE_OF_OLIVIA_CALIBRATION_DATA;i++)
	{
		LOG_Handler(LOG_CDBG, "%s: writing data[%d]: 0x%x\n", __func__,i,data[i]);
	}
	for(i = 0; i < SIZE_OF_OLIVIA_CALIBRATION_DATA; i++){
		CCI_I2C_RdWord(dev_t, I2C_DATA_PORT, &data_verify);
		LOG_Handler(LOG_CDBG, "%s: verify 0x1A: 0x%x\n", __func__, data_verify);
	}
#endif

	for(i=0; i<3; i++){
	   CCI_I2C_RdByte(dev_t, I2C_DATA_PORT, &header_verify0);
	   CCI_I2C_RdByte(dev_t, I2C_DATA_PORT, &header_verify1);
	   header_verify0 &= 0x00ff;
	   header_verify1 &= 0x00ff;
	   header_verify[i] = (header_verify1<<8)|header_verify0;
	}
	LOG_Handler(LOG_CDBG, "%s: header 0x%04x 0x%04x 0x%04x, expect CA1B 0026 0301\n",
								   __func__, header_verify[0],header_verify[1],header_verify[2]);


	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return status;
}

int Laura_Read_Calibration_Value_From_File(struct seq_file *vfile, uint16_t *cal_data){
	int status = 0, i = 0;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	status = Laura_Read_Calibration_Data_From_File(cal_data, SIZE_OF_LAURA_CALIBRATION_DATA);
        if(status < 0){
                LOG_Handler(LOG_ERR, "%s: Load calibration fail!!\n", __func__);
		if(vfile!=NULL){
			seq_printf(vfile,"No calibration data!!\n");
		}
                return status;
        }

	for(i = 0; i < SIZE_OF_LAURA_CALIBRATION_DATA; i++){
                swap_data(cal_data+i);
        }

	LOG_Handler(LOG_CDBG,"Cal data: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
		cal_data[0], cal_data[1], cal_data[2], cal_data[3], cal_data[4],
		cal_data[5], cal_data[6], cal_data[7], cal_data[8], cal_data[9]);

	if(vfile!=NULL){
		seq_printf(vfile,"%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
					cal_data[0], cal_data[1], cal_data[2], cal_data[3], cal_data[4],
				cal_data[5], cal_data[6], cal_data[7], cal_data[8], cal_data[9]);

	}

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return status;
}

int Olivia_Read_Calibration_Value_From_File(struct seq_file *vfile, uint16_t *cal_data){
	int status = 0, i = 0;
	char log_buf[SIZE_OF_OLIVIA_CALIBRATION_DATA*2*8];
	int output_len;
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	status = Laura_Read_Calibration_Data_From_File(cal_data, SIZE_OF_OLIVIA_CALIBRATION_DATA+CONFIDENCE_LENGTH);
        if(status < 0){
                LOG_Handler(LOG_ERR, "%s: Load calibration fail!!\n", __func__);
		if(vfile!=NULL){
			seq_printf(vfile,"No calibration data!!\n");
		}
                return status;
        }

	for(i = 0; i < SIZE_OF_OLIVIA_CALIBRATION_DATA+CONFIDENCE_LENGTH; i++){
                swap_data(cal_data+i);
        }

	if((output_len=format_hex_string(log_buf,sizeof(log_buf),cal_data,SIZE_OF_OLIVIA_CALIBRATION_DATA+CONFIDENCE_LENGTH)) != -1)
	{
		LOG_Handler(LOG_CDBG,"%s: Cal data (log buf len %d)\n%s",__func__,output_len,log_buf);
	}
	else
	{
		LOG_Handler(LOG_CDBG,"Cal data part, log_buf not enough: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
			cal_data[0], cal_data[1], cal_data[2], cal_data[3], cal_data[4],
			cal_data[5], cal_data[6], cal_data[7], cal_data[8], cal_data[9]);
	}

	if(vfile!=NULL){
		for(i=0; i<SIZE_OF_OLIVIA_CALIBRATION_DATA; i++)
			seq_printf(vfile,"%04x",cal_data[i]);
		seq_printf(vfile,"\n");

	}

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return status;
}


/** @brief laura read range
*
*	@param dev_t the laser focus controller
*
*/
#if 0
uint16_t Laura_device_read_range(struct msm_laser_focus_ctrl_t *dev_t)
{
	uint16_t RawRange = 0, i2c_read_data = 0;
	int status;
	struct timeval start, now;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	start = get_current_time();

	/* read count +1  */
	read_range_log_count++;

	/* Verify status is MCPU on */
	while(1){
		status = CCI_I2C_RdWord(dev_t, DEVICE_STATUS, &i2c_read_data);
		if (status < 0){
			return status;
		}

		//include MCPU_ON
		if(i2c_read_data == STATUS_MEASURE_ON){
			break;
		}

		/* Check if time out */
		now = get_current_time();
              if(is_timeout(start,now,5)){
			LOG_Handler(LOG_ERR, "%s: Verify MCPU status time out - register(0x06): 0x%x\n", __func__, i2c_read_data);
					return OUT_OF_RANGE;
              }

		LOG_Handler(LOG_DBG, "%s: register(0x06):0x%x!!\n", __func__,i2c_read_data);
	}

#if READ_RETRY_FLAG
	while(1){
#endif
		/* Trigger single measure */
		status = CCI_I2C_WrWord(dev_t, COMMAND_REGISTER, (SINGLE_MEASURE|VALIDATE_CMD));
		if (status < 0){
				return status;
		}

		/* Wait until data ready */
		while(1){
			status = CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
			if (status < 0){
				return status;
			}

			if(i2c_read_data & NEW_DATA_IN_RESULT_REG){
				break;
			}

			/* Check if time out */
			now = get_current_time();
					if(is_timeout(start,now,80)){
				LOG_Handler(LOG_ERR, "%s: Wait data ready time out - register(0x00): 0x%x\n", __func__, i2c_read_data);
						return OUT_OF_RANGE;
					}

			/* Delay: waitting laser sensor sample ready */
			//usleep(READ_DELAY_TIME);
			usleep_range(READ_DELAY_TIME,READ_DELAY_TIME);
		}

		/* Read distance */
		status = CCI_I2C_RdWord(dev_t, RESULT_REGISTER, &RawRange);
		if (status < 0){
				return status;
		}

		/* Check if target is out of field of view */
		if((RawRange&ERROR_CODE_MASK)==NO_ERROR && (RawRange&VALID_DATA)){
#if READ_OUTPUT_LIMIT_FLAG == 0
			/* Get real range */
                        LOG_Handler(LOG_DBG, "%s: Non-shift Read range:%d\n", __func__, RawRange);
#endif
			RawRange = (RawRange&DISTANCE_MASK)>>2;
#if READ_OUTPUT_LIMIT_FLAG == 0
			LOG_Handler(LOG_CDBG, "%s: Read range:%d\n", __func__, RawRange);
#endif
#if READ_OUTPUT_LIMIT_FLAG
			/* Display distance */
			if(read_range_log_count >= LOG_SAMPLE_RATE){
				read_range_log_count = 0;
				LOG_Handler(LOG_CDBG, "%s: Read range:%d\n", __func__, RawRange);
			}
#endif
#if READ_RETRY_FLAG
			break;
#endif
		}
		else {
			if((RawRange&0x2000)==0x2000){
				LOG_Handler(LOG_ERR, "%s: The target is near of field of view!!\n", __func__);
			}else if((RawRange&0x4000)==0x4000){
				LOG_Handler(LOG_ERR, "%s: The target is out of field of view!!\n", __func__);
			}else{
				LOG_Handler(LOG_ERR, "%s: Read range fail!!\n", __func__);
			}
#if READ_RETRY_FLAG == 0
			return OUT_OF_RANGE;
#endif
		}
#if READ_RETRY_FLAG
		/* Check if time out */
		now = get_current_time();
				if(is_timeout(start,now,TIMEOUT_VAL)){
			LOG_Handler(LOG_ERR, "%s: Read range time out!!\n", __func__);
				return OUT_OF_RANGE;
				}
	}
#endif

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return RawRange;
}
#endif

#if 0
int16_t Laura_device_read_range(struct msm_laser_focus_ctrl_t *dev_t, int *errStatus)
{
	uint16_t RawRange = 0, i2c_read_data = 0, realRange = 0, RawConfidence = 0, confidence_level = 0;
	int status;
	struct timeval start, now;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	start = get_current_time();

	/* Init debug raw data */
	init_debug_raw_data();

	/* read count +1  */
	read_range_log_count++;

	/* Verify status is MCPU on */
	while(1){
		status = CCI_I2C_RdWord(dev_t, 0x06, &i2c_read_data);
		if (status < 0){
			return status;
		}
		i2c_read_data = swap_data(i2c_read_data);

		if(i2c_read_data == STATUS_MEASURE_ON){
			break;
		}

		/* Check if time out */
		now = get_current_time();
              if(is_timeout(start,now,5)){
			LOG_Handler(LOG_ERR, "%s: Verify MCPU status time out - register(0x06): 0x%x\n", __func__, i2c_read_data);
					return OUT_OF_RANGE;
              }

		LOG_Handler(LOG_DBG, "%s: register(0x06):0x%x!!\n", __func__,i2c_read_data);

	}

#if READ_RETRY_FLAG
	while(1){
#endif
		/* Trigger single measure */
		status = CCI_I2C_WrWord(dev_t, 0x04, swap_data(0x0081));
		if (status < 0){
				return status;
		}

		/* Wait until data ready */
		while(1){
			status = CCI_I2C_RdWord(dev_t, 0x00, &i2c_read_data);
			if (status < 0){
				return status;
			}
			i2c_read_data = swap_data(i2c_read_data);

			if((int)(i2c_read_data&0x10) == 0x10){
				break;
			}

			/* Check if time out */
			now = get_current_time();
					if(is_timeout(start,now,80)){
				LOG_Handler(LOG_ERR, "%s: Wait data ready time out - register(0x00): 0x%x\n", __func__, i2c_read_data);
						return OUT_OF_RANGE;
					}

			/* Delay: waitting laser sensor sample ready */
			//usleep(READ_DELAY_TIME);
			usleep_range(READ_DELAY_TIME,READ_DELAY_TIME);
		}

		/* Read distance */
		status = CCI_I2C_RdWord(dev_t, 0x08, &RawRange);
		if (status < 0){
				return status;
		}
		RawRange = swap_data(RawRange);
		debug_raw_range = RawRange;
		LOG_Handler(LOG_DBG, "%s: [SHOW_LOG] Non-shift Read range:%d\n", __func__, RawRange);

		/* Check if target is valid */
		/* RawRange bit 15 is 1 */
		if((RawRange&0x8000)==0x8000){
			/* RawRange bit 13,14 are 00 */
			if((RawRange&0x6000)==0x00){

				/* Get real range success */
				realRange = (RawRange&0x1fff)>>2;

				/* [Issue solution] Inf report smale value :
				 * When the distance measured < 250mm and confidence level < 15, the result is invalid */

				/* Read result confidence level */
				status = CCI_I2C_RdWord(dev_t, 0x0A, &RawConfidence);
				if (status < 0){
					return status;
				}
				RawConfidence = swap_data(RawConfidence);
				debug_raw_confidence = RawConfidence;
				confidence_level = (RawConfidence&0x7fff)>>4;
				LOG_Handler(LOG_DBG,"%s: [SHOW_LOG] confidence level is: %d (Raw data:%d)\n", __func__, confidence_level, RawConfidence);

				/* distance <= 400 and confidence < 11*400/distance
				* , distance > 400 and confidence < 11
				* or distance = 0,
				* then ignored this distance measurement.
				* */
                if((realRange <= 400 && confidence_level < (11*400/realRange))
				  || (realRange > 400 && confidence_level < 11)
				  || realRange == 0){
					LOG_Handler(LOG_DBG, "%s: Read range fail (range,confidence level): (%d,%d)\n", __func__,realRange,confidence_level);
					*errStatus = 0;
					realRange = OUT_OF_RANGE;
                }
				else{
					*errStatus = 0;
				}

#if READ_OUTPUT_LIMIT_FLAG
				/* Display distance */
				if(read_range_log_count >= LOG_SAMPLE_RATE){
					read_range_log_count = 0;
					LOG_Handler(LOG_CDBG, "%s: Read range:%d\n", __func__, realRange);
				}
#endif
			}
			/* RawRange bit 13,14 are 11 */
			else if((RawRange&0x6000)==0x6000){
				LOG_Handler(LOG_DBG, "%s: Read range bit 13, 14, 15 are '1'(%d)\n", __func__,RawRange);
				*errStatus = RANGE_ERR_NOT_ADAPT;
				realRange = OUT_OF_RANGE;
			}
			/* RawRange bit 13 is 1, but bit 14 is 0 */
			else if((RawRange&0x2000)==0x2000){
				LOG_Handler(LOG_DBG, "%s: The target is near of field of view(%d)!!\n", __func__,RawRange);
				*errStatus = 0;
				realRange = 0;
			}
			/* RawRange bit 13 is 0, but bit 14 is 1 */
			else if((RawRange&0x4000)==0x4000){
				LOG_Handler(LOG_DBG, "%s: The target is out of field of view(%d)!!\n", __func__,RawRange);
				*errStatus = 0;
				realRange = OUT_OF_RANGE;
			}
			else{
				LOG_Handler(LOG_DBG, "%s: Read range fail(%d)!!\n", __func__,RawRange);
				*errStatus = 0;
				realRange = OUT_OF_RANGE;
			}
#if READ_RETRY_FLAG
			break;
#endif
		}
		/* RawRange bit 15 is 0 */
		else {
			/* RawRange bit 13 is 1, but bit 14 is 0 */
			if((RawRange&0x2000)==0x2000){
				LOG_Handler(LOG_ERR, "%s: The target is near of field of view(%d)!!\n", __func__,RawRange);
				*errStatus = RANGE_ERR;
				realRange = 0;
			}
			/* RawRange bit 13 is 0, but bit 14 is 1 */
			else if((RawRange&0x4000)==0x4000){
				LOG_Handler(LOG_ERR, "%s: The target is out of field of view(%d)!!\n", __func__,RawRange);
				*errStatus = RANGE_ERR;
				realRange = OUT_OF_RANGE;
			}
			/* RawRange bit 13,14 are 11 or 00 */
			else{
				LOG_Handler(LOG_ERR, "%s: Read range fail(%d)!!\n", __func__,RawRange);
				*errStatus = RANGE_ERR;
				realRange = OUT_OF_RANGE;
			}
#if READ_RETRY_FLAG == 0
			//return OUT_OF_RANGE;
#endif
		}
#if READ_RETRY_FLAG
		/* Check if time out */
		now = get_current_time();
        if(is_timeout(start,now,TIMEOUT_VAL)){
			LOG_Handler(LOG_ERR, "%s: Read range time out!!\n", __func__);
		return OUT_OF_RANGE;
        }
		//usleep(1);
		usleep_range(1,1);
	}
#endif

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return (int)realRange;
}
#endif

#define	MCPU_ON_TIME_OUT_ms		5
#define	MEASURE_TIME_OUT_ms		100

int Verify_MCPU_On_by_Time(struct msm_laser_focus_ctrl_t *dev_t){
	struct timeval start,now;
	uint16_t i2c_read_data = 0;
	int status=0;
	O_get_current_time(&start);
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);
	while(1){
		status = CCI_I2C_RdWord(dev_t, DEVICE_STATUS, &i2c_read_data);
		if (status < 0)
			return status;

		//include MCPU_ON
		if((i2c_read_data & STATUS_MASK) == STATUS_MCPU_ON){
			LOG_Handler(LOG_DBG, "%s: in MCPU_ON\n", __func__);
			break;
		}
		else
		{
			LOG_Handler(LOG_DBG, "%s: status 0x%X NOT ON, is %s\n", __func__,i2c_read_data,getMCPUState(i2c_read_data));
		}
		 O_get_current_time(&now);
              if(is_timeout(start,now,MCPU_ON_TIME_OUT_ms)){
			LOG_Handler(LOG_ERR, "%s: fail (time out)\n", __func__);
					return -OUT_OF_RANGE;
              }
        //usleep_range(READY_DELAY_TIME,READY_DELAY_TIME);
	}
	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);
	return status;
}

int Verify_Range_Data_Ready(struct msm_laser_focus_ctrl_t *dev_t){
	struct timeval start,now;
	uint16_t i2c_read_data = 0;
	int status=0;
	int delay_time = 50;
	int count = 0;
	O_get_current_time(&start);
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);
       while(1){
		status = CCI_I2C_RdWord(dev_t, ICSR, &i2c_read_data);
		if (status < 0)
			return status;
		count++;
		now = get_current_time();
		if(i2c_read_data & NEW_DATA_IN_RESULT_REG){
			LOG_Handler(LOG_DBG, "%s: range data ready,count=%d, cost %lld ms\n",
			 __func__,
			 count,
			((now.tv_sec * 1000000 + now.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec) )/1000
			 );
			break;
		}

				if(is_timeout(start,now,MEASURE_TIME_OUT_ms)){
			LOG_Handler(LOG_ERR, "%s: fail (time out)\n", __func__);
                    return -OUT_OF_RANGE;
				}
		/* Delay: waitting laser sensor sample ready */
		//usleep(READ_DELAY_TIME);
		usleep_range(1000*delay_time,1000*delay_time+10);
		if(delay_time > 2)
		{
			delay_time = delay_time/2;
			if(delay_time < 2)
				delay_time = 2;
		}
       }
       LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);
	return status;


}

int	Read_Range_Data(struct msm_laser_focus_ctrl_t *dev_t){
	uint16_t RawRange = 0, Range = 0, error_status =0;
	uint16_t RawConfidence = 0, confidence_level =0;
	int status;
	int errcode;
	int result=0;

	uint16_t IT_verify;

	int confA = 1300;
	int confC = 5600;
	int ItB = 5000;

	thd = Settings[CONFIDENCE_FACTOR];
	limit = Settings[DISTANCE_THD];
	thd_near_mm = Settings[NEAR_LIMIT];

	CCI_I2C_WrByte(dev_t, 0x18, 0x06);
	CCI_I2C_WrByte(dev_t, 0x19, 0x20);
	CCI_I2C_RdWord(dev_t, I2C_DATA_PORT, &IT_verify);
//
	init_debug_raw_data();

	status = CCI_I2C_RdWord(dev_t, RESULT_REGISTER, &RawRange);
	if (status < 0)
				return status;

	debug_raw_range = RawRange;
	Range = (RawRange&DISTANCE_MASK)>>2;
	LOG_Handler(LOG_DBG, "%s:%d\n",__func__,Range);

	error_status = RawRange&ERROR_CODE_MASK;
	if(RawRange&VALID_DATA){

		Range = (RawRange&DISTANCE_MASK)>>2;

		errcode = 0;
		CCI_I2C_RdWord(dev_t, 0x0A, &RawConfidence);
		if (status < 0)
				return status;

		debug_raw_confidence = RawConfidence;
		confidence_level = (RawConfidence&0x7ff0)>>4;

		if(dev_t->device_state == MSM_LASER_FOCUS_DEVICE_APPLY_CALIBRATION)
		{
			confA = Settings[CONFIDENCE10];
			confC = Settings[CONFIDENCE_THD];
		}
		else
		{
			confA = DEFAULT_CONFIDENCE10;
			confC = DEFAULT_CONFIDENCE_THD;
		}
		ItB =  Settings[IT];

		if(error_status==NO_ERROR){

			if(IT_verify < ItB)
			{
				if(confidence_level < confA){
					result = 1;
					Range =	9999;//means 0, for DIT, 9999
					errcode = RANGE_ERR_NOT_ADAPT;
				}
			}
			else
			{
				if( ItB*(confidence_level) < ItB*confA- (IT_verify - ItB)*(confC - confA) ){
					result = 2;
					Range =	9999;//means 0, for DIT, 9999
					errcode = RANGE_ERR_NOT_ADAPT;
				}
			}
			if(!result && Range == 0){
				result = 3;
				Range =	9999;//means 0, for DIT, 9999
				errcode = RANGE_ERR_NOT_ADAPT;
			}
			if(!result && compareConfidence(confidence_level,Range,thd,limit)){
				result = 4;
				Range =	9999;//means 0, for DIT, 9999
				errcode = RANGE_ERR_NOT_ADAPT;
			}
			if(!result && Range==2047){
				result = 8;
				Range =	OUT_OF_RANGE;
				errcode = RANGE_ADAPT;
			}
			if(!result && Range >= limit){
				result = 9;
				Range =	OUT_OF_RANGE;
				errcode = RANGE_ADAPT;
			}
			if(!result)
			{
				result = 0;
				if(Range < 100)
				{
					errcode = RANGE_ADAPT_WITH_LESS_ACCURACY;//new error code for DIT
				}
				else
				{
					errcode = RANGE_ADAPT;
				}
			}
		}
		else
		{
			if(error_status==GENERAL_ERROR)
			{
				result = 5;
				Range =	9999;//means 0, for DIT, 9999
				errcode = RANGE_ERR_NOT_ADAPT;
			}
			else if(error_status==NEAR_FIELD){
				result = 6;
				Range = 0;//IN_RANGE ?
				errcode = RANGE_ADAPT;
			}
			else if(error_status==FAR_FIELD){
				result = 7;
				Range =	OUT_OF_RANGE;
				errcode = RANGE_ADAPT;
			}
		}

	}
	else
	{
		result = 10;
		Range = 9999;//means 0, for DIT, 9999
		errcode = RANGE_ERR_NOT_ADAPT;
	}

	ErrCode =  errcode;
	if(continuous_measure)
	{
		Range_Cached = Range;

		if(!measure_cached_range_updated)
		{
			measure_cached_range_updated = true;
			LOG_Handler(LOG_CDBG, "%s: Range_Cached first updated!\n", __func__);
		}
	}

	if((ioctrl_read_value_cnt%LOG_SAMPLE_RATE == 0)||proc_read_value_cnt)
	{
		LOG_Handler(LOG_CDBG,"%s: conf(%d)  confA(%d) confC(%d) ItB(%d) IT_verify(0x2006:%d)\n", __func__, confidence_level, confA,confC,ItB,IT_verify);
		LOG_Handler(LOG_CDBG, "%s: status(%d) thd(%d) limit(%d) near(%d) Confidence(%d)\n", __func__,
			error_status>>13, thd, limit, thd_near_mm, confidence_level);
		LOG_Handler(LOG_CDBG, "%s: Range(%d) ErrCode(%d) RawRange(%d) result(%d)\n", __func__,Range,errcode,(RawRange&DISTANCE_MASK)>>2, result);

		if(proc_read_value_cnt)
			proc_read_value_cnt = 0;//reset to 0 to avoid ioctrl always show, cat value in single measure mode
	}

	return Range;

}

int Olivia_device_read_range(struct msm_laser_focus_ctrl_t *dev_t)
{
	int status, Range=0;
	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	read_range_log_count++;

	status = Verify_MCPU_On_by_Time(dev_t);
	if(status != 0) goto read_err;

	do{
		/* Trigger single measure */
		status = CCI_I2C_WrWord(dev_t, COMMAND_REGISTER, (SINGLE_MEASURE|VALIDATE_CMD));

		status = Verify_Range_Data_Ready(dev_t);
		if(status != 0)
		{
			goto read_err;
		}

		Range = Read_Range_Data(dev_t);
		if(Range < 0){
			status = Range;
			goto read_err;
		}

		if(continuous_measure && Disable_Device){//continuous mode exit
			LOG_Handler(LOG_CDBG,"laser disabled, stop measuring, return status %d\n",status);
			return status;
		}
		else if(!continuous_measure)//switch normal mode exit
		{
			LOG_Handler(LOG_DBG,"continuous_measure mode disabled, stop measuring, return range %d\n",Range);
			return Range;
		}
		else
			msleep(40);

	}while(continuous_measure);

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);
	return Range;

read_err:
	LOG_Handler(LOG_ERR, "%s: Exit with Error: %d\n", __func__, status);
	return status;

}



/** @brief MCPU Contorller
*
*	@param dev_t the laser focus controller
*	@param mode the MCPU go to status
*
*/
int Laura_MCPU_Controller(struct msm_laser_focus_ctrl_t *dev_t, int mode){
	int status;
#if DEBUG_LOG_FLAG
	uint16_t i2c_read_data = 0;
#endif

	switch(mode){
		case MCPU_ON:
			/* Enable MCPU to run coming out of standby */
			LOG_Handler(LOG_DBG, "%s: MCPU ON procdure\n", __func__);
			//Set init done, 0x14, 0x0600
			status = CCI_I2C_WrWord(dev_t, PMU_CONFIG, (PATCH_MEM_EN|MCPU_INIT_STATE));
			if (status < 0){
						return status;
				}

			/* Wake up MCPU to ON mode */
			status = CCI_I2C_WrWord(dev_t, COMMAND_REGISTER, (MCPU_TO_ON|VALIDATE_CMD));
			if (status < 0){
				return status;
			}
			break;
		case MCPU_OFF:
			/* Enable patch memory */
			LOG_Handler(LOG_DBG, "%s: MCPU OFF procdure\n", __func__);
			status = CCI_I2C_WrWord(dev_t, PMU_CONFIG, (PATCH_MEM_EN|PATCH_CODE_LD_EN));
			if (status < 0){
						return status;
				}

			/* Go MCPU to OFF status */
			status = CCI_I2C_WrWord(dev_t, COMMAND_REGISTER, (GO_MCPU_OFF|VALIDATE_CMD));
			if (status < 0){
					return status;
			}
			break;
		case MCPU_STANDBY:
			/* Change MCUP to standby mode */
			LOG_Handler(LOG_DBG, "%s: MCPU STANDBY procdure\n", __func__);
			status = CCI_I2C_WrWord(dev_t, COMMAND_REGISTER, (GO_STANDBY|VALIDATE_CMD));
			if (status < 0){
				return status;
				}
			break;
		default:
			LOG_Handler(LOG_ERR, "%s MCPU mode invalid (%d)\n", __func__, mode);
			break;
	}

	/* wait hardware booting(least 500us) */
	//usleep(MCPU_DELAY_TIME);
	usleep_range(MCPU_DELAY_TIME,MCPU_DELAY_TIME);

#if DEBUG_LOG_FLAG
	/* Verify MCPU status */
	status = CCI_I2C_RdWord(dev_t, DEVICE_STATUS, &i2c_read_data);
	if (status < 0){
				return status;
       }
	LOG_Handler(LOG_DBG, "%s: register(0x06):0x%x, state %s\n", __func__,i2c_read_data,getMCPUState(i2c_read_data));

#endif

	return status;
}

int getTOF(uint16_t* config_normal,uint16_t* config_K10,uint16_t* config_K40,int LAURA_CONFIG_SIZE){

	int buf[3*LAURA_CONFIG_SIZE];
	int i=0;
	int status=0;

	for(i=0; i<3*LAURA_CONFIG_SIZE; i++)
		buf[i]=0;

	status = Sysfs_read_word_seq("/factory/Olivia_conf.txt",buf,3*LAURA_CONFIG_SIZE);
	if(status<0)
		return status;

	for(i=0; i< LAURA_CONFIG_SIZE; i++)
		config_normal[i]=buf[i]&0xffff;

	for(i=0; i< LAURA_CONFIG_SIZE; i++)
		config_K10[i]=buf[i+LAURA_CONFIG_SIZE]&0xffff;

	for(i=0; i< LAURA_CONFIG_SIZE; i++)
		config_K40[i]=buf[i+(2*LAURA_CONFIG_SIZE)]&0xffff;

	return 0;
}
/** @brief Initialize Laura tof configure
*
*	@param dev_t the laser focus controller
*	@param config the configuration param
*
*/
int Laura_device_UpscaleRegInit(struct msm_laser_focus_ctrl_t *dev_t, uint16_t *config)
{
	int status = 0;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);
#if 1
//#if DEBUG_LOG_FLAG
	LOG_Handler(LOG_CDBG, "%s: config:(0x%x,0x%x,0x%x,0x%x,0x%x,0x%x)\n", __func__,
		config[0], config[1], config[2], config[3], config[4], config[5]);
#endif

	/* Change the default VCSEL threshold and VCSEL peak */
       status = CCI_I2C_WrWord(dev_t, 0x0C, config[0]);
       if (status < 0){
               return status;
       }

       status = CCI_I2C_WrWord(dev_t, 0x0E, config[1]);
       if (status < 0){
               return status;
       }

       status = CCI_I2C_WrWord(dev_t, 0x20, config[2]);
       if (status < 0){
               return status;
       }

       status = CCI_I2C_WrWord(dev_t, 0x22, config[3]);
       if (status < 0){
               return status;
       }

      status = CCI_I2C_WrWord(dev_t, 0x24, config[4]);
       if (status < 0){
               return status;
       }

       status = CCI_I2C_WrWord(dev_t, 0x26, config[5]);
       if (status < 0){
               return status;
       }

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return status;
}
#define	MCPU_SWITCH_TIMEOUT_ms		15
int WaitMCPUOn(struct msm_laser_focus_ctrl_t *dev_t){
	uint16_t i2c_read_data = 0;
	int status=0;
	struct timeval start,now;
	O_get_current_time(&start);

	Laura_MCPU_Controller(dev_t, MCPU_ON);
	while(1){
		status = CCI_I2C_RdWord(dev_t, DEVICE_STATUS, &i2c_read_data);
		if (status < 0)
			break;

		//include MCPU_ON
		if(i2c_read_data == STATUS_MEASURE_ON){
			LOG_Handler(LOG_DBG, "%s: in MCPU_ON\n", __func__);
			break;
		}

		 O_get_current_time(&now);
		if(is_timeout(start,now,MCPU_SWITCH_TIMEOUT_ms)){
			LOG_Handler(LOG_ERR, "%s:  time out - register(0x06): 0x%x\n", __func__, i2c_read_data);
			status = -TIMEOUT_VAL;
			break;
		}
		usleep_range(READY_DELAY_TIME,READY_DELAY_TIME+1);
	}

	return status;
}
/** @brief Wait device go to standby mode
*
*	@param dev_t the laser focus controller
*
*/
#define	RETRY_STANDBY		50
int WaitMCPUStandby(struct msm_laser_focus_ctrl_t *dev_t){
	int status = 0;
	uint16_t i2c_read_data = 0;
	int cnt=0;

	struct timeval start, now;
	O_get_current_time(&start);

	/* Wait MCPU standby */
	Laura_MCPU_Controller(dev_t, MCPU_STANDBY);
	while(1){
		cnt++;
		status = CCI_I2C_RdWord(dev_t, DEVICE_STATUS, &i2c_read_data);
		if (status < 0)	break;

		i2c_read_data &= STATUS_MASK;

		if(cnt >= RETRY_STANDBY){
			LOG_Handler(LOG_ERR, "%s:  retry %d times, reg(0x06): 0x%x\n", __func__, RETRY_STANDBY, i2c_read_data);
			Laura_MCPU_Controller(dev_t, MCPU_STANDBY);
		}

		if(i2c_read_data == STATUS_STANDBY){
			LOG_Handler(LOG_DBG, "%s: in STANDBY MODE, reg(0x06): 0x%x\n", __func__, i2c_read_data);
			break;
		}

		//Check time out
		O_get_current_time(&now);
		if(is_timeout(start,now,MCPU_SWITCH_TIMEOUT_ms)){
			LOG_Handler(LOG_ERR, "%s:  time out - register(0x06): 0x%x\n", __func__, i2c_read_data);
			status = -TIMEOUT_VAL;
			break;
        }
		msleep(1);
	}

	return status;
}
int WaitMCPUOff(struct msm_laser_focus_ctrl_t *dev_t){

	int status = 0;
	uint16_t i2c_read_data = 0;
	struct timeval start, now;
	O_get_current_time(&start);

	// Set then Verify status is MCPU off
	Laura_MCPU_Controller(dev_t, MCPU_OFF);
	while(1){
		status = CCI_I2C_RdWord(dev_t, DEVICE_STATUS, &i2c_read_data);
		if (status < 0)
			break;

		i2c_read_data &= STATUS_MASK;
		if(i2c_read_data == STATUS_MCPU_OFF){
			LOG_Handler(LOG_DBG, "%s: in OFF MODE, reg(0x06): 0x%x\n", __func__, i2c_read_data);
			break;
		}

		if(is_timeout(start,now,MCPU_SWITCH_TIMEOUT_ms)){
			LOG_Handler(LOG_ERR, "%s: time out - register(0x06): 0x%x\n", __func__, i2c_read_data);
			status = -TIMEOUT_VAL;
			break;
		}
		udelay(500);
	}
	return status;
}
/** @brief Configure i2c interface
*
*	@param dev_t the laser focus controller
*
*/
int Laura_Config_I2C_Interface(struct msm_laser_focus_ctrl_t *dev_t){
	int status = 0;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* Configure I2C interface */
	//include enable auto-increment
	status = CCI_I2C_WrWord(dev_t, 0x1C, 0x0065);
       if (status < 0){
               return status;
       }

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return status;
}

/** @brief Power up initialization without applying calibration data
*
*	@param dev_t the laser focus controller
*
*/
int Laura_No_Apply_Calibration(struct msm_laser_focus_ctrl_t *dev_t){
	int status = 0;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* Wake up MCPU to ON mode */
	Laura_MCPU_Controller(dev_t, MCPU_ON);

#if 0
	/* wait hardware booting(least 500us) */
	//usleep(MCPU_DELAY_TIME);
#endif

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return status;
}

/** @brief Power up initialization which apply calibration data
*
*	@param dev_t the laser focus controller
*
*/
int Laura_Apply_Calibration(struct msm_laser_focus_ctrl_t *dev_t){
	int status = 0;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	/* Load calibration data */
	Olivia_device_Load_Calibration_Value(dev_t);

	/* Control Signal Setting*/
	control_signal_setting(dev_t);

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return status;
}

void Olivia_read_FW_ChipID(struct msm_laser_focus_ctrl_t *dev_t)
{
	Laura_MCPU_Controller(dev_t, MCPU_OFF);

	getRegisterData(dev_t,0xFFC0,fw_version,FW_VERSION_LEN);
	getRegisterData(dev_t,0xC804,module_id,MODULE_ID_LEN);

    LOG_Handler(LOG_CDBG,"Olivia FW version is 0x%X 0x%X -> %d.%d\n",
						fw_version[0],fw_version[1],
						fw_version[0]&0b00111111, fw_version[1]
			   );
    LOG_Handler(LOG_DBG,"Module ID(Hex):%2x %2x %2x %2x %2x %2x %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n",
		   module_id[0],module_id[1],module_id[2],module_id[3],module_id[4],module_id[5],module_id[6],module_id[7],
		   module_id[8],module_id[9],module_id[10],module_id[11],module_id[12],module_id[13],module_id[14],module_id[15],
		   module_id[16],module_id[17],module_id[18],module_id[19],module_id[20],module_id[21],module_id[22],module_id[23],
		   module_id[24],module_id[25],module_id[26],module_id[27],module_id[28],module_id[29],module_id[30],module_id[31],
		   module_id[32],module_id[33]);

    LOG_Handler(LOG_DBG,"Module ID:%d %d %d %d %d %d %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d\n",
		   module_id[0],module_id[1],module_id[2],module_id[3],module_id[4],module_id[5],module_id[6],module_id[7],
		   module_id[8],module_id[9],module_id[10],module_id[11],module_id[12],module_id[13],module_id[14],module_id[15],
		   module_id[16],module_id[17],module_id[18],module_id[19],module_id[20],module_id[21],module_id[22],module_id[23],
		   module_id[24],module_id[25],module_id[26],module_id[27],module_id[28],module_id[29],module_id[30],module_id[31],
		   module_id[32],module_id[33]);
	LOG_Handler(LOG_CDBG,"Module ID:%c%c%c%c%c\n",
		module_id[1],module_id[2],module_id[3],module_id[4],module_id[5]);

	module_id_flag=true;
}

/** @brief Get module id from chip
*
*       @param dev_t the laser focus controller
*
*/
void Laura_Get_Module_ID_From_Chip(struct msm_laser_focus_ctrl_t *dev_t){
	int status = 0, i = 0;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);
	Laura_MCPU_Controller(dev_t, MCPU_OFF);
	status = CCI_I2C_WrByte(dev_t, 0x18, 0x04);
        status = CCI_I2C_WrByte(dev_t, 0x19, 0xC8);

	for(i = 0; i < 34; i++){
				CCI_I2C_RdByte(dev_t, 0x1A, &module_id[i]);
        }

	LOG_Handler(LOG_DBG,"Module ID(Hex):%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n",
		module_id[0],module_id[1],module_id[2],module_id[3],module_id[4],module_id[5],module_id[6],module_id[7],
		module_id[8],module_id[9],module_id[10],module_id[11],module_id[12],module_id[13],module_id[14],module_id[15],
		module_id[16],module_id[17],module_id[18],module_id[19],module_id[20],module_id[21],module_id[22],module_id[23],
		module_id[24],module_id[25],module_id[26],module_id[27],module_id[28],module_id[29],module_id[30],module_id[31],
		module_id[32],module_id[33]);

	LOG_Handler(LOG_DBG,"Module ID:%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d\n",
		module_id[0],module_id[1],module_id[2],module_id[3],module_id[4],module_id[5],module_id[6],module_id[7],
		module_id[8],module_id[9],module_id[10],module_id[11],module_id[12],module_id[13],module_id[14],module_id[15],
		module_id[16],module_id[17],module_id[18],module_id[19],module_id[20],module_id[21],module_id[22],module_id[23],
		module_id[24],module_id[25],module_id[26],module_id[27],module_id[28],module_id[29],module_id[30],module_id[31],
		module_id[32],module_id[33]);

	LOG_Handler(LOG_DBG,"Module ID:%d%d%d%d%d%d\n",
		module_id[0],module_id[1],module_id[2],module_id[3],module_id[4],module_id[5]);

	module_id_flag=true;
	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

}

/** @brief Get Chip from driver
*
*       @param dev_t the laser focus controller
*       @param vfile
*
*/
void Laura_Get_Module_ID(struct msm_laser_focus_ctrl_t *dev_t, struct seq_file *vfile){

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);

	if(!module_id_flag){
		Laura_MCPU_Controller(dev_t, MCPU_OFF);
		Olivia_read_FW_ChipID(dev_t);
		//Laura_Get_Module_ID_From_Chip(dev_t);
	}
	else{
		LOG_Handler(LOG_DBG,"Module ID:%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
				module_id[0],module_id[1],module_id[2],module_id[3],module_id[4],module_id[5],module_id[6],module_id[7],
				module_id[8],module_id[9],module_id[10],module_id[11],module_id[12],module_id[13],module_id[14],module_id[15],
				module_id[16],module_id[17],module_id[18],module_id[19],module_id[20],module_id[21],module_id[22],module_id[23],
				module_id[24],module_id[25],module_id[26],module_id[27],module_id[28],module_id[29],module_id[30],module_id[31],
				module_id[32],module_id[33]);
	}

	if(vfile!=NULL){
                seq_printf(vfile,"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
                        module_id[0],module_id[1],module_id[2],module_id[3],module_id[4],module_id[5],module_id[6],module_id[7],
                        module_id[8],module_id[9],module_id[10],module_id[11],module_id[12],module_id[13],module_id[14],module_id[15],
                        module_id[16],module_id[17],module_id[18],module_id[19],module_id[20],module_id[21],module_id[22],module_id[23],
                        module_id[24],module_id[25],module_id[26],module_id[27],module_id[28],module_id[29],module_id[30],module_id[31],
                        module_id[32],module_id[33]);
        }

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);
}


/** @brief Verify firmware version
*
*	@param dev_t the laser focus controller
*
*/
bool Laura_FirmWare_Verify(struct msm_laser_focus_ctrl_t *dev_t){
	int status = 0;
	uint16_t fw_major_version, fw_minor_version;

	LOG_Handler(LOG_FUN, "%s: Enter\n", __func__);



#if 0
	/* wait hardware handling (least 500us) */
	usleep(MCPU_DELAY_TIME);
#endif

	status = CCI_I2C_WrByte(dev_t, 0x18, 0xC0);
	status = CCI_I2C_WrByte(dev_t, 0x19, 0xFF);

	status = CCI_I2C_RdByte(dev_t, I2C_DATA_PORT, &fw_major_version);
	fw_major_version = fw_major_version & 0x3F;
	status = CCI_I2C_RdByte(dev_t, I2C_DATA_PORT, &fw_minor_version);

	LOG_Handler(LOG_DBG, "%s: LSB: 0x%x ; MSB: 0x%x\n", __func__, fw_major_version, fw_minor_version);

	if( fw_major_version >= 0 && fw_minor_version >= 14 ){
		/* Can do calibraion */
		LOG_Handler(LOG_DBG, "%s: It can do calibration!!\n", __func__);
		return true;
	}
	else{
		/* Can not do calibraion */
		LOG_Handler(LOG_DBG, "%s: The fireware is too old, it can not do calibration!!\n", __func__);
		return false;
	}

	LOG_Handler(LOG_FUN, "%s: Exit\n", __func__);

	return false;
}


int Olivia_DumpKdata(struct msm_laser_focus_ctrl_t *dev_t, uint16_t* cal_data, uint16_t len ){
	int status = 0, i=0;
	uint16_t i2c_read_data;
	uint16_t len_byte = len*2;
	char log_buf[CAL_MSG_F0*2*8];
	int output_len;
	Laura_MCPU_Controller(dev_t, MCPU_STANDBY);
	//usleep(2000);
	usleep_range(2000,2000);
	status = CCI_I2C_RdWord(dev_t, DEVICE_STATUS, &i2c_read_data);
	if (status < 0){
				return status;
       }
	LOG_Handler(LOG_DBG, "%s: register(0x06):0x%x!!\n", __func__,i2c_read_data);

	Laura_MCPU_Controller(dev_t, MCPU_OFF);

	//usleep(2000);
	usleep_range(2000,2000);

	getRegisterData(dev_t,0xC848,f0_data,len_byte);

	for(i=0;i < len; i++){
		cal_data[i] = (f0_data[2*i] | f0_data[2*i+1]<<8);
	}

	if((output_len=format_hex_string(log_buf,sizeof(log_buf),cal_data,len))!=-1)
		LOG_Handler(LOG_DBG,"F0 Data( log buf len %d)\n%s",output_len,log_buf);
	else
		LOG_Handler(LOG_DBG,"F0 Data log buf not enough");

	return status;
}
