// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022, NVIDIA CORPORATION, All rights reserved.

#ifndef _INCLUDE_MACH_ISO_CLIENT_H
#define _INCLUDE_MACH_ISO_CLIENT_H

enum tegra_iso_client {
	TEGRA_ISO_CLIENT_DISP_0,
	TEGRA_ISO_CLIENT_DISP_1,
	TEGRA_ISO_CLIENT_DISP_2,
	TEGRA_ISO_CLIENT_VI_0,
	TEGRA_ISO_CLIENT_VI_1,
	TEGRA_ISO_CLIENT_ISP_A,
	TEGRA_ISO_CLIENT_ISP_B,
	TEGRA_ISO_CLIENT_BBC_0,
	TEGRA_ISO_CLIENT_TEGRA_CAMERA,
	TEGRA_ISO_CLIENT_APE_ADMA,
	TEGRA_ISO_CLIENT_EQOS,
	TEGRA_ISO_CLIENT_COUNT
};

#endif /* _INCLUDE_MACH_ISO_CLIENT_H */