/*
 *  sound/soc/codecs/rt5508.c
 *  Driver to Richtek RT5508 HPAMP IC
 *
 *  Copyright (C) 2015 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
/* alsa sound header */
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

#include "rt5508.h"

struct reg_config {
	uint8_t reg_addr;
	uint32_t reg_data;
};

static const char * const prop_str[RT5508_CFG_MAX] = {
	"general",
	"boost_converter",
	"speaker_protect",
	"safe_guard",
	"eq",
	"bwe",
	"dcr",
	"mbdrc",
	"alc",
};

static const struct reg_config battmode_config[] = {
};

static const struct reg_config adaptive_config[] = {
};

static const struct reg_config general_config[] = {
	{ 0x1e, 0x2b},
	{ 0x23, 0x34},
	{ 0x1f, 0xbc},
	{ 0x20, 0x8a},
	{ 0x21, 0x58},
	{ 0x2c, 0x00},
	{ 0x5b, 0x01},
	{ 0x84, 0x14},
	{ 0x87, 0x10},
	{ 0x88, 0x90},
	{ 0x89, 0x14},
	{ 0x90, 0x9b},
	{ 0x91, 0x55},
	{ 0x92, 0x55},
	{ 0x93, 0x55},
	{ 0x95, 0x00},
	{ 0x96, 0x00},
	{ 0x98, 0x00},
	{ 0x99, 0x0c},
	{ 0x9a, 0x1c},
	{ 0x9d, 0x43},
	{ 0x9e, 0x80},
	{ 0x9f, 0x0c},
	{ 0xc5, 0x00},
	{ 0xc6, 0x03},
	{ 0xc7, 0x00},
	{ 0x10, 0x02},
	{ 0x11, 0x80},
	{ 0x12, 0x80},
	{ 0x13, 0x3bbb},
	{ 0x14, 0x7fff},
	{ 0x81, 0x99},
	{ 0x43, 0x22},
	{ 0x44, 0xb1},
	{ 0x45, 0x1a},
	{ 0x50, 0x3e},
	{ 0xc3, 0x08},
	{ 0x85, 0xdd},
	{ 0x8d, 0x60},
};

static const struct reg_config revg_general_config[] = {
	{ 0x1e, 0x0b},
	{ 0x18, 0x39},
	{ 0x1c, 0x7f},
	{ 0x1d, 0x80},
	{ 0x25, 0x04},
	{ 0x28, 0x20},
	{ 0x50, 0x0c},
	{ 0x5b, 0x01},
	{ 0x81, 0x9a},
	{ 0x86, 0x18},
	{ 0x87, 0x14},
	{ 0x92, 0x5d},
	{ 0x93, 0x56},
	{ 0x99, 0x00},
	{ 0x9a, 0xdc},
	{ 0xa0, 0x28},
	{ 0xb4, 0x01},
	{ 0xb5, 0x64},
	{ 0x82, 0xc2},
	{ 0x83, 0x45},
	{ 0xb3, 0x02},
	{ 0x94, 0x15},
	{ 0x14, 0x6c00},
	{ 0x16, 0x48},
	{ 0x17, 0x15},
	{ 0x19, 0x01b0},
	{ 0x1a, 0x00},
	{ 0x1f, 0xca},
	{ 0x20, 0x97},
	{ 0x21, 0x64},
	{ 0x22, 0x05},
	{ 0x23, 0x3a},
	{ 0x24, 0xb1},
	{ 0x2b, 0x30},
	{ 0x2c, 0x7f},
};

static const struct reg_config orevc_general_config[] = {
	{ 0x82, 0xc2},
	{ 0x83, 0x45},
	{ 0xb3, 0x02},
	{ 0x94, 0x15},
};

static const struct reg_config urevc_general_config[] = {
	{ 0x82, 0x82},
	{ 0x83, 0x55},
	{ 0x94, 0x15},
};

static int rt5508_block_read(
	void *client, u32 reg, int bytes, void *dest)
{
#if RT5508_SIMULATE_DEVICE
	struct rt5508_chip *chip = i2c_get_clientdata(client);
	int offset = 0, ret = 0;

	offset = rt5508_calculate_offset(reg);
	if (offset < 0) {
		dev_err(chip->dev, "%s: unknown register 0x%02x\n", __func__,
			ret);
		ret = -EINVAL;
	} else
		memcpy(dest, chip->sim + offset, bytes);
	return ret;
#else
	return i2c_smbus_read_i2c_block_data(client, reg, bytes, dest);
#endif /* #if RT5508_SIMULATE_DEVICE */
}

static int rt5508_block_write(void *client, u32 reg,
	int bytes, const void *src)
{
#if RT5508_SIMULATE_DEVICE
	struct rt5508_chip *chip = i2c_get_clientdata(client);
	int offset = 0, ret = 0;

	offset = rt5508_calculate_offset(reg);
	if (offset < 0) {
		dev_err(chip->dev, "%s: unknown register 0x%02x\n", __func__,
			ret);
		ret = -EINVAL;
	} else
		memcpy(chip->sim + offset, src, bytes);
	return ret;
#else
	return i2c_smbus_write_i2c_block_data(client, reg, bytes, src);
#endif /* #if RT5508_SIMULATE_DEVICE */
}

static struct rt_regmap_fops rt5508_regmap_ops = {
	.read_device = rt5508_block_read,
	.write_device = rt5508_block_write,
};

/* Global read/write function */
static int rt5508_update_bits(struct i2c_client *i2c, u32 reg,
			   u32 mask, u32 data, int bytes)
{
	struct rt5508_chip *chip = i2c_get_clientdata(i2c);
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd;

	return rt_regmap_update_bits(chip->rd, &rrd, reg, mask, data);
#else
	u32 read_data = 0;
	u8 *p_data = (u8 *)&read_data;
	int i = 0, j = 0, ret = 0;

	down(&chip->io_semaphore);
	ret = rt5508_block_read(chip->i2c, reg, bytes, &read_data);
	if (ret < 0)
		goto err_bits;
	j = (bytes / 2);
	for (i = 0; i < j; i++)
		swap(p_data[i], p_data[bytes - i]);
	ret = rt5508_block_write(chip->i2c, reg, bytes, &read_data);
	if (ret < 0)
		goto err_bits;
err_bits:
	up(&chip->io_semaphore);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt5508_set_bits(struct i2c_client *i2c, u32 reg, u8 mask)
{
	return rt5508_update_bits(i2c, reg, mask, mask, 1);
}

static int rt5508_clr_bits(struct i2c_client *i2c, u32 reg, u8 mask)
{
	return rt5508_update_bits(i2c, reg, mask, 0, 1);
}

static unsigned int rt5508_io_read(struct snd_soc_codec *codec,
	 unsigned int reg)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};

	dev_dbg(codec->dev, "%s: reg %02x\n", __func__, reg);
	ret = rt_regmap_reg_read(chip->rd, &rrd, reg);
	return (ret < 0 ? ret : rrd.rt_data.data_u32);
#else
	u8 data = 0;

	down(&chip->io_semaphore);
	ret = rt5508_block_read(chip->i2c, reg, 1, &data);
	up(&chip->io_semaphore);
	return (ret < 0 ? ret : data);
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt5508_io_write(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int data)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};

	dev_dbg(codec->dev, "%s: reg %02x data %02x\n", __func__, reg, data);
	return rt_regmap_reg_write(chip->rd, &rrd, reg, data);
#else
	int ret = 0;

	down(&chip->io_semaphore);
	ret = rt5508_block_write(chip->i2c, reg, 1, &data);
	up(&chip->io_semaphore);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static inline int rt5508_power_on(struct rt5508_chip *chip, bool en)
{
	int ret = 0;

	dev_dbg(chip->dev, "%s: en %d\n", __func__, en);
	if (en)
		ret = rt5508_clr_bits(chip->i2c, RT5508_REG_CHIPEN,
				RT5508_CHIPPD_ENMASK);
	else
		ret = rt5508_set_bits(chip->i2c, RT5508_REG_CHIPEN,
				RT5508_CHIPPD_ENMASK);
	mdelay(1);
	return ret;
}

static int rt5508_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
#else
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#endif
	int ret = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		dapm->bias_level = level;
		break;
	case SND_SOC_BIAS_STANDBY:
		ret = rt5508_power_on(chip, true);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_MODE,
			0x03, chip->mode_store);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_DSPKCONF4,
			0x80, 0x80);
		if (ret < 0)
			return ret;
		dapm->bias_level = level;
		ret = 0;
		break;
	case SND_SOC_BIAS_OFF:
		ret = snd_soc_update_bits(codec, RT5508_REG_DSPKCONF4,
			0x80, 0x00);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_BST_MODE);
		if (ret < 0)
			return ret;
		chip->mode_store = ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_MODE,
			0x03, 0x00);
		if (ret < 0)
			return ret;
		ret = rt5508_power_on(chip, false);
		if (ret < 0)
			return ret;
		dapm->bias_level = level;
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int rt5508_init_battmode_setting(struct snd_soc_codec *codec)
{
	int i, ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(battmode_config); i++) {
		ret = snd_soc_write(codec, battmode_config[i].reg_addr,
			battmode_config[i].reg_data);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int rt5508_init_adaptive_setting(struct snd_soc_codec *codec)
{
	int i, ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(adaptive_config); i++) {
		ret = snd_soc_write(codec, adaptive_config[i].reg_addr,
			adaptive_config[i].reg_data);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int rt5508_init_general_setting(struct snd_soc_codec *codec)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	int i, ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	ret = snd_soc_update_bits(codec, RT5508_REG_I2SSEL, 0x0c,
				  chip->pdata->chan_sel << 2);
	if (ret < 0)
		return ret;
	if (chip->pdata->do_enable) {
		ret = snd_soc_update_bits(codec, RT5508_REG_I2SDOSEL,
					  0x01, 0x01);
		if (ret < 0)
			return ret;
	}
	for (i = 0; i < ARRAY_SIZE(general_config); i++) {
		ret = snd_soc_write(codec, general_config[i].reg_addr,
			general_config[i].reg_data);
		if (ret < 0)
			return ret;
	}
	if (chip->chip_rev >= RT5508_CHIP_REVG)
		goto over_revg;
	else if (chip->chip_rev >= RT5508_CHIP_REVC)
		goto over_revc;
	else
		goto under_revc;
over_revg:
	for (i = 0; i < ARRAY_SIZE(revg_general_config); i++) {
		ret = snd_soc_write(codec, revg_general_config[i].reg_addr,
			revg_general_config[i].reg_data);
		if (ret < 0)
			return ret;
	}
	goto out_init_general;
over_revc:
	for (i = 0; i < ARRAY_SIZE(orevc_general_config); i++) {
		ret = snd_soc_write(codec, orevc_general_config[i].reg_addr,
			orevc_general_config[i].reg_data);
		if (ret < 0)
			return ret;
	}
	goto out_init_general;
under_revc:
	for (i = 0; i < ARRAY_SIZE(urevc_general_config); i++) {
		ret = snd_soc_write(codec, urevc_general_config[i].reg_addr,
			urevc_general_config[i].reg_data);
		if (ret < 0)
			return ret;
	}
out_init_general:
	return 0;
}

static int rt5508_do_tcsense_fix(struct snd_soc_codec *codec)
{
	int i = 0, j = 0, index = 0;
	uint32_t tc_sense = 0, vtemp = 0;
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	ret = snd_soc_read(codec, RT5508_REG_CALIB_CTRL);
	if (ret < 0)
		return ret;
	if (ret & 0x10)
		goto tcsense_from_reg;
	/* VTEMP 0x90 ~ 0x97 */
	for (i = 0; i < 8; i++) {
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR, 0x97 - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		for (j = 7; j >= 0; j--) {
			if (!(ret & (0x01 << j)))
				break;
		}
		if (j >= 0)
			break;
	}
	dev_dbg(codec->dev, "i = %d, j= %d\n", i, j);
	if (i >= 8) {
		ret = 0x8000;
		goto tcsense_calc;
	}
	/* VTEMP 0x98 ~ 0x117 */
	index = ((7 - i) * 8 + j) * 2 + 0x98;
	ret = snd_soc_write(codec, RT5508_REG_OTPADDR, index);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5508_REG_OTPCONF , 0x88);
	if (ret < 0)
		return ret;
	ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
	if (ret < 0)
		return ret;
	/* prevent devide by zero */
	if (ret == 0)
		ret = 0x8000;
	goto tcsense_calc;
tcsense_from_reg:
	ret = snd_soc_read(codec, RT5508_REG_VTEMP_TRIM);
	if (ret < 0)
		return ret;
tcsense_calc:
	vtemp = ret & 0xffff;
	dev_dbg(codec->dev, "vtemp %04x\n", vtemp);
	/* rounding */
	tc_sense = (1073741824U / vtemp * 250 / 280) << 1;
	if (tc_sense & 0x10000)
		tc_sense = (tc_sense & 0xffff)+ 1;
	tc_sense &= 0xffff;
	dev_dbg(codec->dev, "tc_sense %04x\n", tc_sense);
	return  snd_soc_write(codec, RT5508_REG_TCOEFF, tc_sense);
}

static int rt5508_do_otp_recheck(struct snd_soc_codec *codec)
{
	int i = 0, j = 0;
	uint8_t data = 0;
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	ret = snd_soc_read(codec, RT5508_REG_CALIB_CTRL);
	if (ret < 0)
		return ret;
	data = (uint8_t)ret;
	/* Gsense check 0x1A0~0x1A7 */
	for (i = 0; i < 8; i++) {
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR, 0x1A7 - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		for (j = 7; j >= 0; j--) {
			if (!(ret & (0x01 << j)))
				break;
		}
		if (j >= 0)
			break;
	}
	if (i >= 8) {
		dev_warn(codec->dev, "gsense not calibrated\n");
		data |= 0x02;
	} else
		data &= ~0x02;
	/* RSPK check 0x268~0x26f */
	for (i = 0; i < 8; i++) {
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR, 0x26f - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		for (j = 7; j >= 0; j--) {
			if (!(ret & (0x01 << j)))
				break;
		}
		if (j >= 0)
			break;
	}
	if (i >= 8) {
		dev_warn(codec->dev, "rspk not calibrated\n");
		data |= 0x08;
	} else
		data &= ~0x08;
	return snd_soc_write(codec, RT5508_REG_CALIB_CTRL, data);
}

static int rt5508_init_proprietary_setting(struct snd_soc_codec *codec)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	struct rt5508_proprietary_param *p_param = chip->pdata->p_param;
	const u8 *cfg;
	u32 cfg_size;
	int i = 0, j = 0;
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	if (!p_param)
		goto out_init_proprietary;
	ret = snd_soc_write(codec, RT5508_REG_CLKEN1, 0x04);
	if (ret < 0)
		goto out_init_proprietary;
	for (i = 0; i < RT5508_CFG_MAX; i++) {
		cfg = p_param->cfg[i];
		cfg_size = p_param->cfg_size[i];
		if (!cfg)
			continue;
		dev_dbg(chip->dev, "%s start\n", prop_str[i]);
		for (j = 0; j < cfg_size;) {
#ifdef CONFIG_RT_REGMAP
			ret = rt_regmap_block_write(chip->rd, cfg[0], cfg[1],
					      cfg + 2);
#else
			ret = rt5508_block_write(chip->i2c, cfg[0], cfg[1],
					      cfg + 2);
#endif /* #ifdef CONFIG_RT_REGMAP */
			if (ret < 0)
				dev_err(chip->dev, "set %02x fail\n", cfg[0]);
			j += (2 + cfg[1]);
			cfg += (2 + cfg[1]);
		}
		dev_dbg(chip->dev, "%s end\n", prop_str[i]);
	}
	ret = snd_soc_write(codec, RT5508_REG_CLKEN1, 0x00);
	if (ret < 0)
		goto out_init_proprietary;
	ret = rt5508_do_otp_recheck(codec);
	if (ret < 0) {
		dev_err(codec->dev, "do_otp_recheck fail\n");
		goto out_init_proprietary;
	}
	if (p_param->cfg_size[RT5508_CFG_SPEAKERPROT]) {
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
			RT5508_SPKPROT_ENMASK, RT5508_SPKPROT_ENMASK);
	}
out_init_proprietary:
	return ret;
}

static ssize_t rt5508_proprietary_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct rt5508_chip *chip = dev_get_drvdata(dev);
	struct rt5508_proprietary_param *param = chip->pdata->p_param;
	int i = 0, j = 0;
	const u8 *cfg;
	u32 cfg_size;

	dev_dbg(chip->dev, "%s\n", __func__);
	if (!param) {
		i += scnprintf(buf + i, PAGE_SIZE - i, "no proprietary parm\n");
		goto out_show;
	}
	for (j = 0; j < RT5508_CFG_MAX; j++) {
		cfg = param->cfg[j];
		cfg_size = param->cfg_size[j];
		if (!cfg) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "no %s cfg\n",
				       prop_str[j]);
			continue;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s size %d\n",
			       prop_str[j], cfg_size);
	}
out_show:
	return i;
}

static ssize_t rt5508_proprietary_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t cnt)
{
	struct rt5508_chip *chip = dev_get_drvdata(dev);
	struct rt5508_proprietary_param *param = NULL;
	int i = 0, size = 0;
	const u8 *bin_offset;
	u32 *sptr;

	dev_dbg(chip->dev, "%s, size %d\n", __func__, (int)cnt);
	/* do mark check */
	if (cnt < 7) {
		dev_err(chip->dev, "data is invalid\n");
		goto out_param_write;
	}
	if (strncmp("richtek", buf, 7)) {
		dev_err(chip->dev, "data is invalid\n");
		goto out_param_write;
	}
	/* do size check */
	sptr = (u32 *)(buf + cnt - 4);
	size = *sptr;
	dev_dbg(chip->dev, "size %d\n", size);
	if (cnt < 47 || (size + 47) != cnt) {
		dev_err(chip->dev, "sorry bin size is wrong\n");
		goto out_param_write;
	}
	for (i = 0; i < RT5508_CFG_MAX; i++) {
		sptr = (u32 *)(buf + cnt - 40 + i * 4);
		size -= *sptr;
	}
	if (size != 0) {
		dev_err(chip->dev, "sorry, bin format is wrong\n");
		goto out_param_write;
	}
	/* if previous one is existed, release it */
	param = chip->pdata->p_param;
	if (param) {
		dev_dbg(chip->dev, "previous existed\n");
		for (i = 0; i < RT5508_CFG_MAX; i++)
			devm_kfree(chip->dev, param->cfg[i]);
		devm_kfree(chip->dev, param);
		chip->pdata->p_param = NULL;
	}
	/* start to copy */
	param = devm_kzalloc(chip->dev, sizeof(*param), GFP_KERNEL);
	if (!param) {
		dev_err(chip->dev, "allocation memory fail\n");
		goto out_param_write;
	}
	bin_offset = buf + 7;
	for (i = 0; i < RT5508_CFG_MAX; i++) {
		sptr = (u32 *)(buf + cnt - 40 + i * 4);
		param->cfg_size[i] = *sptr;
		param->cfg[i] = devm_kzalloc(chip->dev,
					     sizeof(u8) * param->cfg_size[i],
					     GFP_KERNEL);
		memcpy(param->cfg[i], bin_offset, param->cfg_size[i]);
		bin_offset += param->cfg_size[i];
	}
	chip->pdata->p_param = param;
	if (rt5508_set_bias_level(chip->codec, SND_SOC_BIAS_STANDBY) < 0)
		goto out_param_write;
	rt5508_init_proprietary_setting(chip->codec);
	if (rt5508_set_bias_level(chip->codec, SND_SOC_BIAS_OFF) < 0)
		goto out_param_write;
	return cnt;
out_param_write:
	return -EINVAL;
}

static struct device_attribute rt5508_proprietary_attr = {
	.attr = {
		.name = "prop_param",
		.mode = S_IRUGO | S_IWUSR,
	},
	.show = rt5508_proprietary_show,
	.store = rt5508_proprietary_store,
};

static int rt5508_param_probe(struct platform_device *pdev)
{
	struct rt5508_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int ret = 0;

	ret = device_create_file(&pdev->dev, &rt5508_proprietary_attr);
	if (ret < 0) {
		dev_err(&pdev->dev, "create file error\n");
		return ret;
	}
	platform_set_drvdata(pdev, chip);
	return 0;
}

static int rt5508_param_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &rt5508_proprietary_attr);
	return 0;
}

static struct platform_driver rt5508_param_driver = {
	.driver = {
		.name = "rt5508_param",
		.owner = THIS_MODULE,
	},
	.probe = rt5508_param_probe,
	.remove = rt5508_param_remove,
};
static int param_drv_registered;

static int rt5508_param_create(struct rt5508_chip *chip)
{
	static int dev_cnt;

	chip->pdev = platform_device_register_data(chip->dev, "rt5508_param",
						   dev_cnt++, NULL, 0);
	if (!chip->pdev)
		return -EFAULT;
	if (!param_drv_registered) {
		param_drv_registered = 1;
		return platform_driver_register(&rt5508_param_driver);
	}
	return 0;
}

static void rt5508_param_destroy(struct rt5508_chip *chip)
{
	platform_device_unregister(chip->pdev);
	if (param_drv_registered) {
		platform_driver_unregister(&rt5508_param_driver);
		param_drv_registered = 0;
	}
}

static int rt5508_codec_probe(struct snd_soc_codec *codec)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	/* CHIP Enable */
	ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
		RT5508_CHIPPD_ENMASK, ~RT5508_CHIPPD_ENMASK);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5508_init_general_setting(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5508_init_adaptive_setting(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5508_init_battmode_setting(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5508_init_proprietary_setting(codec);
	if (ret < 0)
		goto err_out_probe;
	ret = rt5508_do_tcsense_fix(codec);
	if (ret < 0) {
		dev_err(codec->dev, "do tcsense fix fail\n");
		goto err_out_probe;
	}
	ret = rt5508_param_create(chip);
	if (ret < 0)
		goto err_out_probe;
	chip->codec = codec;
	ret = rt5508_calib_create(chip);
	if (ret < 0)
		goto err_out_probe;
	return rt5508_set_bias_level(codec, SND_SOC_BIAS_OFF);
err_out_probe:
	dev_err(codec->dev, "chip io error\n");
	/* Chip Disable */
	snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
		RT5508_CHIPPD_ENMASK, RT5508_CHIPPD_ENMASK);
	return ret;
}

static int rt5508_codec_remove(struct snd_soc_codec *codec)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);

	rt5508_calib_destroy(chip);
	rt5508_param_destroy(chip);
	return rt5508_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

#ifdef CONFIG_PM
static int rt5508_codec_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int rt5508_codec_resume(struct snd_soc_codec *codec)
{
	return 0;
}
#else
#define rt5508_codec_suspend NULL
#define rt5508_codec_resume NULL
#endif /* #ifdef CONFIG_PM */

static int rt5508_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
#else
	struct snd_soc_codec *codec = w->codec;
#endif
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = snd_soc_update_bits(codec, RT5508_REG_CLKEN1,
			RT5508_CLKEN1_MASK, 0xbf);
		if (ret < 0)
			goto out_clk_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_CLKEN2,
			RT5508_CLKEN2_MASK, RT5508_CLKEN2_MASK);
		if (ret < 0)
			goto out_clk_event;
		break;
	case SND_SOC_DAPM_POST_PMD:
		msleep(20);
		ret = snd_soc_update_bits(codec, RT5508_REG_CLKEN2,
			RT5508_CLKEN2_MASK, ~RT5508_CLKEN2_MASK);
		if (ret < 0)
			goto out_clk_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_CLKEN1,
			RT5508_CLKEN1_MASK, ~RT5508_CLKEN1_MASK);
		if (ret < 0)
			goto out_clk_event;
		break;
	default:
		break;
	}
out_clk_event:
	return ret;
}

static int rt5508_boost_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
#else
	struct snd_soc_codec *codec = w->codec;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = snd_soc_read(codec, RT5508_REG_BST_MODE);
		if (ret < 0)
			goto out_boost_event;
		chip->mode_store = ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_MODE,
			0x03, 0x01);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_DSPKCONF5,
			RT5508_VBG_ENMASK, RT5508_VBG_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_DSPKVMID,
			RT5508_VMID_ENMASK, RT5508_VMID_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_DSPKEN1,
				RT5508_BUF_ENMASK | RT5508_BIAS_ENMASK,
				RT5508_BUF_ENMASK | RT5508_BIAS_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec,
			RT5508_REG_AMPCONF, 0xF8, 0xB8);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_CONF1,
			0x80, 0x80);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_OCPOTPEN,
			0x03, 0x03);
		if (ret < 0)
			goto out_boost_event;
		if (chip->chip_rev > RT5508_CHIP_REVF) {
			ret = snd_soc_update_bits(codec, RT5508_REG_OVPUVPCTRL,
				0xe0, 0xe0);
		} else {
			ret = snd_soc_update_bits(codec, RT5508_REG_OVPUVPCTRL,
				0xc0, 0xc0);
		}
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
			RT5508_TRIWAVE_ENMASK, RT5508_TRIWAVE_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		dev_info(chip->dev, "amp turn on\n");
		break;
	case SND_SOC_DAPM_POST_PMU:
		msleep(10);
		ret = snd_soc_update_bits(codec, RT5508_REG_CLKEN1, 0x40, 0x40);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_MODE,
			0x03, chip->mode_store);
		if (ret < 0)
			goto out_boost_event;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_CONF1,
			0x80, 0x80);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_read(codec, RT5508_REG_BST_MODE);
		if (ret < 0)
			goto out_boost_event;
		chip->mode_store = ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_MODE,
			0x03, 0x01);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_CLKEN1, 0x40, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_OVPUVPCTRL,
			0xe0, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_OCPOTPEN,
			0x03, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_read(codec, RT5508_REG_CHIPEN);
		if (ret < 0)
			goto out_boost_event;
		if (ret & RT5508_SPKPROT_ENMASK) {
			ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				RT5508_SPKPROT_ENMASK, ~RT5508_SPKPROT_ENMASK);
			if (ret < 0)
				goto out_boost_event;
			ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				RT5508_SPKPROT_ENMASK, RT5508_SPKPROT_ENMASK);
			if (ret < 0)
				goto out_boost_event;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_info(chip->dev, "amp turn off\n");
		msleep(50);
		ret = snd_soc_update_bits(codec, RT5508_REG_PILOTEN, 0x80, 0);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_PILOTEN,
					  0x80, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
			RT5508_TRIWAVE_ENMASK, ~RT5508_TRIWAVE_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec,
			RT5508_REG_AMPCONF, 0xF8, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_DSPKEN1,
			RT5508_BUF_ENMASK | RT5508_BIAS_ENMASK,
			~(RT5508_BUF_ENMASK | RT5508_BIAS_ENMASK));
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_DSPKVMID,
			RT5508_VMID_ENMASK, ~RT5508_VMID_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_DSPKCONF5,
			RT5508_VBG_ENMASK, ~RT5508_VBG_ENMASK);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_CONF1,
			0x80, 0x00);
		if (ret < 0)
			goto out_boost_event;
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_MODE,
			0x03, chip->mode_store);
		if (ret < 0)
			goto out_boost_event;
		break;
	default:
		break;
	}
out_boost_event:
	return ret;
}

static const char * const rt5508_i2smux_text[] = { "I2S1", "I2S2"};
static const char * const rt5508_i2sdomux_text[] = { "I2SDOR/L", "DATAI3"};
static SOC_ENUM_SINGLE_DECL(rt5508_i2s_muxsel,
	RT5508_REG_I2SSEL, RT5508_I2SSEL_SHFT, rt5508_i2smux_text);
static SOC_ENUM_SINGLE_DECL(rt5508_i2s_dosel,
	RT5508_REG_I2SDOSEL, 1, rt5508_i2sdomux_text);
static const struct snd_kcontrol_new rt5508_i2smux_ctrl =
	SOC_DAPM_ENUM("Switch", rt5508_i2s_muxsel);
static const struct snd_kcontrol_new rt5508_i2sdo_ctrl =
	SOC_DAPM_ENUM("Switch", rt5508_i2s_dosel);
static const struct snd_soc_dapm_widget rt5508_dapm_widgets[] = {
	SND_SOC_DAPM_MUX("I2S Mux", SND_SOC_NOPM, 0, 0, &rt5508_i2smux_ctrl),
	SND_SOC_DAPM_MUX("I2SDO Mux", RT5508_REG_I2SDOSEL, 0, 0,
		&rt5508_i2sdo_ctrl),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV_E("BOOST", RT5508_REG_CHIPEN, RT5508_SPKAMP_ENSHFT,
		0, NULL, 0, rt5508_boost_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("CLK", RT5508_REG_PLLCONF1, 0, 1, rt5508_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};

static const struct snd_soc_dapm_route rt5508_dapm_routes[] = {
	{ "I2S Mux", "I2S1", "AIF1 Playback"},
	{ "I2S Mux", "I2S2", "AIF2 Playback"},
	/* DATAO path start */
	{ "I2SDO Mux", "I2SDOR/L", "I2S Mux"},
	{ "I2SDO Mux", "DATAI3", "I2S Mux"},
	{ "AIF1 Capture", NULL, "I2SDO Mux"},
	/* DATAO path end */
	{ "DAC", NULL, "I2S Mux"},
	{ "DAC", NULL, "CLK"},
	{ "PGA", NULL, "DAC"},
	{ "BOOST", NULL, "PGA"},
	{ "Speaker", NULL, "BOOST"},
};

static int rt5508_alcfixed_gain_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (!chip->rlr_func)
		return -EINVAL;
	ret = snd_soc_read(codec, RT5508_REG_ALCGAIN);
	if (ret < 0)
		return ret;
	ucontrol->value.integer.value[0] = ret & 0x0f;
	return 0;
}

static int rt5508_alcfixed_gain_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *se = (struct soc_enum *)kcontrol->private_value;
	int orig_pwron = 0, val = 0, ret = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 14, 0))
	if (ucontrol->value.enumerated.item[0] >= se->items)
#else
	if (ucontrol->value.enumerated.item[0] >= se->max)
#endif
		return -EINVAL;
	if (!chip->rlr_func)
		return -EINVAL;
	ret = snd_soc_read(codec, RT5508_REG_CHIPEN);
	if (ret < 0)
		return ret;
	orig_pwron = (ret & RT5508_CHIPPD_ENMASK) ? 0 : 1;
	ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				  RT5508_CHIPPD_ENMASK, ~RT5508_CHIPPD_ENMASK);
	if (ret < 0)
		return ret;
	val = ucontrol->value.enumerated.item[0];
	val += (val << 4);
	ret = snd_soc_write(codec, RT5508_REG_ALCGAIN, val);
	if (ret < 0)
		return ret;
	if (!orig_pwron) {
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
					  RT5508_CHIPPD_ENMASK,
					  RT5508_CHIPPD_ENMASK);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int rt5508_rlrfunc_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = chip->rlr_func;
	return 0;
}

static int rt5508_rlrfunc_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *se = (struct soc_enum *)kcontrol->private_value;
	int orig_pwron = 0, orig_proton = 0, ret = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 14, 0))
	if (ucontrol->value.enumerated.item[0] >= se->items)
#else
	if (ucontrol->value.enumerated.item[0] >= se->max)
#endif
		return -EINVAL;
	if (ucontrol->value.enumerated.item[0] == chip->rlr_func)
		return 0;
	ret = snd_soc_read(codec, RT5508_REG_CHIPEN);
	if (ret < 0)
		return ret;
	orig_pwron = (ret & RT5508_CHIPPD_ENMASK) ? 0 : 1;
	orig_proton = (ret & RT5508_SPKPROT_ENMASK) ? 1 : 0;
	ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				  RT5508_CHIPPD_ENMASK | RT5508_SPKPROT_ENMASK,
				  0);
	if (ret < 0)
		return ret;
	if (ucontrol->value.enumerated.item[0]) {
		ret = snd_soc_write(codec, RT5508_REG_FUNCEN, 0x32);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_FUNCEN, 0x3f);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_FUNCEN, 0x32);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_NDELAY);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_NDELAY, ret + 0x128f5c);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_ALCGAIN, 0x00);
		if (ret < 0)
			return ret;
	} else {
		ret = snd_soc_write(codec, RT5508_REG_FUNCEN, 0x3F);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_NDELAY);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_NDELAY, ret - 0x128f5c);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_ALCGAIN, 0x70);
		if (ret < 0)
			return ret;
	}
	if (orig_proton) {
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
					  RT5508_SPKPROT_ENMASK, 0xff);
		if (ret < 0)
			return ret;
	}
	if (!orig_pwron) {
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
					  RT5508_CHIPPD_ENMASK,
					  RT5508_CHIPPD_ENMASK);
		if (ret < 0)
			return ret;
	}
	chip->rlr_func = ucontrol->value.enumerated.item[0];
	return 0;
}

static int rt5508_recv_config_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = chip->recv_spec_set;
	return 0;
}

static int rt5508_recv_config_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *se = (struct soc_enum *)kcontrol->private_value;
	int orig_pwron = 0, ret = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 14, 0))
	if (ucontrol->value.enumerated.item[0] >= se->items)
#else
	if (ucontrol->value.enumerated.item[0] >= se->max)
#endif
		return -EINVAL;
	if (ucontrol->value.enumerated.item[0] == chip->recv_spec_set)
		return 0;
	ret = snd_soc_read(codec, RT5508_REG_CHIPEN);
	if (ret < 0)
		return ret;
	orig_pwron = (ret & RT5508_CHIPPD_ENMASK) ? 0 : 1;
	ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				  RT5508_CHIPPD_ENMASK, ~RT5508_CHIPPD_ENMASK);
	if (ret < 0)
		return ret;
	if (ucontrol->value.enumerated.item[0]) {
		ret = snd_soc_write(codec, RT5508_REG_CLIP_SIGMAX, 0x7fff);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_ISENSE_CTRL,
					  0x80, 0x00);
		if (ret < 0)
			return ret;
	} else {
		ret = snd_soc_update_bits(codec, RT5508_REG_ISENSE_CTRL,
					  0x80, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_CLIP_SIGMAX, 0x6c00);
		if (ret < 0)
			return ret;
	}
	if (!orig_pwron) {
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
					  RT5508_CHIPPD_ENMASK,
					  RT5508_CHIPPD_ENMASK);
		if (ret < 0)
			return ret;
	}
	chip->recv_spec_set = ucontrol->value.enumerated.item[0];
	return 0;
}

static int rt5508_bypassdsp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = chip->bypass_dsp;
	return 0;
}

static int rt5508_bypassdsp_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *se = (struct soc_enum *)kcontrol->private_value;
	int orig_pwron = 0, ret = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 14, 0))
	if (ucontrol->value.enumerated.item[0] >= se->items)
#else
	if (ucontrol->value.enumerated.item[0] >= se->max)
#endif
		return -EINVAL;
	if (ucontrol->value.enumerated.item[0] == chip->bypass_dsp)
		return 0;
	ret = snd_soc_read(codec, RT5508_REG_CHIPEN);
	if (ret < 0)
		return ret;
	orig_pwron = (ret & RT5508_CHIPPD_ENMASK) ? 0 : 1;
	ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				  RT5508_CHIPPD_ENMASK, ~RT5508_CHIPPD_ENMASK);
	if (ret < 0)
		return ret;
	if (ucontrol->value.enumerated.item[0]) {
		ret = snd_soc_read(codec, RT5508_REG_FUNCEN);
		if (ret < 0)
			return ret;
		chip->func_en = ret;
		ret = snd_soc_read(codec, RT5508_REG_CHIPEN);
		if (ret < 0)
			return ret;
		chip->spk_prot_en = ret & RT5508_SPKPROT_ENMASK;
		ret = snd_soc_write(codec, RT5508_REG_FUNCEN, 0);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
					  RT5508_SPKPROT_ENMASK, 0);
		if (ret < 0)
			return ret;
	} else {
		ret = snd_soc_write(codec, RT5508_REG_FUNCEN, chip->func_en);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_FUNCEN, 0);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_FUNCEN, chip->func_en);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
					  RT5508_SPKPROT_ENMASK,
					  chip->spk_prot_en);
		if (ret < 0)
			return ret;
	}
	if (!orig_pwron) {
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
					  RT5508_CHIPPD_ENMASK,
					  RT5508_CHIPPD_ENMASK);
		if (ret < 0)
			return ret;
	}
	chip->bypass_dsp = ucontrol->value.enumerated.item[0];
	return 0;
}

static int rt5508_put_spk_volsw(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#endif
	int orig_pwron = 0, ret = 0;

	orig_pwron = (dapm->bias_level == SND_SOC_BIAS_OFF) ? 0 : 1;
	if (!orig_pwron) {
		ret = rt5508_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
		if (ret < 0)
			return ret;
	}
	ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret < 0)
		return ret;
	if (!orig_pwron) {
		ret = rt5508_set_bias_level(codec, SND_SOC_BIAS_OFF);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int rt5508_tdm_slots_put(struct snd_kcontrol *kcontrol,
			        struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 15, 0))
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
#else
	struct snd_soc_codec *codec = kcontrol->private_data;
#endif
	struct soc_enum *se = (struct soc_enum *)kcontrol->private_value;
	int orig_pwron = 0, ret = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 14, 0))
	if (ucontrol->value.enumerated.item[0] >= se->items)
#else
	if (ucontrol->value.enumerated.item[0] >= se->max)
#endif
		return -EINVAL;
	ret = snd_soc_read(codec, RT5508_REG_CHIPEN);
	if (ret < 0)
		return ret;
	orig_pwron = (ret & RT5508_CHIPPD_ENMASK) ? 0 : 1;
	ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				  RT5508_CHIPPD_ENMASK, ~RT5508_CHIPPD_ENMASK);
	if (ret < 0)
		return ret;
	ret = snd_soc_put_enum_double(kcontrol, ucontrol);
	if (ret < 0)
		return ret;
	if (!orig_pwron) {
		ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
					  RT5508_CHIPPD_ENMASK,
					  RT5508_CHIPPD_ENMASK);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static const DECLARE_TLV_DB_SCALE(dacvol_tlv, -1275, 5, 0);
static const DECLARE_TLV_DB_SCALE(boostvol_tlv, 0, 3, 0);
static const DECLARE_TLV_DB_SCALE(postpgavol_tlv, -31, 1, 0);
static const DECLARE_TLV_DB_SCALE(prepgavol_tlv, -6, 3, 0);
static const char * const rt5508_enable_text[] = { "Disable", "Enable"};
static const char * const rt5508_slots_text[] = { "Slot 0", "Slot 1"};
static const char * const rt5508_alcgain_text[] = {
	"0dB", "3dB", "6dB", "9dB", "12dB", "15dB", "18dB", "21dB" };
static const struct soc_enum rt5508_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rt5508_enable_text), rt5508_enable_text),
	SOC_ENUM_SINGLE(RT5508_REG_TDM_CTRL, 2, ARRAY_SIZE(rt5508_slots_text),
		rt5508_slots_text),
	SOC_ENUM_SINGLE(RT5508_REG_TDM_CTRL, 1, ARRAY_SIZE(rt5508_slots_text),
		rt5508_slots_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rt5508_alcgain_text),
		rt5508_alcgain_text),
};
static const struct snd_kcontrol_new rt5508_controls[] = {
	SOC_SINGLE_EXT_TLV("DAC Volume", RT5508_REG_VOLUME, 0, 255, 1,
		snd_soc_get_volsw, rt5508_put_spk_volsw, dacvol_tlv),
	SOC_SINGLE_EXT_TLV("Boost Volume", RT5508_REG_SPKGAIN, RT5508_BSTGAIN_SHFT,
		5, 0, snd_soc_get_volsw, rt5508_put_spk_volsw, boostvol_tlv),
	SOC_SINGLE_EXT_TLV("PostPGA Volume", RT5508_REG_SPKGAIN,
		RT5508_POSTPGAGAIN_SHFT, 31, 0, snd_soc_get_volsw,
		rt5508_put_spk_volsw, postpgavol_tlv),
	SOC_SINGLE_EXT_TLV("PrePGA Volume", RT5508_REG_DSPKCONF1,
		RT5508_PREPGAGAIN_SHFT, 3, 0, snd_soc_get_volsw,
		rt5508_put_spk_volsw, prepgavol_tlv),
	SOC_SINGLE_EXT("Speaker Protection", RT5508_REG_CHIPEN, RT5508_SPKPROT_ENSHFT,
		1, 0, snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("Limiter Func", RT5508_REG_FUNCEN, RT5508_LMTEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("ALC Func", RT5508_REG_FUNCEN, RT5508_ALCEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("MBDRC Func", RT5508_REG_FUNCEN, RT5508_MBDRCEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("BWE Func", RT5508_REG_FUNCEN, RT5508_BEWEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("HPF Func", RT5508_REG_FUNCEN, RT5508_HPFEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("BQ Func", RT5508_REG_FUNCEN, RT5508_BQEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("CLIP Func", RT5508_REG_CLIP_CTRL, RT5508_CLIPEN_SHFT, 1, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("BoostMode", RT5508_REG_BST_MODE, RT5508_BSTMODE_SHFT, 3, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("I2S_Channel", RT5508_REG_I2SSEL, RT5508_I2SLRSEL_SHFT, 3, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("Ext_DO_Enable", RT5508_REG_I2SDOSEL, 0, 1, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("I2SDOL Mux", RT5508_REG_I2SDOLRSEL, 0, 15, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_SINGLE_EXT("I2SDOR Mux", RT5508_REG_I2SDOLRSEL, 4, 15, 0,
		snd_soc_get_volsw, rt5508_put_spk_volsw),
	SOC_ENUM_EXT("BypassDSP", rt5508_enum[0], rt5508_bypassdsp_get,
		rt5508_bypassdsp_put),
	SOC_ENUM_EXT("Recv_Special_Set", rt5508_enum[0], rt5508_recv_config_get,
		rt5508_recv_config_put),
	SOC_ENUM_EXT("RLR Func", rt5508_enum[0], rt5508_rlrfunc_get,
		rt5508_rlrfunc_put),
	SOC_ENUM_EXT("TDM_ADC_SEL", rt5508_enum[1], snd_soc_get_enum_double,
		rt5508_tdm_slots_put),
	SOC_ENUM_EXT("TDM_DAC_SEL", rt5508_enum[2], snd_soc_get_enum_double,
		rt5508_tdm_slots_put),
	SOC_ENUM_EXT("ALC Fixed Gain", rt5508_enum[3], rt5508_alcfixed_gain_get,
		rt5508_alcfixed_gain_put),
};

static const struct snd_soc_codec_driver rt5508_codec_drv = {
	.probe = rt5508_codec_probe,
	.remove = rt5508_codec_remove,
	.suspend = rt5508_codec_suspend,
	.resume = rt5508_codec_resume,

	.controls = rt5508_controls,
	.num_controls = ARRAY_SIZE(rt5508_controls),
	.dapm_widgets = rt5508_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5508_dapm_widgets),
	.dapm_routes = rt5508_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5508_dapm_routes),

	.set_bias_level = rt5508_set_bias_level,
	.idle_bias_off = true,
	/* codec io */
	.read = rt5508_io_read,
	.write = rt5508_io_write,
};

static int rt5508_aif_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(dai->codec);
	bool action = 1;
	int ret = 0;

	dev_info(dai->dev, "%s: mute %d\n", __func__, mute);
	if (chip->playback_active != dai->playback_active) {
		chip->playback_active = dai->playback_active;
		action = 1;
	}
	if ((mute && dai->playback_active) && action)
		action = 0;
	if (action) {
		ret =  snd_soc_update_bits(dai->codec, RT5508_REG_CHIPEN,
			RT5508_SPKMUTE_ENMASK,
			mute ? RT5508_SPKMUTE_ENMASK : ~RT5508_SPKMUTE_ENMASK);
	}
	return ret;
}

static int rt5508_aif_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 regval = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s: fmt:%d\n", __func__, fmt);
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		regval |= (RT5508_AUDFMT_I2S << RT5508_AUDFMT_SHFT);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		regval |= (RT5508_AUDFMT_RIGHTJ << RT5508_AUDFMT_SHFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		regval |= (RT5508_AUDFMT_LEFTJ << RT5508_AUDFMT_SHFT);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		regval |= (RT5508_DSP_MODEA << RT5508_DSPMODE_SHFT);
		break;
	case SND_SOC_DAIFMT_DSP_B:
		regval |= (RT5508_DSP_MODEB << RT5508_DSPMODE_SHFT);
		break;
	default:
		break;
	}
	ret = snd_soc_update_bits(dai->codec, RT5508_REG_AUDFMT,
			RT5508_DSPMODE_MASK | RT5508_AUDFMT_MASK, regval);
	if (ret < 0)
		dev_err(dai->dev, "config dac audfmt error\n");
	return ret;
}

static int rt5508_aif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(dai->codec);
	/* 0 for sr and bckfs, 1 for audbits */
	u8 regval[2] = {0};
	u32 pll_divider = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s\n", __func__);
	dev_dbg(dai->dev, "format 0x%08x\n", params_format(hw_params));
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
	case SNDRV_PCM_FORMAT_U16:
		regval[0] |= (RT5508_BCKMODE_32FS << RT5508_BCKMODE_SHFT);
		regval[1] |= (RT5508_AUDBIT_16 << RT5508_AUDBIT_SHFT);
		pll_divider = 0x00100000;
		break;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_U18_3LE:
	case SNDRV_PCM_FORMAT_S18_3BE:
	case SNDRV_PCM_FORMAT_U18_3BE:
		regval[0] |= (RT5508_BCKMODE_48FS << RT5508_BCKMODE_SHFT);
		regval[1] |= (RT5508_AUDBIT_18 << RT5508_AUDBIT_SHFT);
		pll_divider = 0x000c0000;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_U20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_U20_3BE:
		regval[0] |= (RT5508_BCKMODE_48FS << RT5508_BCKMODE_SHFT);
		regval[1] |= (RT5508_AUDBIT_20 << RT5508_AUDBIT_SHFT);
		pll_divider = 0x000c0000;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_U24_3BE:
		regval[0] |= (RT5508_BCKMODE_48FS << RT5508_BCKMODE_SHFT);
		regval[1] |= (RT5508_AUDBIT_24 << RT5508_AUDBIT_SHFT);
		pll_divider = 0x000c0000;
		break;
	case SNDRV_PCM_FORMAT_S32:
	case SNDRV_PCM_FORMAT_U32:
		regval[0] |= (RT5508_BCKMODE_64FS << RT5508_BCKMODE_SHFT);
		regval[1] |= (RT5508_AUDBIT_24 << RT5508_AUDBIT_SHFT);
		pll_divider = 0x00080000;
		break;
	default:
		ret = -EINVAL;
		goto out_hw_params;
	}
	dev_dbg(dai->dev, "rate %d\n", params_rate(hw_params));
	switch (params_rate(hw_params)) {
	case 8000:
		regval[0] |= (RT5508_SRMODE_8K << RT5508_SRMODE_SHFT);
		pll_divider *= 6;
		break;
	case 11025:
	case 12000:
		regval[0] |= (RT5508_SRMODE_12K << RT5508_SRMODE_SHFT);
		pll_divider *= 4;
		break;
	case 16000:
		regval[0] |= (RT5508_SRMODE_16K << RT5508_SRMODE_SHFT);
		pll_divider *= 3;
		break;
	case 22050:
	case 24000:
		regval[0] |= (RT5508_SRMODE_24K << RT5508_SRMODE_SHFT);
		pll_divider *= 2;
		break;
	case 32000:
		regval[0] |= (RT5508_SRMODE_32K << RT5508_SRMODE_SHFT);
		pll_divider = (pll_divider * 3) >> 1;
		break;
	case 44100:
	case 48000:
		regval[0] |= (RT5508_SRMODE_48K << RT5508_SRMODE_SHFT);
		break;
	case 88200:
	case 96000:
		regval[0] |= (RT5508_SRMODE_96K << RT5508_SRMODE_SHFT);
		pll_divider >>= 1;
		break;
	case 176400:
	case 192000:
		regval[0] |= (RT5508_SRMODE_192K << RT5508_SRMODE_SHFT);
		pll_divider >>= 2;
		break;
	default:
		ret = -EINVAL;
		goto out_hw_params;
	}
	if (chip->tdm_mode)
		pll_divider >>= 1;
	ret = snd_soc_update_bits(dai->codec, RT5508_REG_AUDSR,
			RT5508_BCKMODE_MASK | RT5508_SRMODE_MASK, regval[0]);
	if (ret < 0) {
		dev_err(dai->dev, "configure bck and sr fail\n");
		goto out_hw_params;
	}
	ret = snd_soc_update_bits(dai->codec, RT5508_REG_AUDFMT,
			RT5508_AUDBIT_MASK, regval[1]);
	if (ret < 0) {
		dev_err(dai->dev, "configure audbit fail\n");
		goto out_hw_params;
	}
	ret = snd_soc_write(dai->codec, RT5508_REG_PLLDIVISOR, pll_divider);
	if (ret < 0)
		dev_err(dai->dev, "configure pll divider fail\n");
out_hw_params:
	return ret;
}

static int rt5508_aif_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
	return rt5508_set_bias_level(dai->codec, SND_SOC_BIAS_STANDBY);
}

static void rt5508_aif_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
}

static int rt5508_aif_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

	dev_dbg(dai->dev, "%s: cmd=%d\n", __func__, cmd);
	dev_dbg(dai->dev, "%s: %c\n", __func__, capture ? 'c' : 'p');
	return 0;
}

static int rt5508_aif_set_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	struct rt5508_chip *chip = snd_soc_codec_get_drvdata(dai->codec);

	dev_dbg(dai->dev, "%s: slots %d\n", __func__, slots);
	if (!slots) {
		dev_dbg(dai->dev, "disable TDM\n");
		chip->tdm_mode = 0;
	} else if (slots == 4) {
		dev_dbg(dai->dev, "enable TDM\n");
		chip->tdm_mode = 1;
	} else
		return -EINVAL;
	return snd_soc_update_bits(dai->codec, RT5508_REG_TDM_CTRL,
				   RT5508_TDM_ENMASK,
				   chip->tdm_mode ? 0xff : 0);
}

static const struct snd_soc_dai_ops rt5508_dai_ops = {
	.set_fmt = rt5508_aif_set_fmt,
	.hw_params = rt5508_aif_hw_params,
	.digital_mute = rt5508_aif_digital_mute,
	.startup = rt5508_aif_startup,
	.shutdown = rt5508_aif_shutdown,
	.trigger = rt5508_aif_trigger,
	.set_tdm_slot = rt5508_aif_set_tdm_slot,
};

#define RT5508_RATES SNDRV_PCM_RATE_8000_192000
#define RT5508_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S18_3LE |\
	SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_3LE |\
	SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver rt5508_i2s_dais[] = {
	{
		.name = "rt5508-aif1",
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5508_RATES,
			.formats = RT5508_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5508_RATES,
			.formats = RT5508_FORMATS,
		},
		.ops = &rt5508_dai_ops,
	},
	{
		.name = "rt5508-aif2",
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5508_RATES,
			.formats = RT5508_FORMATS,
		},
		.ops = &rt5508_dai_ops,
	},
};

static inline int rt5508_codec_register(struct rt5508_chip *chip)
{
	return snd_soc_register_codec(chip->dev, &rt5508_codec_drv,
		rt5508_i2s_dais, ARRAY_SIZE(rt5508_i2s_dais));
}

static inline int rt5508_codec_unregister(struct rt5508_chip *chip)
{
	snd_soc_unregister_codec(chip->dev);
	return 0;
}

static int rt5508_handle_pdata(struct rt5508_chip *chip)
{
	return 0;
}

static int rt5508_i2c_initreg(struct rt5508_chip *chip)
{
	int ret = 0;

	/* mute first, before codec_probe register init */
	ret = rt5508_set_bits(chip->i2c,
		RT5508_REG_CHIPEN, RT5508_SPKMUTE_ENMASK);
	if (ret < 0)
		goto out_init_reg;
	/* disable TriWave */
	ret = rt5508_clr_bits(chip->i2c, RT5508_REG_CHIPEN,
		RT5508_TRIWAVE_ENMASK);
	if (ret < 0)
		goto out_init_reg;
	/* disable all digital clock */
	ret = rt5508_clr_bits(chip->i2c, RT5508_REG_CLKEN1,
		RT5508_CLKEN1_MASK);
	if (ret < 0)
		goto out_init_reg;
	ret = rt5508_clr_bits(chip->i2c, RT5508_REG_CLKEN2,
		RT5508_CLKEN2_MASK);
out_init_reg:
	return ret;
}

static int rt5508_get_chip_rev(struct rt5508_chip *chip)
{
	int ret = 0;
	u8 data = 0;

	ret = rt5508_block_read(chip->i2c, RT5508_REG_CHIPREV, 1, &data);
	if (ret < 0)
		return ret;
	if ((data & RT5508_CHIPID_MASK) != RT5508_CHIP_ID)
		return -ENODEV;
	chip->chip_rev = (data & RT5508_CHIPREV_MASK) >> RT5508_CHIPREV_SHFT;
	dev_dbg(chip->dev, "chip revision %d\n", chip->chip_rev);
	return 0;
}

static int rt5508_sw_reset(struct rt5508_chip *chip)
{
	int ret = 0;
	u8 data = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	ret = rt5508_block_read(chip->i2c, RT5508_REG_SWRESET, 1, &data);
	if (ret < 0)
		return ret;
	data |= RT5508_SWRST_MASK;
	ret = rt5508_block_write(chip->i2c, RT5508_REG_SWRESET, 1, &data);
	mdelay(30);
	return ret;
}

static inline int _rt5508_power_on(struct rt5508_chip *chip, bool en)
{
	int ret = 0;
	u8 data = 0;

	dev_dbg(chip->dev, "%s: en %d\n", __func__, en);
	ret = rt5508_block_read(chip->i2c, RT5508_REG_CHIPEN, 1, &data);
	if (ret < 0)
		return ret;
	data = (en ? (data & ~0x01) : (data | 0x01));
	return rt5508_block_write(chip->i2c, RT5508_REG_CHIPEN, 1, &data);
}

#ifdef CONFIG_OF
static inline int rt5508_parse_dt(struct device *dev,
				  struct rt5508_pdata *pdata)
{
	struct device_node *param_np;
	struct property *prop;
	struct rt5508_proprietary_param *p_param;
	u32 len = 0;
	int i = 0;

	if (of_property_read_u32(dev->of_node,
				 "rt,chan_sel", &pdata->chan_sel) < 0) {
		/* if no default channel, select to (L+R)/2 default */
		pdata->chan_sel = 1;
	}
	if (of_property_read_bool(dev->of_node, "rt,do_enable"))
		pdata->do_enable = true;
	param_np = of_find_node_by_name(dev->of_node, "proprietary_param");
	if (!param_np)
		goto OUT_PARSE_DT;
	p_param = devm_kzalloc(dev, sizeof(*p_param), GFP_KERNEL);
	if (!p_param)
		return -ENOMEM;
	for (i = 0; i < RT5508_CFG_MAX; i++) {
		prop = of_find_property(param_np, prop_str[i], &len);
		if (!prop)
			dev_warn(dev, "no %s setting\n", prop_str[i]);
		else if (!len)
			dev_warn(dev, "%s cfg size is zero\n", prop_str[i]);
		else {
			p_param->cfg[i] = devm_kzalloc(dev, len * sizeof(u8),
						     GFP_KERNEL);
			if (!p_param->cfg[i]) {
				dev_err(dev, "alloc %s fail\n", prop_str[i]);
				return -ENOMEM;
			}
			memcpy(p_param->cfg[i], prop->value, len);
			p_param->cfg_size[i] = len;
		}
	}
	pdata->p_param = p_param;
OUT_PARSE_DT:
	return 0;
}
#else
static inline int rt5508_parse_dt(struct device *dev,
				  struct rt5508_pdata *pdata)
{
	return 0;
}
#endif /* #ifdef CONFIG_OF */

int rt5508_calib_trigger_reset(struct rt5508_chip *chip)
{
	int ret = 0;

	dev_info(chip->dev, "%s begin\n", __func__);
	/* do software reset at default */
	ret = rt5508_sw_reset(chip);
	if (ret < 0) {
		dev_err(chip->dev, "sw_reset fail\n");
		goto calib_trigger_reset;
	}
	ret = _rt5508_power_on(chip, true);
	if (ret < 0) {
		dev_err(chip->dev, "power on fail\n");
		goto calib_trigger_reset;
	}
#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_cache_reload(chip->rd);
	if (ret < 0) {
		dev_err(chip->dev, "cache reload fail\n");
		goto calib_trigger_reset;
	}
#endif /* #ifdef CONFIG_RT_REGMAP */
	ret = rt5508_i2c_initreg(chip);
	if (ret < 0) {
		dev_err(chip->dev, "init_reg fail\n");
		goto calib_trigger_reset;
	}
	ret = rt5508_init_general_setting(chip->codec);
	if (ret < 0)
		goto calib_trigger_reset;
	ret = rt5508_init_adaptive_setting(chip->codec);
	if (ret < 0)
		goto calib_trigger_reset;
	ret = rt5508_init_battmode_setting(chip->codec);
	if (ret < 0)
		goto calib_trigger_reset;
	ret = rt5508_init_proprietary_setting(chip->codec);
	if (ret < 0)
		goto calib_trigger_reset;
	ret = rt5508_do_tcsense_fix(chip->codec);
	if (ret < 0) {
		dev_err(chip->dev, "do tcsense fix fail\n");
		goto calib_trigger_reset;
	}
	dev_info(chip->dev, "%s end\n", __func__);
	return 0;
calib_trigger_reset:
	return ret;
}
EXPORT_SYMBOL(rt5508_calib_trigger_reset);

static int rt5508_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct rt5508_pdata *pdata = client->dev.platform_data;
	struct rt5508_chip *chip;
	int ret = 0;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = rt5508_parse_dt(&client->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
		client->dev.platform_data = pdata;
	} else {
		if (!pdata) {
			dev_err(&client->dev, "Failed, no pdata specified\n");
			return -EINVAL;
		}
	}
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Failed, on memory allocation\n");
		goto err_parse_dt;
	}
	chip->i2c = client;
	chip->dev = &client->dev;
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
#if RT5508_SIMULATE_DEVICE
	ret = rt5508_calculate_total_size();
	chip->sim = devm_kzalloc(&client->dev, ret, GFP_KERNEL);
	if (!chip->sim) {
		ret = -ENOMEM;
		goto err_simulate;
	}
#endif /* #if RT5508_SIMULATE_DEVICE */

	sema_init(&chip->io_semaphore, 1);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_set_active(chip->dev);
	pm_runtime_enable(chip->dev);
#else
	atomic_set(&chip->power_count, 1);
#endif /* #ifdef CONFIG_PM_RUNTIME */
	/* before sw_reset, set CHIP_PD = 0 */
	ret = _rt5508_power_on(chip, true);
	if (ret < 0) {
		dev_err(chip->dev, "power on fail\n");
		goto err_sw_reset;
	}
	/* do software reset at default */
	ret = rt5508_sw_reset(chip);
	if (ret < 0) {
		dev_err(chip->dev, "sw_reset fail\n");
		goto err_sw_reset;
	}
	ret = _rt5508_power_on(chip, true);
	if (ret < 0) {
		dev_err(chip->dev, "power on fail\n");
		goto err_pm_init;
	}
	/* get chip revisioin first */
	ret = rt5508_get_chip_rev(chip);
	if (ret < 0) {
		dev_err(chip->dev, "get chip rev fail\n");
		goto err_sw_reset;
	}
	/* register RegMAP */
	chip->rd = rt5508_regmap_register(
		&rt5508_regmap_ops, &client->dev, (void *)client, chip);
	if (!chip->rd) {
		dev_err(chip->dev, "create regmap device fail\n");
		ret = -EINVAL;
		goto err_regmap;
	}
	ret = rt5508_i2c_initreg(chip);
	if (ret < 0) {
		dev_err(chip->dev, "init_reg fail\n");
		goto err_initreg;
	}
	ret = rt5508_handle_pdata(chip);
	if (ret < 0) {
		dev_err(chip->dev, "init_pdata fail\n");
		goto err_pdata;
	}
	ret = rt5508_power_on(chip, false);
	if (ret < 0) {
		dev_err(chip->dev, "power off fail\n");
		goto err_put_sync;
	}
	ret = rt5508_codec_register(chip);
	if (ret < 0) {
		dev_err(chip->dev, "codec register fail\n");
		goto err_put_sync;
	}
	dev_dbg(&client->dev, "successfully driver probed\n");
	return 0;
err_put_sync:
err_pdata:
err_initreg:
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->rd);
#endif /* #ifdef CONFIG_RT_REGMAP */
err_regmap:
err_pm_init:
	_rt5508_power_on(chip, false);
err_sw_reset:
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(chip->dev);
	pm_runtime_set_suspended(chip->dev);
#else
	atomic_dec(&chip->power_count);
#endif /* #ifdef CONFIG_PM_RUNTIME */
#if RT5508_SIMULATE_DEVICE
	devm_kfree(chip->dev, chip->sim);
err_simulate:
#endif /* #if RT5508_SIMULATE_DEVICE */
	devm_kfree(&client->dev, chip);
err_parse_dt:
	if (client->dev.of_node)
		devm_kfree(&client->dev, pdata);
	return ret;
}

static int rt5508_i2c_remove(struct i2c_client *client)
{
	struct rt5508_chip *chip = i2c_get_clientdata(client);

	rt5508_codec_unregister(chip);
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->rd);
#endif /* #ifdef CONFIG_RT_REGMAP */
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(chip->dev);
	pm_runtime_set_suspended(chip->dev);
#else
	atomic_set(&chip->power_count, 0);
#endif /* #ifdef CONFIG_PM_RUNTIME */
	_rt5508_power_on(chip, false);
#if RT5508_SIMULATE_DEVICE
	devm_kfree(chip->dev, chip->sim);
#endif /* #if RT5508_SIMULATE_DEVICE */
	devm_kfree(chip->dev, chip->pdata);
	chip->pdata = client->dev.platform_data = NULL;
	dev_dbg(&client->dev, "driver removed\n");
	return 0;
}

#ifdef CONFIG_PM
static int rt5508_i2c_suspend(struct device *dev)
{
	return 0;
}

static int rt5508_i2c_resume(struct device *dev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int rt5508_i2c_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int rt5508_i2c_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int rt5508_i2c_runtime_idle(struct device *dev)
{
	/* dummy function */
	return 0;
}
#endif /* #ifdef CONFIG_PM_RUNTIME */

static const struct dev_pm_ops rt5508_i2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rt5508_i2c_suspend, rt5508_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(rt5508_i2c_runtime_suspend,
			   rt5508_i2c_runtime_resume,
			   rt5508_i2c_runtime_idle)
#endif /* #ifdef CONFIG_PM_RUNTIME */
};
#define prt5508_i2c_pm_ops (&rt5508_i2c_pm_ops)
#else
#define prt5508_i2c_pm_ops (NULL)
#endif /* #ifdef CONFIG_PM */

static const struct i2c_device_id rt5508_i2c_id[] = {
	{RT5508_DEVICE_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt5508_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id rt5508_match_table[] = {
	{.compatible = "richtek,rt5508",},
	{},
};
MODULE_DEVICE_TABLE(of, rt5508_match_table);
#endif /* #ifdef CONFIG_OF */

static struct i2c_driver rt5508_i2c_driver = {
	.driver = {
		.name = RT5508_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt5508_match_table),
		.pm = prt5508_i2c_pm_ops,
	},
	.probe = rt5508_i2c_probe,
	.remove = rt5508_i2c_remove,
	.id_table = rt5508_i2c_id,
};

module_i2c_driver(rt5508_i2c_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("RT5508 SPKAMP Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RT5508_DRV_VER);
