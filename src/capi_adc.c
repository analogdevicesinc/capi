/*
 * Copyright (c) 2024-2025 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_adc.c
 * @brief This file contains a set of small wrapper functions that can be used to access
 *        the ADC driver via its ops pointer structure. Note, it does not
 *        implement any thread safety such as mutually excluding calls to the ADC functions.
 *        If this is needed, it is suggested that this file and all of the other driver
 *        wrappers be copied and enhanced in your project.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <capi_adc.h>

int capi_adc_init(struct capi_adc_handle **handle, struct capi_adc_config *config)
{
	if (!handle || !config || !config->adc_ops || !config->adc_ops->init) {
		return -EINVAL;
	}
	return config->adc_ops->init(handle, config);
}

int capi_adc_deinit(struct capi_adc_handle *handle)
{
	if (!handle || !handle->adc_ops || !handle->adc_ops->deinit) {
		return -EINVAL;
	}
	return handle->adc_ops->deinit(handle);
}

int capi_adc_power_on(struct capi_adc_handle *handle)
{
	if (!handle || !handle->adc_ops || !handle->adc_ops->power_on) {
		return -EINVAL;
	}
	return handle->adc_ops->power_on(handle);
}

int capi_adc_power_off(struct capi_adc_handle *handle)
{
	if (!handle || !handle->adc_ops || !handle->adc_ops->power_off) {
		return -EINVAL;
	}
	return handle->adc_ops->power_off(handle);
}

int capi_adc_is_power_on(struct capi_adc_handle *handle, bool *is_on)
{
	if (!handle || !handle->adc_ops || !handle->adc_ops->is_power_on) {
		return -EINVAL;
	}
	return handle->adc_ops->is_power_on(handle, is_on);
}

int capi_adc_set_vref_source(struct capi_adc_handle *handle, enum capi_adc_vref_src src)
{
	if (!handle || !handle->adc_ops || !handle->adc_ops->set_vref_source) {
		return -EINVAL;
	}
	return handle->adc_ops->set_vref_source(handle, src);
}

int capi_adc_get_vref_source(struct capi_adc_handle *handle, enum capi_adc_vref_src *src)
{
	if (!handle || !handle->adc_ops || !handle->adc_ops->get_vref_source) {
		return -EINVAL;
	}
	return handle->adc_ops->get_vref_source(handle, src);
}

int capi_adc_set_calibration(struct capi_adc_handle *handle, uint8_t *cal_data)
{
	if (!handle || !handle->adc_ops || !handle->adc_ops->set_calibration) {
		return -EINVAL;
	}
	return handle->adc_ops->set_calibration(handle, cal_data);
}

int capi_adc_chan_init(struct capi_adc_chan_handle **handle,
		       const struct capi_adc_chan_config *config)
{
	if (!handle || !config || !config->adc_handle || !config->adc_handle->adc_ops ||
	    !config->adc_handle->adc_ops->chan_init) {
		return -EINVAL;
	}
	return config->adc_handle->adc_ops->chan_init(handle, config);
}

int capi_adc_chan_deinit(struct capi_adc_chan_handle *handle)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_deinit) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_deinit(handle);
}

int capi_adc_chan_set_offset(struct capi_adc_chan_handle *handle, uint32_t offset)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_set_offset) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_set_offset(handle, offset);
}

int capi_adc_chan_get_offset(struct capi_adc_chan_handle *handle, uint32_t *offset)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_get_offset) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_get_offset(handle, offset);
}

int capi_adc_chan_set_gain(struct capi_adc_chan_handle *handle, uint32_t gain)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_set_gain) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_set_gain(handle, gain);
}

int capi_adc_chan_get_gain(struct capi_adc_chan_handle *handle, uint32_t *gain)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_get_gain) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_get_gain(handle, gain);
}

int capi_adc_chan_read(struct capi_adc_chan_handle *handle, void *data)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_read) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_read(handle, data);
}

int capi_adc_chan_read_async(struct capi_adc_chan_handle *handle, capi_adc_callback_t callback,
			     void *data)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_read_async) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_read_async(handle, callback, data);
}

int capi_adc_chan_stream_enable(struct capi_adc_chan_handle *handle)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_stream_enable) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_stream_enable(handle);
}

int capi_adc_chan_stream_disable(struct capi_adc_chan_handle *handle)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_stream_disable) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_stream_disable(handle);
}

int capi_adc_chan_set_calibration(const struct capi_adc_chan_handle *handle,
				  struct capi_adc_chan_calibration *cal)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_set_calibration) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_set_calibration(handle, cal);
}

int capi_adc_chan_get_calibration(const struct capi_adc_chan_handle *handle,
				  struct capi_adc_chan_calibration *cal)
{
	if (!handle || !handle->adc_handle || !handle->adc_handle->adc_ops ||
	    !handle->adc_handle->adc_ops->chan_get_calibration) {
		return -EINVAL;
	}
	return handle->adc_handle->adc_ops->chan_get_calibration(handle, cal);
}
