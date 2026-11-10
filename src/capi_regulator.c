/*
 * Copyright (c) 2025-2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_regulator.c
 * @brief This file contains a set of wrapper functions that provide the
 *        Common API (CAPI) interface for regulator drivers. This standardized
 *        interface allows applications to use common regulator functionality
 *        while device-specific features can still be accessed via the direct
 *        driver API.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <capi_regulator.h>

int capi_regulator_init(struct capi_regulator_handle **handle,
			const struct capi_regulator_config *config)
{
	if (!handle || !config || !config->ops || !config->ops->init) {
		return -EINVAL;
	}
	return config->ops->init(handle, config);
}

int capi_regulator_deinit(struct capi_regulator_handle *handle)
{
	if (!handle || !handle->ops || !handle->ops->deinit) {
		return -EINVAL;
	}
	return handle->ops->deinit(handle);
}

int capi_regulator_enable(const struct capi_regulator_handle *handle)
{
	if (!handle || !handle->ops || !handle->ops->enable) {
		return -EINVAL;
	}
	return handle->ops->enable(handle);
}

int capi_regulator_disable(const struct capi_regulator_handle *handle)
{
	if (!handle || !handle->ops || !handle->ops->disable) {
		return -EINVAL;
	}
	return handle->ops->disable(handle);
}

int capi_regulator_get_status(const struct capi_regulator_handle *handle, void *status)
{
	if (!handle || !handle->ops || !handle->ops->get_status || !status) {
		return -EINVAL;
	}
	return handle->ops->get_status(handle, status);
}

int capi_regulator_set_voltage(const struct capi_regulator_handle *handle,
			       uint32_t voltage_microvolt)
{
	if (!handle || !handle->ops || !handle->ops->set_voltage) {
		return -EINVAL;
	}
	return handle->ops->set_voltage(handle, voltage_microvolt);
}

int capi_regulator_get_voltage(const struct capi_regulator_handle *handle,
			       uint32_t *voltage_microvolt)
{
	if (!handle || !handle->ops || !handle->ops->get_voltage || !voltage_microvolt) {
		return -EINVAL;
	}
	return handle->ops->get_voltage(handle, voltage_microvolt);
}

int capi_regulator_set_mode(const struct capi_regulator_handle *handle,
			    enum capi_regulator_mode mode)
{
	if (!handle || !handle->ops || !handle->ops->set_mode) {
		return -EINVAL;
	}
	return handle->ops->set_mode(handle, mode);
}

int capi_regulator_get_mode(const struct capi_regulator_handle *handle,
			    enum capi_regulator_mode *mode)
{
	if (!handle || !handle->ops || !handle->ops->get_mode || !mode) {
		return -EINVAL;
	}
	return handle->ops->get_mode(handle, mode);
}

int capi_regulator_set_current_limit(const struct capi_regulator_handle *handle,
				     uint32_t current_limit_microamp)
{
	if (!handle || !handle->ops || !handle->ops->set_current_limit) {
		return -EINVAL;
	}
	return handle->ops->set_current_limit(handle, current_limit_microamp);
}

int capi_regulator_get_current_limit(const struct capi_regulator_handle *handle,
				     uint32_t *current_limit_microamp)
{
	if (!handle || !handle->ops || !handle->ops->get_current_limit || !current_limit_microamp) {
		return -EINVAL;
	}
	return handle->ops->get_current_limit(handle, current_limit_microamp);
}

int capi_regulator_set_active_discharge(const struct capi_regulator_handle *handle, bool enabled)
{
	if (!handle || !handle->ops || !handle->ops->set_active_discharge) {
		return -EINVAL;
	}
	return handle->ops->set_active_discharge(handle, enabled);
}

int capi_regulator_get_active_discharge(const struct capi_regulator_handle *handle, bool *enabled)
{
	if (!handle || !handle->ops || !handle->ops->get_active_discharge || !enabled) {
		return -EINVAL;
	}
	return handle->ops->get_active_discharge(handle, enabled);
}
