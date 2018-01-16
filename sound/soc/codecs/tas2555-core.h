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
**     tas2555-core.h
**
** Description:
**     header file for tas2555-core.c
**
** =============================================================================
*/

#ifndef _TAS2555_CORE_H
#define _TAS2555_CORE_H

#include "tas2555.h"

void tas2555_enable(struct tas2555_priv *pTAS2555, bool bEnable);
int tas2555_set_sampling_rate(struct tas2555_priv *pTAS2555, 
	unsigned int nSamplingRate);
int tas2555_set_bit_rate(struct tas2555_priv *pTAS2555, 
	enum channel chn, unsigned int nBitRate);
int tas2555_get_bit_rate(struct tas2555_priv *pTAS2555, 
	enum channel chn, unsigned char *pBitRate);
int tas2555_set_config(struct tas2555_priv *pTAS2555, int config);
void tas2555_fw_ready(const struct firmware *pFW, void *pContext);
int tas2555_set_program(struct tas2555_priv *pTAS2555,
	unsigned int nProgram);
int tas2555_set_calibration(struct tas2555_priv *pTAS2555,
	int nCalibration);
int tas2555_load_default(struct tas2555_priv *pTAS2555);
int tas2555_parse_dt(struct device *dev, struct tas2555_priv *pTAS2555);
int tas2555_get_DAC_gain(struct tas2555_priv *pTAS2555, 
	enum channel chl, unsigned char *pnGain);
int tas2555_set_DAC_gain(struct tas2555_priv *pTAS2555, 
	enum channel chl, unsigned int nGain);
	
#endif /* _TAS2555_CORE_H */
