# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022-2023, NVIDIA CORPORATION.  All rights reserved.

ifeq ($(findstring ack_src,$(NV_BUILD_KERNEL_OPTIONS)),)
ifneq ($(CONFIG_TEGRA_IVC_LEGACY_DISABLE),y)
obj-m += mods/
endif
endif
obj-m += nvsciipc/
ifdef CONFIG_PCI
obj-m += tegra-pcie-dma-test.o
endif
obj-m += bluedroid_pm.o
obj-m += nvscic2c-pcie/
obj-m += ioctl_example.o
