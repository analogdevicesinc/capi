/*
 * Copyright (c) 2024-2025 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_mailbox.c
 * @brief This file contains a set of small wrapper functions that can be used to access
 *        the Mailbox driver via its ops pointer structure. Note, it does not
 *        implement any thread safety such as mutually excluding calls to the Mailbox functions.
 *        If this is needed, it is suggested that this file and all of the other driver
 *        wrappers be copied and enhanced in your project.
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <capi_mailbox.h>

int capi_mailbox_init(struct capi_mailbox_handle **handle, const struct capi_mailbox_config *config)
{
	if (config != NULL && config->ops != NULL && config->ops->init != NULL) {
		return config->ops->init(handle, config);
	}

	return -EINVAL;
}

int capi_mailbox_deinit(struct capi_mailbox_handle *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->deinit != NULL) {
		return handle->ops->deinit(handle);
	}

	return -EINVAL;
}

int capi_mailbox_transmit_sync(struct capi_mailbox_handle *handle,
			       struct capi_mailbox_transaction *msg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->transmit_sync != NULL) {
		return handle->ops->transmit_sync(handle, msg);
	}

	return -EINVAL;
}

int capi_mailbox_receive_sync(struct capi_mailbox_handle *handle,
			      struct capi_mailbox_transaction *msg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->receive_sync != NULL) {
		return handle->ops->receive_sync(handle, msg);
	}

	return -EINVAL;
}

int capi_mailbox_register_callback(struct capi_mailbox_handle *handle,
				   capi_mailbox_callback const callback, void *const callback_arg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->register_callback != NULL) {
		return handle->ops->register_callback(handle, callback, callback_arg);
	}

	return -EINVAL;
}

int capi_mailbox_transmit_async(struct capi_mailbox_handle *handle,
				struct capi_mailbox_transaction *msg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->transmit_async != NULL) {
		return handle->ops->transmit_async(handle, msg);
	}

	return -EINVAL;
}

int capi_mailbox_receive_async(struct capi_mailbox_handle *handle,
			       struct capi_mailbox_transaction *msg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->receive_async != NULL) {
		return handle->ops->receive_async(handle, msg);
	}

	return -EINVAL;
}

int capi_mailbox_acknowledge(struct capi_mailbox_handle *handle, capi_mailbox_id_t dest_id)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->acknowledge != NULL) {
		return handle->ops->acknowledge(handle, dest_id);
	}

	return -EINVAL;
}

int capi_mailbox_ring_doorbell(struct capi_mailbox_handle *handle, capi_mailbox_id_t dest_id)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->ring_doorbell != NULL) {
		return handle->ops->ring_doorbell(handle, dest_id);
	}

	return -EINVAL;
}

int capi_mailbox_flush(struct capi_mailbox_handle *handle, capi_mailbox_id_t id)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->flush != NULL) {
		return handle->ops->flush(handle, id);
	}

	return -EINVAL;
}

void capi_mailbox_isr(void *handle)
{
	struct capi_mailbox_handle *ch = (struct capi_mailbox_handle *)handle;

	if (handle != NULL && ch->ops != NULL && ch->ops->isr != NULL) {
		ch->ops->isr(handle);
	}
}
