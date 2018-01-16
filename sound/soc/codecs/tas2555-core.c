/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software 
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
** Street, Fifth Floor, Boston, MA 02110-1301, USA.
**
** File:
**     tas2555-core.c
**
** Description:
**     TAS2555 common functions for Android Linux
**
** =============================================================================
*/

#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#include "tas2555.h"
#include "tas2555-core.h"

#define PPC_WITH_DRIVER_VERSION		0x010bc000
#define TAS2555_CAL_NAME    "/data/tas2555_cal.bin"

//set default PLL CLKIN to GPI2 (MCLK) = 0x00
#define TAS2555_DEFAULT_PLL_CLKIN 0x00

static void tas2555_load_calibration(struct tas2555_priv *pTAS2555,
	char *pFileName);
static void tas2555_load_data(struct tas2555_priv *pTAS2555, TData * pData,
	unsigned int nType);
static void tas2555_load_block(struct tas2555_priv *pTAS2555, TBlock * pBlock);
static void tas2555_load_configuration(struct tas2555_priv *pTAS2555,
	unsigned int nConfiguration, bool bLoadSame);
	
#define TAS2555_UDELAY 0xFFFFFFFE

#define FW_ERR_HEADER -1
#define FW_ERR_SIZE -2

#define TAS2555_BLOCK_PLL			0x00
#define TAS2555_BLOCK_MAIN_DEV_A	0x01
#define TAS2555_BLOCK_MAIN_DEV_B	0x08
#define TAS2555_BLOCK_COEFF_DEV_A	0x03
#define TAS2555_BLOCK_COEFF_DEV_B	0x0a
#define TAS2555_BLOCK_PRE_DEV_A		0x04
#define TAS2555_BLOCK_PRE_DEV_B		0x0B
#define TAS2555_BLOCK_POST			0x05
#define TAS2555_BLOCK_POST_POWER_UP	0x06

static unsigned int p_tas2555_default_data[] = {
	channel_both, TAS2555_ASI1_DAC_FORMAT_REG, 0x10,	//ASI1 DAC word length = 24 bits

	channel_both, TAS2555_PLL_CLKIN_REG, TAS2555_DEFAULT_PLL_CLKIN,	//PLL_CLKIN = GPIO1 (BCLK)
	channel_both, TAS2555_MAIN_CLKIN_REG, 0x0F,	//NDIV_MUX_CLKIN = PLL_CLK
	channel_both, TAS2555_PLL_P_VAL_REG, 0x01,	//PLL P = 1
//  TAS2555_PLL_J_VAL_REG,      0x10, //PLL J = 16
	channel_both, TAS2555_PLL_J_VAL_REG, 0x30,	//PLL J = 48 -> PLL_CLK = 1.536MHz * 48 = 73.728MHz
	channel_both, TAS2555_PLL_D_VAL_MSB_REG, 0x00,	//PLL D = 0
	channel_both, TAS2555_PLL_D_VAL_LSB_REG, 0x00,
	channel_both, TAS2555_PLL_N_VAL_REG, 0x03,	//PLL N = 3 -> NDIV_CLK = 24.576MHz
	channel_both, TAS2555_DAC_MADC_VAL_REG, 0x08,	//MDAC = 8
	channel_both, TAS2555_CLK_MISC_REG, 0x20,	//DSP CLK = PLL out
//  TAS2555_ISENSE_DIV_REG,     0x40, //Isense div and MADC final divider configure auto
	channel_both, TAS2555_ISENSE_DIV_REG, 0x00,	//Isense div and MADC final divider configure auto
//  TAS2555_RAMP_CLK_DIV_LSB_REG,   0x20, //ramp_clk divider = 32 so that 12.288MHz/32 = 384KHz
	channel_both, TAS2555_RAMP_CLK_DIV_LSB_REG, 0x40,	//ramp_clk divider = 64 so that 24.576MHz/64 = 384KHz
	channel_both, TAS2555_DSP_MODE_SELECT_REG, 0x22,	//DSP ROM mode 2, default coeffs

//  TAS2555_SPK_CTRL_REG,       0x74, //DAC channel gain
	channel_both, TAS2555_SPK_CTRL_REG, 0x7C,	//DAC channel gain
//  TAS2555_POWER_CTRL2_REG,    0xA3, //power up
//  TAS2555_POWER_CTRL1_REG,    0xF8, //power up
//  TAS2555_MUTE_REG,       0x00, //unmute
//  TAS2555_SOFT_MUTE_REG,      0x00, //soft unmute
//  TAS2555_CLK_ERR_CTRL,       0x09, //enable clock error detection on PLL
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

#define TAS2555_STARTUP_DATA_PLL_CLKIN_INDEX 5
static unsigned int p_tas2555_startup_data[] = {
	channel_both, TAS2555_CLK_ERR_CTRL, 0x00,	//disable clock error detection on PLL
	channel_both, TAS2555_PLL_CLKIN_REG, TAS2555_DEFAULT_PLL_CLKIN,
	channel_both, TAS2555_POWER_CTRL2_REG, 0xA0,	//Class-D, Boost power up
	channel_both, TAS2555_POWER_CTRL2_REG, 0xA3,	//Class-D, Boost, IV sense power up
	channel_both, TAS2555_POWER_CTRL1_REG, 0xF8,	//PLL, DSP, clock dividers power up
	channel_both, TAS2555_UDELAY, 2000,		//delay
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2555_unmute_data[] = {
	channel_both, TAS2555_MUTE_REG, 0x00,		//unmute
	channel_both, TAS2555_SOFT_MUTE_REG, 0x00,	//soft unmute
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2555_shutdown_data[] = {
	channel_both, TAS2555_SOFT_MUTE_REG, 0x01,	//soft mute
	channel_both, TAS2555_UDELAY, 10000,		//delay 10ms
	channel_both, TAS2555_MUTE_REG, 0x03,		//mute
	channel_both, TAS2555_PLL_CLKIN_REG, 0x0F,	//PLL clock input = osc
	channel_both, TAS2555_POWER_CTRL1_REG, 0x60,	//DSP power down
	channel_both, TAS2555_UDELAY, 2000,		//delay 2ms
	channel_both, TAS2555_POWER_CTRL2_REG, 0x00,	//Class-D, Boost power down
	channel_both, TAS2555_POWER_CTRL1_REG, 0x00,	//all power down
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};


static unsigned int p_tas2555_mute_DSP_down_data[] = {
	channel_both, TAS2555_MUTE_REG, 0x03,		//mute
	channel_both, TAS2555_PLL_CLKIN_REG, 0x0F,	//PLL clock input = osc
	channel_both, TAS2555_POWER_CTRL1_REG, 0x60,	//DSP power down
	channel_both, TAS2555_UDELAY, 0xFF,		//delay
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

static int tas2555_dev_load_data(struct tas2555_priv *pTAS2555,
	unsigned int *pData)
{
	int ret = 0;
	unsigned int n = 0;
	enum channel chl;
	unsigned int nRegister;
	unsigned int nData;

	do {
		chl = pData[n * 3];
		nRegister = pData[n * 3 + 1];
		nData = pData[n * 3 + 2];
		if (nRegister == TAS2555_UDELAY)
			udelay(nData);
		else if (nRegister != 0xFFFFFFFF){
			ret = pTAS2555->write(pTAS2555, chl, nRegister, nData);
			if(ret < 0) {
				dev_err(pTAS2555->dev, "Reg Write err %d\n", ret);
				break;
			}
		}
		n++;
	} while (nRegister != 0xFFFFFFFF);
	
	return ret;
}

int tas2555_load_default(struct tas2555_priv *pTAS2555)
{
	return tas2555_dev_load_data(pTAS2555, p_tas2555_default_data);
}

void tas2555_enable(struct tas2555_priv *pTAS2555, bool bEnable)
{
	dev_dbg(pTAS2555->dev, "Enable: %d\n", bEnable);
	if (bEnable) {
		if (!pTAS2555->mbPowerUp) {
			TConfiguration *pConfiguration;

			if (!pTAS2555->mbCalibrationLoaded) {
				tas2555_load_calibration(pTAS2555, TAS2555_CAL_NAME);
				pTAS2555->mbCalibrationLoaded = true;
			}
			dev_dbg(pTAS2555->dev, "Enable: load startup sequence\n");
			tas2555_dev_load_data(pTAS2555, p_tas2555_startup_data);
			if (pTAS2555->mpFirmware->mpConfigurations) {
				pConfiguration = &(pTAS2555->mpFirmware->mpConfigurations[pTAS2555->mnCurrentConfiguration]);
				tas2555_load_data(pTAS2555, &(pConfiguration->mData),
					TAS2555_BLOCK_POST_POWER_UP);
				if (pTAS2555->mbLoadConfigurationPostPowerUp) {
					dev_dbg(pTAS2555->dev,	"Enable: load configuration: %s, %s\n",
						pConfiguration->mpName, pConfiguration->mpDescription);
					tas2555_load_data(pTAS2555, &(pConfiguration->mData),
						TAS2555_BLOCK_COEFF_DEV_A);
					tas2555_load_data(pTAS2555, &(pConfiguration->mData),
						TAS2555_BLOCK_COEFF_DEV_B);
					pTAS2555->mbLoadConfigurationPostPowerUp = false;
					if (pTAS2555->mpCalFirmware->mnCalibrations) {
						dev_dbg(pTAS2555->dev, "Enable: load calibration\n");
						tas2555_load_block(pTAS2555, 
							&(pTAS2555->mpCalFirmware->mpCalibrations[pTAS2555->mnCurrentCalibration].mBlock));
						pTAS2555->mbLoadCalibrationPostPowerUp = false;
					}
				}else{
					if (pTAS2555->mpCalFirmware->mnCalibrations) {
						if(pTAS2555->mbLoadCalibrationPostPowerUp){
							dev_dbg(pTAS2555->dev, "Enable: load calibration\n");
							tas2555_load_block(pTAS2555, 
								&(pTAS2555->mpCalFirmware->mpCalibrations[pTAS2555->mnCurrentCalibration].mBlock));
							pTAS2555->mbLoadCalibrationPostPowerUp = false;
						}
					}
				}
			}
			dev_dbg(pTAS2555->dev, "Enable: load unmute sequence\n");
			tas2555_dev_load_data(pTAS2555, p_tas2555_unmute_data);
			pTAS2555->mbPowerUp = true;
		}
	} else {
		if (pTAS2555->mbPowerUp) {
			dev_dbg(pTAS2555->dev, "Enable: load shutdown sequence\n");
			tas2555_dev_load_data(pTAS2555, p_tas2555_shutdown_data);
			//tas2555_dev_load_data(pTAS2555, p_tas2555_shutdown_clk_err);
			pTAS2555->mbPowerUp = false;
		}
	}
}

int tas2555_set_bit_rate(struct tas2555_priv *pTAS2555, 
	enum channel chn, unsigned int nBitRate)
{
	int ret = 0, n = -1;
	
	dev_dbg(pTAS2555->dev, "tas2555_set_bit_rate: nBitRate = %d \n",
		nBitRate);
	switch(nBitRate){
		case 16:
			n = 0;
		break;
		case 20:
			n = 1;
		break;
		case 24:
			n = 2;
		break;
		case 32:
			n = 3;
		break;
	}
	
	if(n >= 0)
		ret = pTAS2555->update_bits(pTAS2555, chn,
			TAS2555_ASI1_DAC_FORMAT_REG, 0x18, n<<3);	
		
	return ret;	
}

int tas2555_get_bit_rate(struct tas2555_priv *pTAS2555, 
	enum channel chn, unsigned char *pBitRate)
{
	int ret = 0;
	unsigned int nValue = 0;
	unsigned char bitRate;
	
	ret = pTAS2555->read(pTAS2555, chn,
		TAS2555_ASI1_DAC_FORMAT_REG, &nValue);
	if(ret >=0){	
		bitRate = (nValue&0x18)>>3;
		if(bitRate == 0) bitRate = 16;
		else if(bitRate == 1) bitRate = 20;
		else if(bitRate == 2) bitRate = 24;
		else if(bitRate == 3) bitRate = 32;
		
		*pBitRate = bitRate;
	}else{
		dev_err(pTAS2555->dev, "read left channel error %d\n", ret);
	}
		
	return ret;	
}

int tas2555_get_DAC_gain(struct tas2555_priv *pTAS2555, enum channel chl, unsigned char *pnGain)
{
	int ret = 0;
	unsigned int nValue = 0;
	
	ret = pTAS2555->read(pTAS2555, chl,
			TAS2555_SPK_CTRL_REG, &nValue);
	if(ret >=0){
		*pnGain = 
			((nValue&TAS2555_DAC_GAIN_MASK)>>TAS2555_DAC_GAIN_SHIFT);
	}
		
	return ret;	
}

int tas2555_set_DAC_gain(struct tas2555_priv *pTAS2555, enum channel chl, unsigned int nGain)
{
	int ret = 0;
	
	ret = pTAS2555->update_bits(pTAS2555, chl,
			TAS2555_SPK_CTRL_REG, 
			TAS2555_DAC_GAIN_MASK, 
			(nGain<<TAS2555_DAC_GAIN_SHIFT));
		
	return ret;	
}

int tas2555_set_sampling_rate(struct tas2555_priv *pTAS2555, unsigned int nSamplingRate)
{
	TConfiguration *pConfiguration;
	unsigned int nConfiguration;

	dev_dbg(pTAS2555->dev, "tas2555_setup_clocks: nSamplingRate = %d [Hz]\n",
		nSamplingRate);

	if ((!pTAS2555->mpFirmware->mpPrograms) ||
		(!pTAS2555->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2555->dev, "Firmware not loaded\n");
		return -EINVAL;
	}

	pConfiguration = &(pTAS2555->mpFirmware->mpConfigurations[pTAS2555->mnCurrentConfiguration]);
	if (pConfiguration->mnSamplingRate == nSamplingRate) {
		dev_info(pTAS2555->dev, "Sampling rate for current configuration matches: %d\n",
			nSamplingRate);
		return 0;
	}

	for (nConfiguration = 0;
		nConfiguration < pTAS2555->mpFirmware->mnConfigurations;
		nConfiguration++) {
		pConfiguration =
			&(pTAS2555->mpFirmware->mpConfigurations[nConfiguration]);
		if ((pConfiguration->mnSamplingRate == nSamplingRate)
			&&(pConfiguration->mnProgram == pTAS2555->mnCurrentProgram)){
			dev_info(pTAS2555->dev,
				"Found configuration: %s, with compatible sampling rate %d\n",
				pConfiguration->mpName, nSamplingRate);
			tas2555_load_configuration(pTAS2555, nConfiguration, false);
			return 0;
		}
	}

	dev_err(pTAS2555->dev, "Cannot find a configuration that supports sampling rate: %d\n",
		nSamplingRate);

	return -EINVAL;
}

static void fw_print_header(struct tas2555_priv *pTAS2555, TFirmware * pFirmware)
{
	dev_info(pTAS2555->dev, "FW Size       = %d", pFirmware->mnFWSize);
	dev_info(pTAS2555->dev, "Checksum      = 0x%04X", pFirmware->mnChecksum);
	dev_info(pTAS2555->dev, "PPC Version   = 0x%04X", pFirmware->mnPPCVersion);
	dev_info(pTAS2555->dev, "DSPFWVersion  = 0x%04X", pFirmware->mnFWVersion);	
	if(pFirmware->mnPPCVersion >= PPC_WITH_DRIVER_VERSION)
	dev_info(pTAS2555->dev, "Driver Version= 0x%04X", pFirmware->mnDriverVersion);
	dev_info(pTAS2555->dev, "Timestamp     = %d", pFirmware->mnTimeStamp);
	dev_info(pTAS2555->dev, "DDC Name      = %s", pFirmware->mpDDCName);
	dev_info(pTAS2555->dev, "Description   = %s", pFirmware->mpDescription);
}

inline unsigned int fw_convert_number(unsigned char *pData)
{
	return pData[3] + (pData[2] << 8) + (pData[1] << 16) + (pData[0] << 24);
}

static int fw_parse_header(struct tas2555_priv *pTAS2555, 
	TFirmware * pFirmware, unsigned char *pData,
	unsigned int nSize)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned char pMagicNumber[] = { 0x35, 0x35, 0x35, 0x32 };
	if (nSize < 104) {
		dev_err(pTAS2555->dev, "Firmware: Header too short");
		return -1;
	}

	if (memcmp(pData, pMagicNumber, 4)) {
		dev_err(pTAS2555->dev, "Firmware: Magic number doesn't match");
		return -1;
	}

	pData += 4;

	pFirmware->mnFWSize = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnChecksum = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnPPCVersion = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnFWVersion = fw_convert_number(pData);
	pData += 4;

	if(pFirmware->mnPPCVersion >= PPC_WITH_DRIVER_VERSION){
		pFirmware->mnDriverVersion = fw_convert_number(pData);
		pData += 4;		
	}	

	pFirmware->mnTimeStamp = fw_convert_number(pData);
	pData += 4;

	memcpy(pFirmware->mpDDCName, pData, 64);
	pData += 64;

	n = strlen(pData);
	pFirmware->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
	pData += n + 1;

	if ((pData - pDataStart) >= nSize) {
		dev_err(pTAS2555->dev, "Firmware: Header too short after DDC description");
		return -1;
	}

	pFirmware->mnDeviceFamily = fw_convert_number(pData);
	pData += 4;

	pFirmware->mnDevice = fw_convert_number(pData);
	pData += 4;

	fw_print_header(pTAS2555, pFirmware);

	return pData - pDataStart;
}

static int fw_parse_block_data(struct tas2555_priv *pTAS2555,
	TBlock * pBlock, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;

	pBlock->mnType = fw_convert_number(pData);
	pData += 4;

	dev_dbg(pTAS2555->dev, "TBlock type[%d]\n", pBlock->mnType);
		
	pBlock->mnCommands = fw_convert_number(pData);
	pData += 4;

	n = pBlock->mnCommands * 4;
	pBlock->mpData = kmemdup(pData, n, GFP_KERNEL);
	pData += n;

	return pData - pDataStart;
}

static int fw_parse_data(struct tas2555_priv *pTAS2555, 
	TData * pImageData, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int nBlock;
	unsigned int n;

	memcpy(pImageData->mpName, pData, 64);
	pData += 64;

	n = strlen(pData);
	pImageData->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
	pData += n + 1;

	dev_dbg(pTAS2555->dev, "TData name=%s\n", pImageData->mpName);
	dev_dbg(pTAS2555->dev, "TData Desc=%s\n", pImageData->mpDescription);
		
	pImageData->mnBlocks = (pData[0] << 8) + pData[1];
	pData += 2;

	pImageData->mpBlocks =
		kmalloc(sizeof(TBlock) * pImageData->mnBlocks, GFP_KERNEL);

	for (nBlock = 0; nBlock < pImageData->mnBlocks; nBlock++) {
		n = fw_parse_block_data(pTAS2555,
			&(pImageData->mpBlocks[nBlock]), pData);
		pData += n;
	}

	return pData - pDataStart;
}

static int fw_parse_pll_data(struct tas2555_priv *pTAS2555, 
	TFirmware * pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nPLL;
	TPLL *pPLL;

	pFirmware->mnPLLs = (pData[0] << 8) + pData[1];
	pData += 2;

	pFirmware->mpPLLs = kmalloc(sizeof(TPLL) * pFirmware->mnPLLs, GFP_KERNEL);
	for (nPLL = 0; nPLL < pFirmware->mnPLLs; nPLL++) {
		pPLL = &(pFirmware->mpPLLs[nPLL]);

		memcpy(pPLL->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pPLL->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		dev_dbg(pTAS2555->dev, "PLL[%d] Name=%s\n", nPLL, pPLL->mpName);
		dev_dbg(pTAS2555->dev, "PLL[%d] Desc=%s\n", nPLL, pPLL->mpDescription);
		n = fw_parse_block_data(pTAS2555, &(pPLL->mBlock), pData);
		pData += n;
	}

	return pData - pDataStart;
}

static int fw_parse_program_data(struct tas2555_priv *pTAS2555,
	TFirmware * pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nProgram;
	TProgram *pProgram;

	pFirmware->mnPrograms = (pData[0] << 8) + pData[1];
	pData += 2;

	pFirmware->mpPrograms =
		kmalloc(sizeof(TProgram) * pFirmware->mnPrograms, GFP_KERNEL);
	for (nProgram = 0; nProgram < pFirmware->mnPrograms; nProgram++) {
		pProgram = &(pFirmware->mpPrograms[nProgram]);
		memcpy(pProgram->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pProgram->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		dev_dbg(pTAS2555->dev, "Program[%d] Name=%s\n", nProgram, pProgram->mpName);
		dev_dbg(pTAS2555->dev, "Program[%d] Desc=%s\n", nProgram, pProgram->mpDescription);
		
		n = fw_parse_data(pTAS2555, &(pProgram->mData), pData);
		pData += n;
	}

	return pData - pDataStart;
}

static int fw_parse_configuration_data(struct tas2555_priv *pTAS2555,
	TFirmware * pFirmware,
	unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nConfiguration;
	TConfiguration *pConfiguration;

	pFirmware->mnConfigurations = (pData[0] << 8) + pData[1];
	pData += 2;

	pFirmware->mpConfigurations =
		kmalloc(sizeof(TConfiguration) * pFirmware->mnConfigurations,
		GFP_KERNEL);
	for (nConfiguration = 0; nConfiguration < pFirmware->mnConfigurations;
		nConfiguration++) {
		pConfiguration = &(pFirmware->mpConfigurations[nConfiguration]);
		memcpy(pConfiguration->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pConfiguration->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		pConfiguration->mnProgram = pData[0];
		pData++;
		
		dev_dbg(pTAS2555->dev, "Configuration[%d] Name=%s\n", nConfiguration, pConfiguration->mpName);
		dev_dbg(pTAS2555->dev, "Configuration[%d] Desc=%s\n", nConfiguration, pConfiguration->mpDescription);

		pConfiguration->mnPLL = pData[0];
		pData++;

		pConfiguration->mnSamplingRate = fw_convert_number(pData);
		pData += 4;

		n = fw_parse_data(pTAS2555, &(pConfiguration->mData), pData);
		pData += n;
	}

	return pData - pDataStart;
}

int fw_parse_calibration_data(struct tas2555_priv *pTAS2555,
	TFirmware * pFirmware, unsigned char *pData)
{
	unsigned char *pDataStart = pData;
	unsigned int n;
	unsigned int nCalibration;
	TCalibration *pCalibration;

	pFirmware->mnCalibrations = (pData[0] << 8) + pData[1];
	pData += 2;

	pFirmware->mpCalibrations =
		kmalloc(sizeof(TCalibration) * pFirmware->mnCalibrations, GFP_KERNEL);
	for (nCalibration = 0;
		nCalibration < pFirmware->mnCalibrations;
		nCalibration++) {
		pCalibration = &(pFirmware->mpCalibrations[nCalibration]);
		memcpy(pCalibration->mpName, pData, 64);
		pData += 64;

		n = strlen(pData);
		pCalibration->mpDescription = kmemdup(pData, n + 1, GFP_KERNEL);
		pData += n + 1;

		pCalibration->mnProgram = pData[0];
		pData++;

		pCalibration->mnConfiguration = pData[0];
		pData++;

		n = fw_parse_block_data(pTAS2555, &(pCalibration->mBlock), pData);
		pData += n;
	}

	return pData - pDataStart;
}

static int fw_parse(struct tas2555_priv *pTAS2555,
	TFirmware * pFirmware,
	unsigned char *pData,
	unsigned int nSize)
{
	int nPosition = 0;

	nPosition = fw_parse_header(pTAS2555, pFirmware, pData, nSize);
	if (nPosition < 0) {
		dev_err(pTAS2555->dev, "Firmware: Wrong Header");
		return FW_ERR_HEADER;
	}

	if (nPosition >= nSize) {
		dev_err(pTAS2555->dev, "Firmware: Too short");
		return FW_ERR_SIZE;
	}

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	nPosition = fw_parse_pll_data(pTAS2555, pFirmware, pData);

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	nPosition = fw_parse_program_data(pTAS2555, pFirmware, pData);

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	nPosition = fw_parse_configuration_data(pTAS2555, pFirmware, pData);

	pData += nPosition;
	nSize -= nPosition;
	nPosition = 0;

	if (nSize > 64)
		nPosition = fw_parse_calibration_data(pTAS2555, pFirmware, pData);

	return 0;
}

static void tas2555_load_block(struct tas2555_priv *pTAS2555, TBlock * pBlock)
{
	unsigned int nCommand = 0;
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nOffset;
	unsigned char nData;
	unsigned int nLength;
	enum channel chl;
	unsigned char *pData = pBlock->mpData;

	dev_dbg(pTAS2555->dev, "TAS2555 load block: Type = %d, commands = %d\n",
		pBlock->mnType, pBlock->mnCommands);
	
	if((pBlock->mnType == TAS2555_BLOCK_PLL)
		||(pBlock->mnType == TAS2555_BLOCK_POST)
		||(pBlock->mnType == TAS2555_BLOCK_POST_POWER_UP)){
		chl = channel_both;
	}else if((pBlock->mnType == TAS2555_BLOCK_MAIN_DEV_A)
		||(pBlock->mnType == TAS2555_BLOCK_COEFF_DEV_A)
		||(pBlock->mnType == TAS2555_BLOCK_PRE_DEV_A)){
		chl = channel_left;
	}else if((pBlock->mnType == TAS2555_BLOCK_MAIN_DEV_B)
		||(pBlock->mnType == TAS2555_BLOCK_COEFF_DEV_B)
		||(pBlock->mnType == TAS2555_BLOCK_PRE_DEV_B)){
		chl = channel_right;		
	}else{
		dev_err(pTAS2555->dev, "block type error %d\n", pBlock->mnType);
		return;
	}
			
	while (nCommand < pBlock->mnCommands) {
		pData = pBlock->mpData + nCommand * 4;

		nBook = pData[0];
		nPage = pData[1];
		nOffset = pData[2];
		nData = pData[3];

		nCommand++;

		if (nOffset <= 0x7F){
			pTAS2555->write(pTAS2555, 
							chl, 
							TAS2555_REG(nBook, nPage, nOffset),
							nData);
		}else if (nOffset == 0x81) {
			unsigned int nSleep = (nBook << 8) + nPage;
			msleep(nSleep);
		}else if (nOffset == 0x85) {
			pData += 4;
			nLength = (nBook << 8) + nPage;
			nBook = pData[0];
			nPage = pData[1];
			nOffset = pData[2];
			if (nLength > 1)
				pTAS2555->bulk_write(pTAS2555, 
									chl, 
									TAS2555_REG(nBook, nPage, nOffset), 
									pData + 3, 
									nLength);
			else
				pTAS2555->write(pTAS2555, 
								chl, 
								TAS2555_REG(nBook, nPage, nOffset),
								pData[3]);

			nCommand++;
			if (nLength >= 2)
				nCommand += ((nLength - 2) / 4) + 1;
		}
	}
}

static void tas2555_load_data(struct tas2555_priv *pTAS2555, TData * pData,
	unsigned int nType)
{
	unsigned int nBlock;
	TBlock *pBlock;

	dev_dbg(pTAS2555->dev,
		"TAS2555 load data: %s, Blocks = %d, Block Type = %d\n", pData->mpName,
		pData->mnBlocks, nType);

	for (nBlock = 0; nBlock < pData->mnBlocks; nBlock++) {
		pBlock = &(pData->mpBlocks[nBlock]);
		if (pBlock->mnType == nType)
			tas2555_load_block(pTAS2555, pBlock);
	}
}

static void tas2555_load_configuration(struct tas2555_priv *pTAS2555,
	unsigned int nConfiguration, bool bLoadSame)
{
	TConfiguration *pCurrentConfiguration;
	TConfiguration *pNewConfiguration;
	TPLL *pNewPLL;

	dev_dbg(pTAS2555->dev, "tas2555_load_configuration: %d\n", nConfiguration);

	if ((!pTAS2555->mpFirmware->mpPrograms) ||
		(!pTAS2555->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2555->dev, "Firmware not loaded\n");
		return;
	}

	if (nConfiguration >= pTAS2555->mpFirmware->mnConfigurations) {
		dev_err(pTAS2555->dev, "Configuration %d doesn't exist\n",
			nConfiguration);
		return;
	}

	if ((nConfiguration == pTAS2555->mnCurrentConfiguration) && (!bLoadSame)) {
		dev_info(pTAS2555->dev, "Configuration %d is already loaded\n",
			nConfiguration);
		return;
	}

	pCurrentConfiguration =
		&(pTAS2555->mpFirmware->mpConfigurations[pTAS2555->mnCurrentConfiguration]);
	pNewConfiguration =
		&(pTAS2555->mpFirmware->mpConfigurations[nConfiguration]);

	if (pNewConfiguration->mnProgram != pCurrentConfiguration->mnProgram) {
		dev_err(pTAS2555->dev,
			"Configuration %d, %s doesn't share the same program as current %d\n",
			nConfiguration, pNewConfiguration->mpName, pCurrentConfiguration->mnProgram);
		return;
	}

	if (pNewConfiguration->mnPLL >= pTAS2555->mpFirmware->mnPLLs) {
		dev_err(pTAS2555->dev,
			"Configuration %d, %s doesn't have a valid PLL index %d\n",
			nConfiguration, pNewConfiguration->mpName, pNewConfiguration->mnPLL);
		return;
	}
	
	pNewPLL = &(pTAS2555->mpFirmware->mpPLLs[pNewConfiguration->mnPLL]);

	if (pTAS2555->mbPowerUp) {
		if (pNewConfiguration->mnPLL != pCurrentConfiguration->mnPLL) {
			dev_dbg(pTAS2555->dev,
				"TAS2555 is powered up -> mute and power down DSP before loading new configuration\n");
			//tas2555_dev_load_data(pTAS2555, p_tas2555_mute_DSP_down_data);
			tas2555_dev_load_data(pTAS2555, p_tas2555_shutdown_data);

			dev_dbg(pTAS2555->dev,
				"load post block from current configuration: %s, before loading new configuration: %s\n",
				pCurrentConfiguration->mpName, pNewConfiguration->mpName);
			tas2555_load_data(pTAS2555, &(pCurrentConfiguration->mData),
				TAS2555_BLOCK_POST);
			dev_dbg(pTAS2555->dev, "TAS2555: load new PLL: %s, block data\n",
				pNewPLL->mpName);
			tas2555_load_block(pTAS2555, &(pNewPLL->mBlock));
			pTAS2555->mnCurrentSampleRate = pNewConfiguration->mnSamplingRate;
			dev_dbg(pTAS2555->dev,
				"load new configuration: %s, pre block data\n",
				pNewConfiguration->mpName);
			tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_PRE_DEV_A);
			tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_PRE_DEV_B);
			dev_dbg(pTAS2555->dev, "TAS2555: power up TAS2555\n");
			tas2555_dev_load_data(pTAS2555, p_tas2555_startup_data);
			dev_dbg(pTAS2555->dev,
				"TAS2555: load new configuration: %s, post power up block data\n",
				pNewConfiguration->mpName);
			tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_POST_POWER_UP);
			dev_dbg(pTAS2555->dev,
				"TAS2555: load new configuration: %s, coeff block data\n",
				pNewConfiguration->mpName);
			tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_COEFF_DEV_A);
				tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_COEFF_DEV_B);
			dev_dbg(pTAS2555->dev, "TAS2555: unmute TAS2555\n");
			tas2555_dev_load_data(pTAS2555, p_tas2555_unmute_data);
		} else {
			dev_dbg(pTAS2555->dev,
				"TAS2555 is powered up, no change in PLL: load new configuration: %s, coeff block data\n",
				pNewConfiguration->mpName);
			tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_COEFF_DEV_A);
			tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_COEFF_DEV_B);					
		}
		
		if (pTAS2555->mpCalFirmware->mnCalibrations) {
				dev_dbg(pTAS2555->dev, "Enable: load calibration\n");
				tas2555_load_block(pTAS2555, 
					&(pTAS2555->mpCalFirmware->mpCalibrations[pTAS2555->mnCurrentCalibration].mBlock));
				pTAS2555->mbLoadCalibrationPostPowerUp = false;
		}
		
		pTAS2555->mbLoadConfigurationPostPowerUp = false;
	} else {
		dev_dbg(pTAS2555->dev,
			"TAS2555 was powered down -> set flag to load configuration data when OS powers up the TAS2555 the next time\n");
		if (pNewConfiguration->mnPLL != pCurrentConfiguration->mnPLL) {
			dev_dbg(pTAS2555->dev,
				"load post block from current configuration: %s, before loading new configuration: %s\n",
				pCurrentConfiguration->mpName, pNewConfiguration->mpName);
			tas2555_load_data(pTAS2555, &(pCurrentConfiguration->mData),
				TAS2555_BLOCK_POST);
			dev_dbg(pTAS2555->dev, "TAS2555: load new PLL: %s, block data\n",
				pNewPLL->mpName);
			tas2555_load_block(pTAS2555, &(pNewPLL->mBlock));
			pTAS2555->mnCurrentSampleRate = pNewConfiguration->mnSamplingRate;
			dev_dbg(pTAS2555->dev,
				"load new configuration: %s, pre block data\n",
				pNewConfiguration->mpName);
			tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_PRE_DEV_A);
			tas2555_load_data(pTAS2555, &(pNewConfiguration->mData),
				TAS2555_BLOCK_PRE_DEV_B);
		}
		
		pTAS2555->mbLoadConfigurationPostPowerUp = true;
	}

	pTAS2555->mnCurrentConfiguration = nConfiguration;
}

int tas2555_set_config(struct tas2555_priv *pTAS2555, int config)
{
	TConfiguration *pConfiguration;
	TProgram *pProgram;
	unsigned int nProgram = pTAS2555->mnCurrentProgram;
	unsigned int nConfiguration = config;

	if ((!pTAS2555->mpFirmware->mpPrograms) ||
		(!pTAS2555->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2555->dev, "Firmware not loaded\n");
		return -1;
	}

	if (nConfiguration >= pTAS2555->mpFirmware->mnConfigurations) {
		dev_err(pTAS2555->dev, "Configuration %d doesn't exist\n",
			nConfiguration);
		return -1;
	}

	pConfiguration = &(pTAS2555->mpFirmware->mpConfigurations[nConfiguration]);
	pProgram = &(pTAS2555->mpFirmware->mpPrograms[nProgram]);

	if (nProgram != pConfiguration->mnProgram) {
		dev_err(pTAS2555->dev,
			"Configuration %d, %s with Program %d isn't compatible with existing Program %d, %s\n",
			nConfiguration, pConfiguration->mpName, pConfiguration->mnProgram,
			nProgram, pProgram->mpName);
		return -1;
	}

	tas2555_load_configuration(pTAS2555, nConfiguration, false);

	return 0;
}

void tas2555_clear_firmware(TFirmware *pFirmware)
{
	unsigned int n, nn;
	if (!pFirmware) return;
	if (pFirmware->mpDescription) kfree(pFirmware->mpDescription);	

	for (n = 0; n < pFirmware->mnPLLs; n++)
	{
		kfree(pFirmware->mpPLLs[n].mpDescription);
		kfree(pFirmware->mpPLLs[n].mBlock.mpData);
	}
	kfree(pFirmware->mpPLLs);

	for (n = 0; n < pFirmware->mnPrograms; n++)
	{
		kfree(pFirmware->mpPrograms[n].mpDescription);
		kfree(pFirmware->mpPrograms[n].mData.mpDescription);
		for (nn = 0; nn < pFirmware->mpPrograms[n].mData.mnBlocks; nn++)
			kfree(pFirmware->mpPrograms[n].mData.mpBlocks[nn].mpData);
		kfree(pFirmware->mpPrograms[n].mData.mpBlocks);
	}
	kfree(pFirmware->mpPrograms);

	for (n = 0; n < pFirmware->mnConfigurations; n++)
	{
		kfree(pFirmware->mpConfigurations[n].mpDescription);
		kfree(pFirmware->mpConfigurations[n].mData.mpDescription);
		for (nn = 0; nn < pFirmware->mpConfigurations[n].mData.mnBlocks; nn++)
			kfree(pFirmware->mpConfigurations[n].mData.mpBlocks[nn].mpData);
		kfree(pFirmware->mpConfigurations[n].mData.mpBlocks);
	}
	kfree(pFirmware->mpConfigurations);

	for (n = 0; n < pFirmware->mnCalibrations; n++)
	{
		kfree(pFirmware->mpCalibrations[n].mpDescription);
		kfree(pFirmware->mpCalibrations[n].mBlock.mpData);
	}
	kfree(pFirmware->mpCalibrations);

	memset(pFirmware, 0x00, sizeof(TFirmware));
}

static void tas2555_load_calibration(struct tas2555_priv *pTAS2555,
	char *pFileName)
{
	int nResult;
	int nFile;
	mm_segment_t fs;
	unsigned char pBuffer[512];
	int nSize = 0;

	dev_dbg(pTAS2555->dev, "%s:\n", __func__);

	fs = get_fs();
	set_fs(KERNEL_DS);
	nFile = sys_open(pFileName, O_RDONLY, 0);

	dev_info(pTAS2555->dev, "TAS2555 calibration file = %s, handle = %d\n",
		pFileName, nFile);

	if (nFile >= 0) {
		nSize = sys_read(nFile, pBuffer, 512);
		sys_close(nFile);
	} else {
		dev_err(pTAS2555->dev, "TAS2555 cannot open calibration file: %s\n",
			pFileName);
	}

	set_fs(fs);

	if (!nSize)
		return;

	tas2555_clear_firmware(pTAS2555->mpCalFirmware);
		
	dev_info(pTAS2555->dev, "TAS2555 calibration file size = %d\n", nSize);
	nResult = fw_parse(pTAS2555, pTAS2555->mpCalFirmware, pBuffer, nSize);

	if (nResult) {
		dev_err(pTAS2555->dev, "TAS2555 calibration file is corrupt\n");
		return;
	}

	dev_info(pTAS2555->dev, "TAS2555 calibration: %d calibrations\n",
		pTAS2555->mpCalFirmware->mnCalibrations);
}

void tas2555_fw_ready(const struct firmware *pFW, void *pContext)
{
	struct tas2555_priv *pTAS2555 = (struct tas2555_priv *) pContext;
	int nResult;
	unsigned int nProgram = 0;
	unsigned int nSampleRate = 0;

	dev_info(pTAS2555->dev, "%s:\n", __func__);

	if (unlikely(!pFW) || unlikely(!pFW->data)) {
		dev_err(pTAS2555->dev, "%s firmware is not loaded.\n",
			TAS2555_FW_NAME);
		return;
	}

	if (pTAS2555->mpFirmware->mpConfigurations){
		nProgram = pTAS2555->mnCurrentProgram;
		nSampleRate = pTAS2555->mnCurrentSampleRate;
		dev_dbg(pTAS2555->dev, "clear current firmware\n");
		tas2555_clear_firmware(pTAS2555->mpFirmware);
	}	
		
	nResult = fw_parse(pTAS2555, pTAS2555->mpFirmware, 
		(unsigned char *) (pFW->data),	pFW->size);

	release_firmware(pFW);
	
	if (nResult) {
		dev_err(pTAS2555->dev, "firmware is corrupt\n");
		return;
	}

	if (!pTAS2555->mpFirmware->mnPrograms) {
		dev_err(pTAS2555->dev, "firmware contains no programs\n");
		return;
	}
	
	if (!pTAS2555->mpFirmware->mnConfigurations) {
		dev_err(pTAS2555->dev, 
			"firmware contains no configurations\n");
		return;
	}
	
	if(nProgram >= pTAS2555->mpFirmware->mnPrograms){
		dev_info(pTAS2555->dev, 
			"no previous program, set to default\n");
		nProgram = 0;
	}
		
	pTAS2555->mnCurrentSampleRate = nSampleRate;

	tas2555_set_program(pTAS2555, nProgram);
}	

int tas2555_set_program(struct tas2555_priv *pTAS2555,
	unsigned int nProgram)
{
	TPLL *pPLL;
	TConfiguration *pConfiguration;
	unsigned int nConfiguration = 0;
	unsigned int nSampleRate = 0;
	unsigned int Value = 0;
	bool bFound = false;
	int nResult = -1;

	if ((!pTAS2555->mpFirmware->mpPrograms) ||
		(!pTAS2555->mpFirmware->mpConfigurations)) {
		dev_err(pTAS2555->dev, "Firmware not loaded\n");
		return -1;
	}
	
	if (nProgram >= pTAS2555->mpFirmware->mnPrograms) {
		dev_err(pTAS2555->dev, "TAS2555: Program %d doesn't exist\n",
			nConfiguration);
		return -1;
	}
	
	nConfiguration = 0;
	nSampleRate = pTAS2555->mnCurrentSampleRate;
	
	while (!bFound 
		&& (nConfiguration < pTAS2555->mpFirmware->mnConfigurations)) {
		if (pTAS2555->mpFirmware->mpConfigurations[nConfiguration].mnProgram 
			== nProgram){
			if(nSampleRate == 0){
				bFound = true;
				dev_info(pTAS2555->dev, "find default configuration %d\n", nConfiguration);
			}else if(nSampleRate 
				== pTAS2555->mpFirmware->mpConfigurations[nConfiguration].mnSamplingRate){
				bFound = true;
				dev_info(pTAS2555->dev, "find matching configuration %d\n", nConfiguration);
			}else{
				nConfiguration++;
			}
		}else{
			nConfiguration++;
		}
	}
	
	if (!bFound) {
		dev_err(pTAS2555->dev, 
			"Program %d, no valid configuration found for sample rate %d, ignore\n",
			nProgram, nSampleRate);
		return -1;
	}
	
	pTAS2555->mnCurrentProgram = nProgram;
	if(pTAS2555->mbPowerUp)
		tas2555_dev_load_data(pTAS2555, p_tas2555_mute_DSP_down_data);
	pTAS2555->write(pTAS2555, channel_both, TAS2555_SW_RESET_REG, 0x01);
	udelay(1000);
	
	dev_info(pTAS2555->dev, "load program %d\n", nProgram);
	tas2555_load_data(pTAS2555,
		&(pTAS2555->mpFirmware->mpPrograms[nProgram].mData),
		TAS2555_BLOCK_MAIN_DEV_A);
	tas2555_load_data(pTAS2555,
		&(pTAS2555->mpFirmware->mpPrograms[nProgram].mData),
		TAS2555_BLOCK_MAIN_DEV_B);

	pTAS2555->mnCurrentConfiguration = nConfiguration;

	pConfiguration =
		&(pTAS2555->mpFirmware->mpConfigurations[nConfiguration]);
	pPLL = &(pTAS2555->mpFirmware->mpPLLs[pConfiguration->mnPLL]);
	dev_dbg(pTAS2555->dev,
		"TAS2555 load PLL: %s block for Configuration %s\n",
		pPLL->mpName, pConfiguration->mpName);
	
	tas2555_load_block(pTAS2555, &(pPLL->mBlock));
	pTAS2555->mnCurrentSampleRate = pConfiguration->mnSamplingRate;
	dev_dbg(pTAS2555->dev,
		"load configuration %s conefficient pre block\n",
		pConfiguration->mpName);		
	tas2555_load_data(pTAS2555, &(pConfiguration->mData), TAS2555_BLOCK_PRE_DEV_A);
	tas2555_load_data(pTAS2555, &(pConfiguration->mData), TAS2555_BLOCK_PRE_DEV_B);

	nResult = pTAS2555->read(pTAS2555, channel_left, TAS2555_CRC_CHECKSUM_REG, &Value);
	dev_info(pTAS2555->dev, "Left uCDSP Checksum: 0x%02x\n", Value);
	nResult = pTAS2555->read(pTAS2555, channel_right, TAS2555_CRC_CHECKSUM_REG, &Value);
	dev_info(pTAS2555->dev, "Right uCDSP Checksum: 0x%02x\n", Value);	
	nResult = pTAS2555->read(pTAS2555, channel_left, TAS2555_PLL_CLKIN_REG, &Value);
	dev_info(pTAS2555->dev, "TAS2555 PLL_CLKIN = 0x%02X\n", Value);
	p_tas2555_startup_data[TAS2555_STARTUP_DATA_PLL_CLKIN_INDEX] = Value;

	if (pTAS2555->mbPowerUp){
		dev_dbg(pTAS2555->dev, "device powered up, load startup\n");
		tas2555_dev_load_data(pTAS2555, p_tas2555_startup_data);
		dev_dbg(pTAS2555->dev, 
			"device powered up, load configuration %s post power block\n",
			pConfiguration->mpName);
		tas2555_load_data(pTAS2555, &(pConfiguration->mData),
			TAS2555_BLOCK_POST_POWER_UP);
	}
	
	tas2555_load_configuration(pTAS2555, nConfiguration, true);
	if (pTAS2555->mbPowerUp){
		dev_dbg(pTAS2555->dev,
			"device powered up, load unmute\n");
		tas2555_dev_load_data(pTAS2555, p_tas2555_unmute_data);
	}

	return 0;
}

int tas2555_set_calibration(struct tas2555_priv *pTAS2555,
	int nCalibration)
{
	if ((!pTAS2555->mpFirmware->mpPrograms) || (!pTAS2555->mpFirmware->mpConfigurations)) 
	{
		dev_err(pTAS2555->dev, "Firmware not loaded\n\r");
		return -1;
	}

	if (nCalibration == 0x00FF)
	{
		dev_info(pTAS2555->dev, "load new calibration file %s\n", TAS2555_CAL_NAME); 	
		tas2555_load_calibration(pTAS2555, TAS2555_CAL_NAME);
		nCalibration = 0;
	}

	if (nCalibration >= pTAS2555->mpFirmware->mnCalibrations) {
		dev_err(pTAS2555->dev,
			"Calibration %d doesn't exist\n", nCalibration);
		return -1;
	}

	pTAS2555->mnCurrentCalibration = nCalibration;
	if(pTAS2555->mbPowerUp){
		tas2555_load_block(pTAS2555, 
			&(pTAS2555->mpCalFirmware->mpCalibrations[pTAS2555->mnCurrentCalibration].mBlock));
		pTAS2555->mbLoadCalibrationPostPowerUp = false; 
	}else{
		pTAS2555->mbLoadCalibrationPostPowerUp = true; 
	}

	return 0;
}

int tas2555_parse_dt(struct device *dev,
			struct tas2555_priv *pTAS2555)
{
	struct device_node *np = dev->of_node;
	int rc= 0, ret = 0;
	unsigned int value;
	
	pTAS2555->mnResetGPIO = of_get_named_gpio(np, "ti,cdc-reset-gpio", 0);
	if (pTAS2555->mnResetGPIO < 0) {
		dev_err(pTAS2555->dev, "Looking up %s property in node %s failed %d\n",
			"ti,cdc-reset-gpio", np->full_name,
			pTAS2555->mnResetGPIO);
		ret = -1;
	}else{
		dev_dbg(pTAS2555->dev, "ti,cdc-reset-gpio=%d\n", pTAS2555->mnResetGPIO);
	}
	
	if(ret >=0){
		rc = of_property_read_u32(np, "ti,left-channel", &value);
		if (rc) {
			dev_err(pTAS2555->dev, "Looking up %s property in node %s failed %d\n",
				"ti,left-channel", np->full_name, rc);
			ret = -2;
		}else{
			pTAS2555->mnLAddr = value;
			dev_dbg(pTAS2555->dev, "ti,left-channel=0x%x\n", pTAS2555->mnLAddr);
		}
	}
	
	if(ret >=0){
		rc = of_property_read_u32(np, "ti,right-channel", &value);
		if (rc) {
			dev_err(pTAS2555->dev, "Looking up %s property in node %s failed %d\n",
				"ti,right-channel", np->full_name, rc);
			ret = -3;
		}else{
			pTAS2555->mnRAddr = value;
			dev_dbg(pTAS2555->dev, "ti,right-channel=0x%x", pTAS2555->mnRAddr);
		}
	}
	
	return ret;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2555 common functions for Android Linux");
MODULE_LICENSE("GPLv2");