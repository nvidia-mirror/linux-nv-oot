/*
 * tegra210_dmic_alt.c - Tegra210 DMIC driver
 *
 * Copyright (c) 2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>

#include "tegra210_xbar_alt.h"
#include "tegra210_dmic_alt.h"
#include "ahub_unit_fpga_clock.h"

#define DRV_NAME "tegra210-dmic"

static int tegra210_dmic_runtime_suspend(struct device *dev)
{
	struct tegra210_dmic *dmic = dev_get_drvdata(dev);
	regcache_cache_only(dmic->regmap, true);

#ifndef CONFIG_MACH_GRENADA
	clk_disable_unprepare(dmic->clk_dmic);
#endif
	return 0;
}

static int tegra210_dmic_runtime_resume(struct device *dev)
{
	struct tegra210_dmic *dmic = dev_get_drvdata(dev);

#ifndef CONFIG_MACH_GRENADA
	int ret;

	ret = clk_prepare_enable(dmic->clk_dmic);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}
#endif

	regcache_cache_only(dmic->regmap, false);
	return 0;
}

static int tegra210_dmic_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra210_dmic *dmic = snd_soc_dai_get_drvdata(dai);
	int channels, srate, dmic_clk, osr = TEGRA210_DMIC_OSR_64;
	struct tegra210_xbar_cif_conf cif_conf;

	channels = params_channels(params);
	srate = params_rate(params);
	dmic_clk = (1 << (6+osr)) * (srate/2);

#ifdef CONFIG_MACH_GRENADA
	program_dmic_gpio();
	program_dmic_clk(dmic_clk);
#endif

	regmap_update_bits(dmic->regmap,
				TEGRA210_DMIC_CTRL,
				TEGRA210_DMIC_CTRL_OSR_MASK,
				osr << TEGRA210_DMIC_CTRL_OSR_SHIFT);

	regmap_update_bits(dmic->regmap,
				TEGRA210_DMIC_DBG_CTRL,
				TEGRA210_DMIC_DBG_CTRL_SC_ENABLE,
				TEGRA210_DMIC_DBG_CTRL_SC_ENABLE);

	regmap_update_bits(dmic->regmap,
				TEGRA210_DMIC_CTRL,
				TEGRA210_DMIC_CTRL_CHANNEL_SELECT_MASK,
				((1 << channels) - 1) <<
				   TEGRA210_DMIC_CTRL_CHANNEL_SELECT_SHIFT);

	cif_conf.threshold = 0;
	cif_conf.audio_channels = channels;
	cif_conf.client_channels = channels;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_32;
		break;
	default:
		dev_err(dev, "Wrong format!\n");
		return -EINVAL;
	}

	cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_24;
	cif_conf.expand = 0;
	cif_conf.stereo_conv = 0;
	cif_conf.replicate = 0;
	cif_conf.truncate = 0;
	cif_conf.mono_conv = 0;

	dmic->soc_data->set_audio_cif(dmic->regmap, TEGRA210_DMIC_TX_CIF_CTRL,
		&cif_conf);

	return 0;
}

static int tegra210_dmic_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra210_dmic *dmic = snd_soc_codec_get_drvdata(codec);
	int ret;

	codec->control_data = dmic->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 32, 32, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	return 0;
}

static struct snd_soc_dai_ops tegra210_dmic_dai_ops = {
	.hw_params	= tegra210_dmic_hw_params,
};

static struct snd_soc_dai_driver tegra210_dmic_dais[] = {
	{
		.name = "CIF",
		.capture = {
			.stream_name = "DMIC Transmit",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &tegra210_dmic_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "DAP",
		.playback = {
			.stream_name = "DMIC Receive",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &tegra210_dmic_dai_ops,
		.symmetric_rates = 1,
	}
};

static const struct snd_soc_dapm_widget tegra210_dmic_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("DMIC TX", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN("DMIC RX", NULL, 0, TEGRA210_DMIC_ENABLE,
				TEGRA210_DMIC_ENABLE_EN_SHIFT, 0),
};

static const struct snd_soc_dapm_route tegra210_dmic_routes[] = {
	{ "DMIC RX",       NULL, "DMIC Receive" },
	{ "DMIC TX",       NULL, "DMIC RX" },
	{ "DMIC Transmit", NULL, "DMIC TX" },
};

static struct snd_soc_codec_driver tegra210_dmic_codec = {
	.probe = tegra210_dmic_codec_probe,
	.dapm_widgets = tegra210_dmic_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_dmic_widgets),
	.dapm_routes = tegra210_dmic_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra210_dmic_routes),
};

/* Regmap callback functions */
static bool tegra210_dmic_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_DMIC_TX_INT_MASK:
	case TEGRA210_DMIC_TX_INT_SET:
	case TEGRA210_DMIC_TX_INT_CLEAR:
	case TEGRA210_DMIC_TX_CIF_CTRL:

	case TEGRA210_DMIC_ENABLE:
	case TEGRA210_DMIC_SOFT_RESET:
	case TEGRA210_DMIC_CG:
	case TEGRA210_DMIC_CTRL:
		return true;
	default:
		if (((reg % 4) == 0) && (reg >= TEGRA210_DMIC_DBG_CTRL) &&
		    (reg <= TEGRA210_DMIC_CORRECTION_BIQUAD_1_COEF_4))
			return true;
		else
			return false;
	};
}

static bool tegra210_dmic_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_DMIC_TX_STATUS:
	case TEGRA210_DMIC_TX_INT_STATUS:
	case TEGRA210_DMIC_TX_INT_MASK:
	case TEGRA210_DMIC_TX_INT_SET:
	case TEGRA210_DMIC_TX_INT_CLEAR:
	case TEGRA210_DMIC_TX_CIF_CTRL:

	case TEGRA210_DMIC_ENABLE:
	case TEGRA210_DMIC_SOFT_RESET:
	case TEGRA210_DMIC_CG:
	case TEGRA210_DMIC_STATUS:
	case TEGRA210_DMIC_INT_STATUS:
	case TEGRA210_DMIC_CTRL:
		return true;
	default:
		if (((reg % 4) == 0) && (reg >= TEGRA210_DMIC_DBG_CTRL) &&
		    (reg <= TEGRA210_DMIC_CORRECTION_BIQUAD_1_COEF_4))
			return true;
		else
			return false;
	};
}

static bool tegra210_dmic_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_DMIC_TX_STATUS:
	case TEGRA210_DMIC_TX_INT_STATUS:
	case TEGRA210_DMIC_TX_INT_SET:

	case TEGRA210_DMIC_SOFT_RESET:
	case TEGRA210_DMIC_STATUS:
	case TEGRA210_DMIC_INT_STATUS:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra210_dmic_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_DMIC_CORRECTION_BIQUAD_1_COEF_4,
	.writeable_reg = tegra210_dmic_wr_reg,
	.readable_reg = tegra210_dmic_rd_reg,
	.volatile_reg = tegra210_dmic_volatile_reg,
	.precious_reg = NULL,
	.cache_type = REGCACHE_RBTREE,
};

static const struct tegra210_dmic_soc_data soc_data_tegra210 = {
	.set_audio_cif = tegra210_xbar_set_cif,
};

static const struct of_device_id tegra210_dmic_of_match[] = {
	{ .compatible = "nvidia,tegra210-dmic", .data = &soc_data_tegra210 },
	{},
};

static int tegra210_dmic_platform_probe(struct platform_device *pdev)
{
	struct tegra210_dmic *dmic;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra210_dmic_soc_data *soc_data;

	match = of_match_device(tegra210_dmic_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra210_dmic_soc_data *)match->data;

	dmic = devm_kzalloc(&pdev->dev, sizeof(struct tegra210_dmic), GFP_KERNEL);
	if (!dmic) {
		dev_err(&pdev->dev, "Can't allocate dmic\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, dmic);

	dmic->soc_data = soc_data;

#ifndef CONFIG_MACH_GRENADA
	dmic->clk_dmic = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(dmic->clk_dmic)) {
		dev_err(&pdev->dev, "Can't retrieve dmic clock\n");
		ret = PTR_ERR(dmic->clk_dmic);
		goto err;
	}
#endif

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	memregion = devm_request_mem_region(&pdev->dev, mem->start,
					    resource_size(mem), pdev->name);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_clk_put;
	}

	regs = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_clk_put;
	}

	dmic->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra210_dmic_regmap_config);
	if (IS_ERR(dmic->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(dmic->regmap);
		goto err_clk_put;
	}
	regcache_cache_only(dmic->regmap, true);

	if (of_property_read_u32(pdev->dev.of_node,
				"nvidia,ahub-dmic-id",
				&pdev->dev.id) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,ahub-dmic-id\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra210_dmic_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra210_dmic_codec,
				     tegra210_dmic_dais,
				     ARRAY_SIZE(tegra210_dmic_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_dmic_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_clk_put:
#ifndef CONFIG_MACH_GRENADA
	devm_clk_put(&pdev->dev, dmic->clk_dmic);
#endif
err:
	return ret;
}

static int tegra210_dmic_platform_remove(struct platform_device *pdev)
{
	struct tegra210_dmic *dmic;

	dmic = dev_get_drvdata(&pdev->dev);
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra210_dmic_runtime_suspend(&pdev->dev);

#ifndef CONFIG_MACH_GRENADA
	devm_clk_put(&pdev->dev, dmic->clk_dmic);
#endif
	return 0;
}

static const struct dev_pm_ops tegra210_dmic_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_dmic_runtime_suspend,
			   tegra210_dmic_runtime_resume, NULL)
};

static struct platform_driver tegra210_dmic_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra210_dmic_of_match,
		.pm = &tegra210_dmic_pm_ops,
	},
	.probe = tegra210_dmic_platform_probe,
	.remove = tegra210_dmic_platform_remove,
};
module_platform_driver(tegra210_dmic_driver)

MODULE_AUTHOR("Rahul Mittal <rmittal@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 DMIC ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra210_dmic_of_match);
