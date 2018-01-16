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
**     tas2555-regmap.c
**
** Description:
**     I2C driver with regmap for Texas Instruments TAS2555 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2555_I2C_STEREO

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

#ifdef CONFIG_TAS2555_CODEC_STEREO
#include "tas2555-codec.h"
#endif

#ifdef CONFIG_TAS2555_MISC_STEREO
#include "tas2555-misc.h"
#endif

#define ENABLE_TILOAD			//only enable this for in-system tuning or debug, not for production systems
#ifdef ENABLE_TILOAD
#include "tiload.h"
#endif

/*
* tas2555_i2c_write_device : write single byte to device
* platform dependent, need platform specific support
*/
static int tas2555_i2c_write_device(
	struct tas2555_priv *pTAS2555,
	unsigned char addr, 
	unsigned char reg, 
	unsigned char value)
{
	int ret = 0;
	
	pTAS2555->client->addr = addr;
	ret = regmap_write(pTAS2555->mpRegmap, reg, value);
	if(ret < 0){
		dev_err(pTAS2555->dev, "%s[0x%x] Error %d\n",
			__FUNCTION__, addr, ret);
	}else{
		ret = 1;
	}
	
	return ret;
}

/*
* tas2555_i2c_bulkwrite_device : write multiple bytes to device
* platform dependent, need platform specific support
*/
static int tas2555_i2c_bulkwrite_device(
	struct tas2555_priv *pTAS2555,
	unsigned char addr, 
	unsigned char reg, 
	unsigned char *pBuf,
	unsigned int len)
{
	int ret = 0;
	
	pTAS2555->client->addr = addr;
	ret = regmap_bulk_write(pTAS2555->mpRegmap, reg, pBuf, len);
	if(ret < 0){
		dev_err(pTAS2555->dev, "%s[0x%x] Error %d\n",
			__FUNCTION__, addr, ret);
	}else{
		ret = len;
	}
	return ret;
}

/*
* tas2555_i2c_read_device : read single byte from device
* platform dependent, need platform specific support
*/
static int tas2555_i2c_read_device(	
	struct tas2555_priv *pTAS2555,
	unsigned char addr, 
	unsigned char reg, 
	unsigned char *p_value)
{
	int ret = 0;
	unsigned int val = 0;
	
	pTAS2555->client->addr = addr;
	ret = regmap_read(pTAS2555->mpRegmap, reg, &val);
	if(ret < 0){
		dev_err(pTAS2555->dev, "%s[0x%x] Error %d\n",
			__FUNCTION__, addr, ret);
	}else{
		*p_value = (unsigned char)val;
		ret = 1;
	}
	
	return ret;
}

/*
* tas2555_i2c_bulkread_device : read multiple bytes from device
* platform dependent, need platform specific support
*/
static int tas2555_i2c_bulkread_device(	
	struct tas2555_priv *pTAS2555,
	unsigned char addr, 
	unsigned char reg, 
	unsigned char *p_value,
	unsigned int len)
{
	int ret = 0;
	pTAS2555->client->addr = addr;
	ret = regmap_bulk_read(pTAS2555->mpRegmap, reg, p_value, len);
	
	if(ret < 0){
		dev_err(pTAS2555->dev, "%s[0x%x] Error %d\n",
			__FUNCTION__, addr, ret);
	}else{
		ret = len;
	}
	
	return ret;
}

static int tas2555_i2c_update_bits(	
	struct tas2555_priv *pTAS2555,
	unsigned char addr, 
	unsigned char reg, 
	unsigned char mask,
	unsigned char value)
{
	int ret = 0;
	
	pTAS2555->client->addr = addr;
	ret = regmap_update_bits(pTAS2555->mpRegmap, reg, mask, value);
	
	if(ret < 0){
		dev_err(pTAS2555->dev, "%s[0x%x] Error %d\n",
			__FUNCTION__, addr, ret);
	}else{
		ret = 1;
	}
	
	return ret;
}

/* 
* tas2555_change_book_page : switch to certain book and page
* platform independent, don't change unless necessary
*/
static int tas2555_change_book_page(
	struct tas2555_priv *pTAS2555, 
	enum channel chn,
	int nBook,
	int nPage)
{
	int nResult = 0;
	
	if(chn&channel_left){
		if(pTAS2555->mnLCurrentBook == nBook){
			if(pTAS2555->mnLCurrentPage != nPage){
				nResult = tas2555_i2c_write_device(pTAS2555, pTAS2555->mnLAddr, TAS2555_BOOKCTL_PAGE, nPage);
				if(nResult >=0 )
					pTAS2555->mnLCurrentPage = nPage;
			}
		}else{
			nResult = tas2555_i2c_write_device(pTAS2555, pTAS2555->mnLAddr, TAS2555_BOOKCTL_PAGE, 0);
			if(nResult >=0){
				pTAS2555->mnLCurrentPage = 0;
				nResult = tas2555_i2c_write_device(pTAS2555, pTAS2555->mnLAddr, TAS2555_BOOKCTL_REG, nBook);
				pTAS2555->mnLCurrentBook = nBook;
				if(nPage != 0){
					tas2555_i2c_write_device(pTAS2555, pTAS2555->mnLAddr, TAS2555_BOOKCTL_PAGE, nPage);
					pTAS2555->mnLCurrentPage = nPage;
				}
			}
		}
	}

	if(chn&channel_right){
		if(pTAS2555->mnRCurrentBook == nBook){
			if(pTAS2555->mnRCurrentPage != nPage){
				nResult = tas2555_i2c_write_device(pTAS2555, pTAS2555->mnRAddr, TAS2555_BOOKCTL_PAGE, nPage);
				if(nResult >=0 )
					pTAS2555->mnRCurrentPage = nPage;
			}
		}else{
			nResult = tas2555_i2c_write_device(pTAS2555, pTAS2555->mnRAddr, TAS2555_BOOKCTL_PAGE, 0);
			if(nResult >=0){
				pTAS2555->mnRCurrentPage = 0;
				nResult = tas2555_i2c_write_device(pTAS2555, pTAS2555->mnRAddr, TAS2555_BOOKCTL_REG, nBook);
				pTAS2555->mnRCurrentBook = nBook;
				if(nPage != 0){
					tas2555_i2c_write_device(pTAS2555, pTAS2555->mnRAddr, TAS2555_BOOKCTL_PAGE, nPage);
					pTAS2555->mnRCurrentPage = nPage;
				}
			}
		}
	}	
	return nResult;
}

/* 
* tas2555_dev_read :
* platform independent, don't change unless necessary
*/
static int tas2555_dev_read(
	struct tas2555_priv *pTAS2555,
	enum channel chn,
	unsigned int nRegister, 
	unsigned int *pValue)
{
	int nResult = 0;
	unsigned char Value = 0;

	mutex_lock(&pTAS2555->dev_lock);	
	
	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)){
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;			// let only reads from TILoad pass.
		}
		nRegister &= ~0x80000000;
		
		dev_dbg(pTAS2555->dev, "TiLoad R CH[%d] REG B[%d]P[%d]R[%d]\n", 
				chn,
				TAS2555_BOOK_ID(nRegister),
				TAS2555_PAGE_ID(nRegister),
				TAS2555_PAGE_REG(nRegister));
	}

	nResult = tas2555_change_book_page(pTAS2555, 
							chn, 
							TAS2555_BOOK_ID(nRegister),
							TAS2555_PAGE_ID(nRegister));
	if(nResult >=0){
		if(chn == channel_left){
			nResult = tas2555_i2c_read_device(pTAS2555, pTAS2555->mnLAddr, TAS2555_PAGE_REG(nRegister), &Value);
		}else if(chn == channel_right){
			nResult = tas2555_i2c_read_device(pTAS2555, pTAS2555->mnRAddr, TAS2555_PAGE_REG(nRegister), &Value);
		}else{
			dev_err(pTAS2555->dev, "read chn ERROR %d\n", chn);
			nResult = -1;
		}
		
		if(nResult>=0) *pValue = Value;
	}
	mutex_unlock(&pTAS2555->dev_lock);
	return nResult;
}

/* 
* tas2555_dev_write :
* platform independent, don't change unless necessary
*/
static int tas2555_dev_write(
	struct tas2555_priv *pTAS2555,
	enum channel chn,
	unsigned int nRegister, 
	unsigned int nValue)
{
	int nResult = 0;
	
	mutex_lock(&pTAS2555->dev_lock);
	if ((nRegister == 0xAFFEAFFE) && (nValue == 0xBABEBABE)) {
		pTAS2555->mbTILoadActive = true;
		mutex_unlock(&pTAS2555->dev_lock);
		
		dev_dbg(pTAS2555->dev, "TiLoad Active\n");
		return 0;
	}

	if ((nRegister == 0xBABEBABE) && (nValue == 0xAFFEAFFE)) {
		pTAS2555->mbTILoadActive = false;
		mutex_unlock(&pTAS2555->dev_lock);
		
		dev_dbg(pTAS2555->dev, "TiLoad DeActive\n");
		return 0;
	}

	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)){
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;			// let only writes from TILoad pass.
		}
		nRegister &= ~0x80000000;
		
		dev_dbg(pTAS2555->dev, "TiLoad W CH[%d] REG B[%d]P[%d]R[%d] =0x%x\n", 
						chn,
						TAS2555_BOOK_ID(nRegister),
						TAS2555_PAGE_ID(nRegister),
						TAS2555_PAGE_REG(nRegister),
						nValue);
	}

	nResult = tas2555_change_book_page(pTAS2555, 
							chn, 
							TAS2555_BOOK_ID(nRegister),
							TAS2555_PAGE_ID(nRegister));

	if(nResult >= 0){
		if(chn & channel_left){
			nResult = tas2555_i2c_write_device(pTAS2555, pTAS2555->mnLAddr, TAS2555_PAGE_REG(nRegister), nValue);
		}
		
		if(chn & channel_right){
			nResult = tas2555_i2c_write_device(pTAS2555, pTAS2555->mnRAddr, TAS2555_PAGE_REG(nRegister), nValue);
		}	
	}
	
	mutex_unlock(&pTAS2555->dev_lock);		
	
	return nResult;
}

/* 
* tas2555_dev_bulk_read :
* platform independent, don't change unless necessary
*/
static int tas2555_dev_bulk_read(
	struct tas2555_priv *pTAS2555,
	enum channel chn,
	unsigned int nRegister, 
	u8 * pData, 
	unsigned int nLength)
{
	int nResult = 0;
	unsigned char reg = 0;
	unsigned char Addr = 0;
	
	mutex_lock(&pTAS2555->dev_lock);
	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)){
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;			// let only writes from TILoad pass.
		}
								
		nRegister &= ~0x80000000;
		dev_dbg(pTAS2555->dev, "TiLoad BR CH[%d] REG B[%d]P[%d]R[%d], count=%d\n", 
				chn,
				TAS2555_BOOK_ID(nRegister),
				TAS2555_PAGE_ID(nRegister),
				TAS2555_PAGE_REG(nRegister),
				nLength);		
	}

	nResult = tas2555_change_book_page(pTAS2555, 
									chn,
									TAS2555_BOOK_ID(nRegister),
									TAS2555_PAGE_ID(nRegister));
	if(nResult >= 0){
		reg = TAS2555_PAGE_REG(nRegister);		
		if(chn == channel_left){			
			Addr = pTAS2555->mnLAddr;
		}else if(chn == channel_right){
			Addr = pTAS2555->mnRAddr;
		}else{
			dev_err(pTAS2555->dev, "bulk read chn ERROR %d\n", chn);
			nResult = -1;
		}
		
		if(nResult >= 0){
			nResult = tas2555_i2c_bulkread_device(
				pTAS2555, Addr, reg, pData, nLength);
		}
	}
									
	mutex_unlock(&pTAS2555->dev_lock);	

	return nResult;
}

/* 
* tas2555_dev_bulk_write :
* platform independent, don't change unless necessary
*/
static int tas2555_dev_bulk_write(
	struct tas2555_priv *pTAS2555,
	enum channel chn,
	unsigned int nRegister, 
	u8 * pData, 
	unsigned int nLength)
{
	int nResult = 0;
	unsigned char reg = 0;
	
	mutex_lock(&pTAS2555->dev_lock);
	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)){
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;			// let only writes from TILoad pass.
		}
							
		nRegister &= ~0x80000000;
		
		dev_dbg(pTAS2555->dev, "TiLoad BW CH[%d] REG B[%d]P[%d]R[%d], count=%d\n", 
				chn,
				TAS2555_BOOK_ID(nRegister),
				TAS2555_PAGE_ID(nRegister),
				TAS2555_PAGE_REG(nRegister),
				nLength);		
	}

	nResult = tas2555_change_book_page(
		pTAS2555, 
		chn,
		TAS2555_BOOK_ID(nRegister),
		TAS2555_PAGE_ID(nRegister));
		
	if(nResult >=0){
		reg = TAS2555_PAGE_REG(nRegister);	
		if(chn & channel_left){
			nResult = tas2555_i2c_bulkwrite_device(
				pTAS2555,
				pTAS2555->mnLAddr,
				reg, pData, nLength);
			if(nResult < 0){
				dev_err(pTAS2555->dev, "bulk write error %d\n", nResult);
			}
		}
		
		if(chn & channel_right){
			nResult = tas2555_i2c_bulkwrite_device(
				pTAS2555,
				pTAS2555->mnRAddr,
				reg, pData, nLength);
			if(nResult < 0){
				dev_err(pTAS2555->dev, "bulk write error %d\n", nResult);
			}
		}
	}
	mutex_unlock(&pTAS2555->dev_lock);		
	
	return nResult;
}

/* 
* tas2555_dev_update_bits :
* platform independent, don't change unless necessary
*/
static int tas2555_dev_update_bits(
	struct tas2555_priv *pTAS2555,
	enum channel chn,
	unsigned int nRegister, 
	unsigned int nMask, 
	unsigned int nValue)
{
	int nResult = 0;
	
	mutex_lock(&pTAS2555->dev_lock);
	
	if (pTAS2555->mbTILoadActive) {
		if (!(nRegister & 0x80000000)){
			mutex_unlock(&pTAS2555->dev_lock);
			return 0;			// let only writes from TILoad pass.
		}
								
		nRegister &= ~0x80000000;
		
		dev_dbg(pTAS2555->dev, "TiLoad SB CH[%d] REG B[%d]P[%d]R[%d], mask=0x%x, value=0x%x\n", 
				chn,
				TAS2555_BOOK_ID(nRegister),
				TAS2555_PAGE_ID(nRegister),
				TAS2555_PAGE_REG(nRegister),
				nMask, nValue);		
	}
	
	nResult = tas2555_change_book_page(
		pTAS2555,
		chn,
		TAS2555_BOOK_ID(nRegister),
		TAS2555_PAGE_ID(nRegister));
	
	if(nResult>=0){
		if(chn&channel_left){
			tas2555_i2c_update_bits(pTAS2555, pTAS2555->mnLAddr, TAS2555_PAGE_REG(nRegister), nMask, nValue);
		}
		
		if(chn&channel_right){
			tas2555_i2c_update_bits(pTAS2555, pTAS2555->mnRAddr, TAS2555_PAGE_REG(nRegister), nMask, nValue);
		}	
	}
		
	mutex_unlock(&pTAS2555->dev_lock);		
	return nResult;
}

static bool tas2555_volatile(struct device *pDev, unsigned int nRegister)
{
	return true;
}

static bool tas2555_writeable(struct device *pDev, unsigned int nRegister)
{
	return true;
}

static const struct regmap_config tas2555_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas2555_writeable,
	.volatile_reg = tas2555_volatile,
	.cache_type = REGCACHE_NONE,
	.max_register = 128,
};
/* 
* tas2555_i2c_probe :
* platform dependent
* should implement hardware reset functionality
*/
static int tas2555_i2c_probe(struct i2c_client *pClient,
	const struct i2c_device_id *pID)
{
	struct tas2555_priv *pTAS2555;
	int nResult;
	unsigned int nValue = 0;

	dev_info(&pClient->dev, "%s enter\n", __FUNCTION__);
		
	pTAS2555 = devm_kzalloc(&pClient->dev, sizeof(struct tas2555_priv), GFP_KERNEL);
	if (!pTAS2555){
		dev_err(&pClient->dev, " -ENOMEM\n");
		return -ENOMEM;
	}
	
	pTAS2555->client = pClient;
	pTAS2555->dev = &pClient->dev;
	i2c_set_clientdata(pClient, pTAS2555);
	dev_set_drvdata(&pClient->dev, pTAS2555);	
	mutex_init(&pTAS2555->dev_lock);
	
	nResult = tas2555_parse_dt(&pClient->dev, pTAS2555);
	if(nResult < 0){
		dev_err(&pClient->dev, " dt error\n");
		return -EINVAL;
	}

	pTAS2555->mpRegmap = devm_regmap_init_i2c(pClient, &tas2555_i2c_regmap);
	if (IS_ERR(pTAS2555->mpRegmap)) {
		nResult = PTR_ERR(pTAS2555->mpRegmap);
		dev_err(&pClient->dev, "Failed to allocate register map: %d\n",
			nResult);
		return nResult;
	}
	
#ifdef HARD_RESET	
	//TODO implement hardware reset here to both TAS2555 devices
	//platform dependent
	dev_info(&pClient->dev, "reset gpio is %d\n", pTAS2555->mnResetGPIO);
	if (gpio_is_valid(pTAS2555->mnResetGPIO)) {
		devm_gpio_request_one(&pClient->dev, pTAS2555->mnResetGPIO,
			GPIOF_OUT_INIT_LOW, "TAS2555_RST");
		mdelay(2);
		gpio_set_value_cansleep(pTAS2555->mnResetGPIO, 1);
		udelay(1000);
	}
#else	
	pTAS2555->mnLCurrentBook = -1;
	pTAS2555->mnLCurrentPage = -1;
	pTAS2555->mnRCurrentBook = -1;
	pTAS2555->mnRCurrentPage = -1;
#endif	
	/* Reset the chip */
	nResult = tas2555_dev_write(pTAS2555, channel_both, TAS2555_SW_RESET_REG, 1);
	if(nResult < 0){
		dev_err(&pClient->dev, "I2c fail, %d\n", nResult);	
		mutex_destroy(&pTAS2555->dev_lock);
	}else{
		udelay(1000);
		tas2555_dev_read(pTAS2555, channel_left, TAS2555_REV_PGID_REG, &nValue);
		dev_info(&pClient->dev, "Left Chn, PGID=0x%x\n", nValue);
		tas2555_dev_read(pTAS2555, channel_right, TAS2555_REV_PGID_REG, &nValue);
		dev_info(&pClient->dev, "Right Chn, PGID=0x%x\n", nValue);	
		
		pTAS2555->mpFirmware = devm_kzalloc(&pClient->dev, sizeof(TFirmware), GFP_KERNEL);
		if (!pTAS2555->mpFirmware){
			dev_err(&pClient->dev, "mpFirmware ENOMEM\n");	
			mutex_destroy(&pTAS2555->dev_lock);
			return -ENOMEM;
		}

		pTAS2555->mpCalFirmware = devm_kzalloc(&pClient->dev, sizeof(TFirmware), GFP_KERNEL);
		if (!pTAS2555->mpCalFirmware){
			dev_err(&pClient->dev, "mpCalFirmware ENOMEM\n");
			mutex_destroy(&pTAS2555->dev_lock);			
			return -ENOMEM;
		}
		
		pTAS2555->read = tas2555_dev_read;
		pTAS2555->write = tas2555_dev_write;
		pTAS2555->bulk_read = tas2555_dev_bulk_read;
		pTAS2555->bulk_write = tas2555_dev_bulk_write;
		pTAS2555->update_bits = tas2555_dev_update_bits;
		pTAS2555->set_config = tas2555_set_config;
		pTAS2555->set_calibration = tas2555_set_calibration;
		
		nResult = request_firmware_nowait(THIS_MODULE, 1, TAS2555_FW_NAME,
			pTAS2555->dev, GFP_KERNEL, pTAS2555, tas2555_fw_ready);

#ifdef CONFIG_TAS2555_CODEC_STEREO	
		tas2555_register_codec(pTAS2555);
#endif

#ifdef CONFIG_TAS2555_MISC_STEREO	
		mutex_init(&pTAS2555->file_lock);
		tas2555_register_misc(pTAS2555);
#endif

#ifdef ENABLE_TILOAD
		tiload_driver_init(pTAS2555, channel_left);
		tiload_driver_init(pTAS2555, channel_right);
#endif

	}
	
	return nResult;
}

static int tas2555_i2c_remove(struct i2c_client *pClient)
{
	struct tas2555_priv *pTAS2555 = i2c_get_clientdata(pClient);
	
	dev_info(pTAS2555->dev, "%s\n", __FUNCTION__);
	
#ifdef CONFIG_TAS2555_CODEC_STEREO		
	tas2555_deregister_codec(pTAS2555);
#endif

#ifdef CONFIG_TAS2555_MISC_STEREO		
	tas2555_deregister_misc(pTAS2555);
	mutex_destroy(&pTAS2555->file_lock);
#endif
	
	return 0;
}

static const struct i2c_device_id tas2555_i2c_id[] = {
	{"tas2555s", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tas2555_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id tas2555_of_match[] = {
	{.compatible = "ti,tas2555s"},
	{},
};

MODULE_DEVICE_TABLE(of, tas2555_of_match);
#endif

static struct i2c_driver tas2555_i2c_driver = {
	.driver = {
			.name = "tas2555s",
			.owner = THIS_MODULE,
#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr(tas2555_of_match),
#endif
		},
	.probe = tas2555_i2c_probe,
	.remove = tas2555_i2c_remove,
	.id_table = tas2555_i2c_id,
};

module_i2c_driver(tas2555_i2c_driver);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2555 Stereo I2C Smart Amplifier driver");
MODULE_LICENSE("GPLv2");
#endif