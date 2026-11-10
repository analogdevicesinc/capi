/*
 * Copyright (c) 2024-2025 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_pinctrl.c
 * @brief This file contains a set of small wrapper functions that can be used to access
 *        the PINCTRL driver via its ops pointer structure. Note, it does not
 *        implement any thread safety such as mutually excluding calls to the PINCTRL functions.
 *        If this is needed, it is suggested that this file and all of the other driver
 *        wrappers be copied and enhanced in your project.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <capi_pinctrl.h>

int capi_pinctrl_init(struct capi_pinctrl_handle **handle, const struct capi_pinctrl_config *config)
{
	if (!handle || !config || !config->ops || !config->ops->pinctrl_init) {
		return -EINVAL;
	}
	return config->ops->pinctrl_init(handle, config);
}

int capi_pinctrl_deinit(struct capi_pinctrl_handle **handle)
{
	if (!handle || !*handle || !(*handle)->ops || !(*handle)->ops->pinctrl_deinit) {
		return -EINVAL;
	}
	return (*handle)->ops->pinctrl_deinit(handle);
}

int capi_pinctrl_set_pin(const struct capi_pinctrl_handle *handle,
			 const struct capi_pinctrl_descriptor *pin)
{
	if (!handle || !handle->ops || !handle->ops->pinctrl_set_pin) {
		return -EINVAL;
	}
	return handle->ops->pinctrl_set_pin(handle, pin);
}

int capi_pinctrl_set_pins(const struct capi_pinctrl_handle *handle,
			  const struct capi_pinctrl_descriptor *pins, uint32_t num_pins)
{
	if (!handle || !handle->ops || !handle->ops->pinctrl_set_pins) {
		return -EINVAL;
	}
	return handle->ops->pinctrl_set_pins(handle, pins, num_pins);
}
