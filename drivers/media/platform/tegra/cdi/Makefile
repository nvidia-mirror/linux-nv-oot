# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

obj-m	+= cdi_gpio.o
obj-m	+= cdi_mgr.o
obj-m	+= cdi_dev.o
obj-m	+= cdi_pwm.o
ifeq ($(findstring ack_src,$(NV_BUILD_KERNEL_OPTIONS)),)
obj-m   += cam_cdi_tsc.o
endif
