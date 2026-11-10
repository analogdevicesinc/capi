/*
 * Copyright (c) 2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_can.c
 * @brief This file contains a set of small wrapper functions that can be used to access
 *        the CAN driver via its ops pointer structure. Note, it does not
 *        implement any thread safety such as mutually excluding calls to the CAN functions.
 *        If this is needed, it is suggested that this file and all of the other driver
 *        wrappers be copied and enhanced in your project.
 *
 *        capi_can_calc_timing() is the one exception to the "pure forward" rule
 *        followed by every other function in this file: it is self-contained
 *        integer math with no handle/ops access, deliberately kept out of the
 *        ops vtable since it has no hardware dependency (mirrors Zephyr's
 *        standalone can_calc_timing() being distinct from can_set_timing()).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <capi_can.h>

int capi_can_init(struct capi_can_controller_handle **handle, const struct capi_can_config *config)
{
	if (!handle || !config || !config->ops || !config->ops->init) {
		return -EINVAL;
	}
	return config->ops->init(handle, config);
}

int capi_can_deinit(struct capi_can_controller_handle *handle)
{
	if (!handle || !handle->ops || !handle->ops->deinit) {
		return -EINVAL;
	}
	return handle->ops->deinit(handle);
}

int capi_can_start(struct capi_can_controller_handle *handle)
{
	if (!handle || !handle->ops || !handle->ops->start) {
		return -EINVAL;
	}
	return handle->ops->start(handle);
}

int capi_can_stop(struct capi_can_controller_handle *handle)
{
	if (!handle || !handle->ops || !handle->ops->stop) {
		return -EINVAL;
	}
	return handle->ops->stop(handle);
}

int capi_can_set_timing(struct capi_can_controller_handle *handle,
			const struct capi_can_timing *nominal, const struct capi_can_timing *data)
{
	if (!handle || !nominal || !handle->ops || !handle->ops->set_timing) {
		return -EINVAL;
	}
	return handle->ops->set_timing(handle, nominal, data);
}

int capi_can_set_mode(struct capi_can_controller_handle *handle, enum capi_can_mode mode)
{
	if (!handle || !handle->ops || !handle->ops->set_mode) {
		return -EINVAL;
	}
	return handle->ops->set_mode(handle, mode);
}

int capi_can_get_capabilities(struct capi_can_controller_handle *handle, uint32_t *caps)
{
	if (!handle || !caps || !handle->ops || !handle->ops->get_capabilities) {
		return -EINVAL;
	}
	return handle->ops->get_capabilities(handle, caps);
}

int capi_can_add_filter(struct capi_can_controller_handle *handle,
			const struct capi_can_filter *filter)
{
	if (!handle || !filter || !handle->ops || !handle->ops->add_filter) {
		return -EINVAL;
	}
	return handle->ops->add_filter(handle, filter);
}

int capi_can_remove_filter(struct capi_can_controller_handle *handle, int filter_id)
{
	if (!handle || !handle->ops || !handle->ops->remove_filter) {
		return -EINVAL;
	}
	return handle->ops->remove_filter(handle, filter_id);
}

int capi_can_send(struct capi_can_controller_handle *handle, const struct capi_can_frame *frame,
		  uint32_t timeout_ms)
{
	if (!handle || !frame || !handle->ops || !handle->ops->send) {
		return -EINVAL;
	}
	return handle->ops->send(handle, frame, timeout_ms);
}

int capi_can_receive(struct capi_can_controller_handle *handle, struct capi_can_frame *frame,
		     uint32_t timeout_ms)
{
	if (!handle || !frame || !handle->ops || !handle->ops->receive) {
		return -EINVAL;
	}
	return handle->ops->receive(handle, frame, timeout_ms);
}

int capi_can_send_async(struct capi_can_controller_handle *handle,
			const struct capi_can_frame *frame)
{
	if (!handle || !frame || !handle->ops || !handle->ops->send_async) {
		return -EINVAL;
	}
	return handle->ops->send_async(handle, frame);
}

int capi_can_register_callback(struct capi_can_controller_handle *handle,
			       capi_can_callback const callback, void *const callback_arg)
{
	if (!handle || !handle->ops || !handle->ops->register_callback) {
		return -EINVAL;
	}
	return handle->ops->register_callback(handle, callback, callback_arg);
}

int capi_can_get_error_counters(struct capi_can_controller_handle *handle,
				struct capi_can_error_counters *counters)
{
	if (!handle || !counters || !handle->ops || !handle->ops->get_error_counters) {
		return -EINVAL;
	}
	return handle->ops->get_error_counters(handle, counters);
}

int capi_can_recover(struct capi_can_controller_handle *handle)
{
	if (!handle || !handle->ops || !handle->ops->recover) {
		return -EINVAL;
	}
	return handle->ops->recover(handle);
}

void capi_can_isr(void *handle)
{
	if (!handle) {
		return;
	}
	struct capi_can_controller_handle *ctrl_handle =
		(struct capi_can_controller_handle *)handle;
	if (!ctrl_handle->ops || !ctrl_handle->ops->isr) {
		return;
	}
	ctrl_handle->ops->isr(handle);
}

/*
 * capi_can_calc_timing() - pure math, no handle/ops access.
 *
 * Brute-force search over prescaler values for the (seg1, seg2) split that
 * lands the sample point closest to 87.5% of the bit time (a common default,
 * e.g. Bosch's recommendation for higher bit rates / Linux SocketCAN),
 * targeting an exact bitrate match if possible. Without an explicit
 * `limits` argument, the field-width limits used (seg1 <= 32, seg2 <= 16,
 * prescaler <= UINT16_MAX) are a conservative common denominator across the
 * surveyed hardware, not part of the API contract -- pass a platform's real
 * limits to guarantee the result is accepted by its capi_can_set_timing().
 */
int capi_can_calc_timing(uint32_t clk_freq_hz, uint32_t bitrate,
			 const struct capi_can_timing_limits *limits,
			 struct capi_can_timing *timing)
{
	if (!timing || bitrate == 0 || clk_freq_hz == 0) {
		return -EINVAL;
	}

	uint8_t max_seg1 = limits ? limits->max_seg1 : 32;
	uint8_t max_seg2 = limits ? limits->max_seg2 : 16;
	uint8_t max_sjw = limits ? limits->max_sjw : 4;
	uint32_t max_prescaler = limits ? limits->max_prescaler : UINT16_MAX;

	if (limits && (max_seg1 == 0 || max_seg2 == 0 || max_sjw == 0 || max_prescaler == 0)) {
		return -EINVAL;
	}

	for (uint32_t prescaler = 1; prescaler <= max_prescaler; prescaler++) {
		uint64_t denom = (uint64_t)bitrate * prescaler;
		uint32_t tq_per_bit = (uint32_t)(clk_freq_hz / denom);

		if (tq_per_bit < 3) {
			/* Too few time quanta per bit to form sync+seg1+seg2; larger
			 * prescalers only make this worse, so stop searching. */
			break;
		}
		if (clk_freq_hz % denom != 0) {
			/* Not an exact bitrate match at this prescaler. */
			continue;
		}

		/* 1 sync quantum (implicit) + seg1 + seg2 == tq_per_bit. */
		uint32_t remaining = tq_per_bit - 1;
		uint32_t seg1 = (remaining * 875) / 1000;

		if (seg1 < 1) {
			seg1 = 1;
		}
		if (seg1 > remaining - 1) {
			seg1 = remaining - 1;
		}
		uint32_t seg2 = remaining - seg1;

		if (seg1 > max_seg1 || seg2 < 1 || seg2 > max_seg2) {
			continue;
		}

		uint32_t sjw = seg2 < max_sjw ? seg2 : max_sjw;

		timing->prescaler = (uint16_t)prescaler;
		timing->seg1 = (uint8_t)seg1;
		timing->seg2 = (uint8_t)seg2;
		timing->sjw = (uint8_t)sjw;
		return 0;
	}

	return -EINVAL;
}
