/*
 * Copyright (c) 2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_eth_mac.c
 * @brief This file contains a set of small wrapper functions that can be used to access
 *        the Ethernet MAC driver via its ops pointer structure. Note, it does not
 *        implement any thread safety such as mutually excluding calls to the MAC functions.
 *        If this is needed, it is suggested that this file and all of the other driver
 *        wrappers be copied and enhanced in your project.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <capi_eth_mac.h>
#include <capi_eth_phy.h>

int capi_eth_fb_init(struct capi_eth_frame_buffer *fb, uint8_t *buffer, uint32_t size,
		     uint32_t head_start)
{
	if (fb == NULL || buffer == NULL || head_start > size) {
		return -EINVAL;
	}

	memset(fb, 0, sizeof(*fb));
	fb->buffer = buffer;
	fb->max_length = size;
	fb->head = head_start;
	fb->end = head_start;

	return 0;
}

uint8_t *capi_eth_fb_head_push(struct capi_eth_frame_buffer *fb, const uint8_t *data, uint32_t size)
{
	uint8_t *buf;

	if (fb == NULL || size == 0 || size > fb->head) {
		return NULL;
	}

	fb->head -= size;
	fb->data_length += size;

	buf = (uint8_t *)fb->buffer + fb->head;
	if (data != NULL) {
		memcpy(buf, data, size);
	}

	return buf;
}

uint8_t *capi_eth_fb_head_pop(struct capi_eth_frame_buffer *fb, uint32_t size)
{
	if (fb == NULL || size == 0 || size > fb->data_length) {
		return NULL;
	}

	fb->head += size;
	fb->data_length -= size;

	return (uint8_t *)fb->buffer + fb->head;
}

uint8_t *capi_eth_fb_end_push(struct capi_eth_frame_buffer *fb, const uint8_t *data, uint32_t size)
{
	uint8_t *buf;

	if (fb == NULL || size == 0 || fb->end + size > fb->max_length) {
		return NULL;
	}

	buf = (uint8_t *)fb->buffer + fb->end;
	if (data != NULL) {
		memcpy(buf, data, size);
	}

	fb->end += size;
	fb->data_length += size;

	return buf;
}

uint8_t *capi_eth_fb_end_pop(struct capi_eth_frame_buffer *fb, uint32_t size)
{
	if (fb == NULL || size == 0 || size > fb->data_length) {
		return NULL;
	}

	fb->end -= size;
	fb->data_length -= size;

	return (uint8_t *)fb->buffer + fb->end;
}

int capi_eth_mac_init(struct capi_eth_mac_handle **handle,
		      const struct capi_eth_mac_init_config *config)
{
	int ret;

	if (config == NULL || config->ops == NULL || config->ops->init == NULL) {
		return -EINVAL;
	}

	ret = config->ops->init(handle, config);
	if (ret != 0) {
		return ret;
	}

	if ((*handle)->phy != NULL) {
		return capi_eth_mac_set_phy(*handle, (*handle)->phy);
	}

	return 0;
}

int capi_eth_mac_deinit(struct capi_eth_mac_handle *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->deinit != NULL) {
		return handle->ops->deinit(handle);
	}

	return -EINVAL;
}

int capi_eth_mac_power_control(struct capi_eth_mac_handle *handle,
			       enum capi_eth_mac_power_state state)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->power_control != NULL) {
		return handle->ops->power_control(handle, state);
	}

	return -EINVAL;
}

int capi_eth_mac_get_capabilities(struct capi_eth_mac_handle *handle,
				  struct capi_eth_mac_capabilities *capabilities)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->get_capabilities != NULL) {
		return handle->ops->get_capabilities(handle, capabilities);
	}

	return -EINVAL;
}

int capi_eth_mac_get_address(struct capi_eth_mac_handle *handle, capi_eth_mac_addr addr)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->get_address != NULL) {
		return handle->ops->get_address(handle, addr);
	}

	return -EINVAL;
}

int capi_eth_mac_set_address(struct capi_eth_mac_handle *handle, const capi_eth_mac_addr addr)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->set_address != NULL) {
		return handle->ops->set_address(handle, addr);
	}

	return -EINVAL;
}

int capi_eth_mac_set_address_filter(struct capi_eth_mac_handle *handle,
				    const capi_eth_mac_addr addr, uint8_t *slot)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->set_address_filter != NULL) {
		return handle->ops->set_address_filter(handle, addr, slot);
	}

	return -EINVAL;
}

int capi_eth_mac_clear_address_filter(struct capi_eth_mac_handle *handle, uint8_t slot)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->clear_address_filter != NULL) {
		return handle->ops->clear_address_filter(handle, slot);
	}

	return -EINVAL;
}

int capi_eth_mac_send_frame(struct capi_eth_mac_handle *handle,
			    const struct capi_eth_frame_buffer *frame)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->send_frame != NULL) {
		return handle->ops->send_frame(handle, frame);
	}

	return -EINVAL;
}

int capi_eth_mac_get_rx_frame_size(struct capi_eth_mac_handle *handle, uint32_t *size)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->get_rx_frame_size != NULL) {
		return handle->ops->get_rx_frame_size(handle, size);
	}

	return -EINVAL;
}

int capi_eth_mac_read_frame(struct capi_eth_mac_handle *handle, struct capi_eth_frame_buffer *frame)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->read_frame != NULL) {
		return handle->ops->read_frame(handle, frame);
	}

	return -EINVAL;
}

int capi_eth_mac_set_link_config(struct capi_eth_mac_handle *handle,
				 const struct capi_eth_mac_link_config *config)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->set_link_config != NULL) {
		return handle->ops->set_link_config(handle, config);
	}

	return -EINVAL;
}

int capi_eth_mac_set_broadcast(struct capi_eth_mac_handle *handle, bool enable)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->set_broadcast != NULL) {
		return handle->ops->set_broadcast(handle, enable);
	}

	return -EINVAL;
}

int capi_eth_mac_set_all_multicast(struct capi_eth_mac_handle *handle, bool enable)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->set_all_multicast != NULL) {
		return handle->ops->set_all_multicast(handle, enable);
	}

	return -EINVAL;
}

int capi_eth_mac_set_promiscuous(struct capi_eth_mac_handle *handle, bool enable)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->set_promiscuous != NULL) {
		return handle->ops->set_promiscuous(handle, enable);
	}

	return -EINVAL;
}

int capi_eth_mac_start(struct capi_eth_mac_handle *handle)
{
	int ret;

	if (handle == NULL || handle->ops == NULL || handle->ops->start == NULL) {
		return -EINVAL;
	}

	ret = handle->ops->start(handle);
	if (ret == 0) {
		handle->started = true;
	}

	return ret;
}

int capi_eth_mac_stop(struct capi_eth_mac_handle *handle)
{
	int ret;

	if (handle == NULL || handle->ops == NULL || handle->ops->stop == NULL) {
		return -EINVAL;
	}

	ret = handle->ops->stop(handle);
	if (ret == 0) {
		handle->started = false;
	}

	return ret;
}

int capi_eth_mac_flush_rx_buffer(struct capi_eth_mac_handle *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->flush_rx_buffer != NULL) {
		return handle->ops->flush_rx_buffer(handle);
	}

	return -EINVAL;
}

int capi_eth_mac_flush_tx_buffer(struct capi_eth_mac_handle *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->flush_tx_buffer != NULL) {
		return handle->ops->flush_tx_buffer(handle);
	}

	return -EINVAL;
}

int capi_eth_mac_sleep(struct capi_eth_mac_handle *handle, bool enable)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->sleep != NULL) {
		return handle->ops->sleep(handle, enable);
	}

	return -EINVAL;
}

int capi_eth_mac_set_vlan_filter(struct capi_eth_mac_handle *handle,
				 const struct capi_eth_vlan_filter *filter)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->set_vlan_filter != NULL) {
		return handle->ops->set_vlan_filter(handle, filter);
	}

	return -EINVAL;
}

int capi_eth_mac_ptp_get_time(struct capi_eth_mac_handle *handle, struct capi_eth_mac_time *time)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->ptp_get_time != NULL) {
		return handle->ops->ptp_get_time(handle, time);
	}

	return -EINVAL;
}

int capi_eth_mac_ptp_set_time(struct capi_eth_mac_handle *handle,
			      const struct capi_eth_mac_time *time)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->ptp_set_time != NULL) {
		return handle->ops->ptp_set_time(handle, time);
	}

	return -EINVAL;
}

int capi_eth_mac_ptp_inc_time(struct capi_eth_mac_handle *handle,
			      const struct capi_eth_mac_time *time)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->ptp_inc_time != NULL) {
		return handle->ops->ptp_inc_time(handle, time);
	}

	return -EINVAL;
}

int capi_eth_mac_ptp_dec_time(struct capi_eth_mac_handle *handle,
			      const struct capi_eth_mac_time *time)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->ptp_dec_time != NULL) {
		return handle->ops->ptp_dec_time(handle, time);
	}

	return -EINVAL;
}

int capi_eth_mac_ptp_set_alarm(struct capi_eth_mac_handle *handle,
			       const struct capi_eth_mac_time *time)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->ptp_set_alarm != NULL) {
		return handle->ops->ptp_set_alarm(handle, time);
	}

	return -EINVAL;
}

int capi_eth_mac_ptp_adjust_freq(struct capi_eth_mac_handle *handle, int32_t correction_q31)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->ptp_adjust_freq != NULL) {
		return handle->ops->ptp_adjust_freq(handle, correction_q31);
	}

	return -EINVAL;
}

int capi_eth_mac_get_stats(struct capi_eth_mac_handle *handle, struct capi_eth_mac_stats *stats)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->get_stats != NULL) {
		return handle->ops->get_stats(handle, stats);
	}

	return -EINVAL;
}

int capi_eth_mac_get_queue_stats(struct capi_eth_mac_handle *handle, unsigned int queue,
				 struct capi_eth_mac_stats *stats)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->get_queue_stats != NULL) {
		return handle->ops->get_queue_stats(handle, queue, stats);
	}

	return -EINVAL;
}

int capi_eth_mac_register_callback(struct capi_eth_mac_handle *handle,
				   capi_eth_mac_event_callback const callback,
				   void *const callback_arg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->register_callback != NULL) {
		return handle->ops->register_callback(handle, callback, callback_arg);
	}

	return -EINVAL;
}

int capi_eth_mac_reg_read(struct capi_eth_mac_handle *handle, uint32_t addr, uint32_t *data)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->reg_read != NULL) {
		return handle->ops->reg_read(handle, addr, data);
	}

	return -EINVAL;
}

int capi_eth_mac_reg_write(struct capi_eth_mac_handle *handle, uint32_t addr, uint32_t data)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->reg_write != NULL) {
		return handle->ops->reg_write(handle, addr, data);
	}

	return -EINVAL;
}

int capi_eth_mac_get_phy(struct capi_eth_mac_handle *handle, struct capi_eth_phy_handle **phy)
{
	if (handle == NULL || phy == NULL) {
		return -EINVAL;
	}

	*phy = handle->phy;

	return 0;
}

int capi_eth_mac_set_phy(struct capi_eth_mac_handle *handle, struct capi_eth_phy_handle *phy)
{
	struct capi_eth_mac_capabilities caps;
	int ret;

	if (handle == NULL) {
		return -EINVAL;
	}

	if (phy == NULL) {
		handle->phy = NULL;
		return 0;
	}

	ret = capi_eth_mac_get_capabilities(handle, &caps);
	if (ret == 0)
		capi_eth_phy_set_interface(phy, (enum capi_eth_interface)caps.media_interface);

	handle->phy = phy;

	return 0;
}

int capi_eth_mac_poll(struct capi_eth_mac_handle *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->poll != NULL) {
		return handle->ops->poll(handle);
	}

	return -EINVAL;
}

void capi_eth_mac_isr(void *handle)
{
	struct capi_eth_mac_handle *ch = (struct capi_eth_mac_handle *)handle;

	if (handle != NULL && ch->ops != NULL && ch->ops->isr != NULL) {
		ch->ops->isr(handle);
	}
}
