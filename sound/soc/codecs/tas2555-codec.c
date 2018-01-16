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
**     tas2555-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2555 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2555_CODEC_STEREO

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
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tas2555-core.h"
#include "tas2555-codec.h"

//#undef KCONTROL_CODEC
#define KCONTROL_CODEC

static unsigned int tas2555_codec_read(struct snd_soc_codec *pCodec,
	unsigned int nRegister)
{
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;
	unsigned int Value = 0;

	ret = pTAS2555->read(pTAS2555, 
		pTAS2555->mnCurrentChannel, nRegister, &Value);
	
	if (ret < 0) {
		dev_err(pTAS2555->dev, "%s, %d, ERROR happen=%d\n", __FUNCTION__,
			__LINE__, ret);
		return 0;
	} else
		return Value;
}

static int tas2555_codec_write(struct snd_soc_codec *pCodec, unsigned int nRegister,
	unsigned int nValue)
{
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(pCodec);
	int ret = 0;

	ret = pTAS2555->write(pTAS2555, 
		pTAS2555->mnCurrentChannel, nRegister, nValue);
	
	return ret;
}

static const struct snd_soc_dapm_widget tas2555_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("Stereo ASI1", "Stereo ASI1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("Stereo ASI2", "Stereo ASI2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("Stereo ASIM", "Stereo ASIM Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("Stereo DAC", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUT_DRV("Stereo ClassD", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Stereo PLL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Stereo NDivider", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("Stereo OUT")
};

static const struct snd_soc_dapm_route tas2555_audio_map[] = {
	{"Stereo DAC", NULL, "Stereo ASI1"},
	{"Stereo DAC", NULL, "Stereo ASI2"},
	{"Stereo DAC", NULL, "Stereo ASIM"},
	{"Stereo ClassD", NULL, "Stereo DAC"},
	{"Stereo OUT", NULL, "Stereo ClassD"},
	{"Stereo DAC", NULL, "Stereo PLL"},
	{"Stereo DAC", NULL, "Stereo NDivider"},
};

static int tas2555_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	
	dev_dbg(pTAS2555->dev, "%s\n", __func__);

	return 0;
}

static void tas2555_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2555->dev, "%s\n", __func__);
}

static int tas2555_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	
	dev_dbg(pTAS2555->dev, "%s\n", __func__);

	tas2555_enable(pTAS2555, !mute);
	
	return 0;
}

static int tas2555_set_dai_sysclk(struct snd_soc_dai *pDAI,
	int nClkID, unsigned int nFreqency, int nDir)
{
	struct snd_soc_codec *pCodec = pDAI->codec;
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(pCodec);

	dev_dbg(pTAS2555->dev, "tas2555_set_dai_sysclk: freq = %u\n", nFreqency);

	return 0;
}

static int tas2555_hw_params(struct snd_pcm_substream *pSubstream,
	struct snd_pcm_hw_params *pParams, struct snd_soc_dai *pDAI)
{
	struct snd_soc_codec *pCodec = pDAI->codec;
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(pCodec);

	dev_dbg(pTAS2555->dev, "%s\n", __func__);
	
	tas2555_set_bit_rate(pTAS2555, channel_both, 
		snd_pcm_format_width(params_format(pParams)));
	tas2555_set_sampling_rate(pTAS2555, params_rate(pParams));

	return 0;
}

static int tas2555_set_dai_fmt(struct snd_soc_dai *pDAI, unsigned int nFormat)
{
	struct snd_soc_codec *codec = pDAI->codec;
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	
	dev_dbg(pTAS2555->dev, "%s\n", __func__);
	
	return 0;
}

static int tas2555_prepare(struct snd_pcm_substream *pSubstream,
	struct snd_soc_dai *pDAI)
{
	struct snd_soc_codec *codec = pDAI->codec;
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	
	dev_dbg(pTAS2555->dev, "%s\n", __func__);
	
	return 0;
}

static int tas2555_set_bias_level(struct snd_soc_codec *pCodec,
	enum snd_soc_bias_level eLevel)
{
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(pCodec);
	
	dev_dbg(pTAS2555->dev, "%s: %d\n", __func__, eLevel);

	return 0;
}

static int tas2555_codec_probe(struct snd_soc_codec *pCodec)
{
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(pCodec);

	dev_dbg(pTAS2555->dev, "%s\n", __func__);

	return 0;
}

static int tas2555_codec_remove(struct snd_soc_codec *pCodec)
{
	return 0;
}

static int tas2555_power_ctrl_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);

	pValue->value.integer.value[0] = pTAS2555->mbPowerUp;
	dev_dbg(pTAS2555->dev, "tas2555_power_ctrl_get = %d\n",
		pTAS2555->mbPowerUp);
		
	return 0;
}

static int tas2555_power_ctrl_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);

	pTAS2555->mbPowerUp = pValue->value.integer.value[0];

	dev_dbg(pTAS2555->dev, "tas2555_power_ctrl_put = %d\n",
		pTAS2555->mbPowerUp);

	if (pTAS2555->mbPowerUp == 1)
		tas2555_enable(pTAS2555, true);
	if (pTAS2555->mbPowerUp == 0)
		tas2555_enable(pTAS2555, false);
		
	return 0;
}

static int tas2555_fs_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);

	int nFS = 48000;
	
	if (pTAS2555->mpFirmware->mnConfigurations)
		nFS = pTAS2555->mpFirmware->mpConfigurations[pTAS2555->mnCurrentConfiguration].mnSamplingRate;
	
	pValue->value.integer.value[0] = nFS;
		
	dev_dbg(pTAS2555->dev, "tas2555_fs_get = %d\n", nFS);
	return 0;
}

static int tas2555_fs_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int nFS = pValue->value.integer.value[0];
	
	dev_info(pTAS2555->dev, "tas2555_fs_put = %d\n", nFS);
	
	ret = tas2555_set_sampling_rate(pTAS2555, nFS);
	
	return ret;
}

static int tas2555_program_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);

	pValue->value.integer.value[0] = pTAS2555->mnCurrentProgram;
	dev_dbg(pTAS2555->dev, "tas2555_program_get = %d\n",
		pTAS2555->mnCurrentProgram);
		
	return 0;
}

static int tas2555_program_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	unsigned int nProgram = pValue->value.integer.value[0];
	int ret = 0;
	
	ret = tas2555_set_program(pTAS2555, nProgram);
	
	return ret;
}

static int tas2555_configuration_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);

	pValue->value.integer.value[0] = pTAS2555->mnCurrentConfiguration;
	dev_dbg(pTAS2555->dev, "tas2555_configuration_get = %d\n",
		pTAS2555->mnCurrentConfiguration);
			
	return 0;
}

static int tas2555_configuration_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	unsigned int nConfiguration = pValue->value.integer.value[0];
	int ret = 0;

	ret = tas2555_set_config(pTAS2555, nConfiguration);
	
	return ret;
}

static int tas2555_calibration_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);

	pValue->value.integer.value[0] = pTAS2555->mnCurrentCalibration;
	dev_info(pTAS2555->dev,
		"tas2555_calibration_get = %d\n",
		pTAS2555->mnCurrentCalibration);
			
	return 0;
}

static int tas2555_calibration_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	unsigned int nCalibration = pValue->value.integer.value[0];
	int ret = 0;
	
	ret = tas2555_set_calibration(pTAS2555, nCalibration);
	
	return ret;
}

static int tas2555_ldac_gain_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	unsigned char nGain = 0;
	int ret = -1;
	
	ret = tas2555_get_DAC_gain(pTAS2555, channel_left, &nGain);
	if(ret >= 0){
		pValue->value.integer.value[0] = nGain;
	}
	
	dev_dbg(pTAS2555->dev, "%s, ret = %d, %d\n", __func__, ret, nGain);
			
	return ret;
}

static int tas2555_ldac_gain_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	unsigned int nGain = pValue->value.integer.value[0];
	int ret = 0;
	
	ret = tas2555_set_DAC_gain(pTAS2555, channel_left, nGain);
	
	return ret;
}

static int tas2555_rdac_gain_get(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	unsigned char nGain = 0;
	int ret = -1;
	
	ret = tas2555_get_DAC_gain(pTAS2555, channel_right, &nGain);
	if(ret >= 0){
		pValue->value.integer.value[0] = nGain;
	}
	
	dev_dbg(pTAS2555->dev, "%s, ret = %d, %d\n", __func__, ret, nGain);
			
	return ret;
}

static int tas2555_rdac_gain_put(struct snd_kcontrol *pKcontrol,
	struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2555_priv *pTAS2555 = snd_soc_codec_get_drvdata(codec);
	unsigned int nGain = pValue->value.integer.value[0];
	int ret = 0;
	
	ret = tas2555_set_DAC_gain(pTAS2555, channel_right, nGain);
	
	return ret;
}

static const struct snd_kcontrol_new tas2555_snd_controls[] = {
	SOC_SINGLE_EXT("Stereo LDAC Playback Volume", SND_SOC_NOPM, 0, 0x0f, 0, 
		tas2555_ldac_gain_get, tas2555_ldac_gain_put),
	SOC_SINGLE_EXT("Stereo RDAC Playback Volume", SND_SOC_NOPM, 0, 0x0f, 0, 
		tas2555_rdac_gain_get, tas2555_rdac_gain_put),		
	SOC_SINGLE_EXT("Stereo PowerCtrl", SND_SOC_NOPM, 0, 0x0001, 0,
		tas2555_power_ctrl_get, tas2555_power_ctrl_put),
	SOC_SINGLE_EXT("Stereo Program", SND_SOC_NOPM, 0, 0x00FF, 0, tas2555_program_get,
		tas2555_program_put),
	SOC_SINGLE_EXT("Stereo Configuration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2555_configuration_get, tas2555_configuration_put),
	SOC_SINGLE_EXT("Stereo FS", SND_SOC_NOPM, 8000, 48000, 0,
		tas2555_fs_get, tas2555_fs_put),
	SOC_SINGLE_EXT("Stereo Calibration", SND_SOC_NOPM, 0, 0x00FF, 0,
		tas2555_calibration_get, tas2555_calibration_put),
};

static struct snd_soc_codec_driver soc_codec_driver_tas2555 = {
	.probe = tas2555_codec_probe,
	.remove = tas2555_codec_remove,
	.read = tas2555_codec_read,
	.write = tas2555_codec_write,
	.set_bias_level = tas2555_set_bias_level,
	.idle_bias_off = true,
	//.ignore_pmdown_time = true,
	.controls = tas2555_snd_controls,
	.num_controls = ARRAY_SIZE(tas2555_snd_controls),
	.dapm_widgets = tas2555_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tas2555_dapm_widgets),
	.dapm_routes = tas2555_audio_map,
	.num_dapm_routes = ARRAY_SIZE(tas2555_audio_map),
};

static struct snd_soc_dai_ops tas2555_dai_ops = {
	.startup = tas2555_startup,
	.shutdown = tas2555_shutdown,
	.digital_mute = tas2555_mute,
	.hw_params = tas2555_hw_params,
	.prepare = tas2555_prepare,
	.set_sysclk = tas2555_set_dai_sysclk,
	.set_fmt = tas2555_set_dai_fmt,
};

#define TAS2555_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
             SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)
static struct snd_soc_dai_driver tas2555_dai_driver[] = {
	{
		.name = "tas2555 Stereo ASI1",
		.id = 0,
		.playback = {
				.stream_name = "Stereo ASI1 Playback",
				.channels_min = 2,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = TAS2555_FORMATS,
			},
		.ops = &tas2555_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tas2555 Stereo ASI2",
		.id = 1,
		.playback = {
				.stream_name = "Stereo ASI2 Playback",
				.channels_min = 2,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = TAS2555_FORMATS,
			},
		.ops = &tas2555_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "tas2555 Stereo ASIM",
		.id = 2,
		.playback = {
				.stream_name = "Stereo ASIM Playback",
				.channels_min = 2,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_192000,
				.formats = TAS2555_FORMATS,
			},
		.ops = &tas2555_dai_ops,
		.symmetric_rates = 1,
	},
};

int tas2555_register_codec(struct tas2555_priv *pTAS2555)
{
	int nResult = 0;

	dev_info(pTAS2555->dev, "%s, enter\n", __FUNCTION__);
	  
	nResult = snd_soc_register_codec(pTAS2555->dev, 
		&soc_codec_driver_tas2555,
		tas2555_dai_driver, ARRAY_SIZE(tas2555_dai_driver));
		
	return nResult;
}

int tas2555_deregister_codec(struct tas2555_priv *pTAS2555)
{
	snd_soc_unregister_codec(pTAS2555->dev);
		
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2555 ALSA SOC Smart Amplifier Stereo driver");
MODULE_LICENSE("GPLv2");
#endif
