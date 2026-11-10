/*
 * Copyright (c) 2024-2025 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file
 * @brief Common HAL Mailbox API
 */

#ifndef _CAPI_MAILBOX_H_
#define _CAPI_MAILBOX_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * Cores are number 0 to n which will be
 *  translated by the driver to native mailbox identifiers. It is up to the application to
 *  manage the core IDs such that all cores have a unique identifier.
 */
typedef uint32_t capi_mailbox_id_t;

/**
 * @brief mailbox async events
 */
enum capi_mailbox_async_event {
	CAPI_MAILBOX_EVENT_MSG_RECIEVED,     /**< message received */
	CAPI_MAILBOX_EVENT_MSG_DOORBELL,     /**< doorbell indication */
	CAPI_MAILBOX_EVENT_MSG_ACKNOWLEDGED, /**< message acknowledged */
	CAPI_MAILBOX_EVENT_MSG_ACK_TIMEOUT   /**< Acknowledge timeout */
};

/**
 * @brief Mailbox controller configuration
 */
struct capi_mailbox_config {
	/** Mailbox controller identifier: Base address for internal controllers or instance number
	 */
	uint32_t identifier;
	/** Platform specific implementation of the API. (Mandatory field, but can be NULL and we'll
	 * make sure it gets auto populated when using our build system.) */
	const struct capi_mailbox_ops *ops;
};

/**
 * @brief Mailbox handle
 *
 * Drivers may need own handle type to handle internals.
 * Driver developer shall declare this as the first field of private handle structure.
 */
struct capi_mailbox_handle {
	const struct capi_mailbox_ops *ops; /**< set and used by capi thin layer */
	bool init_allocated;                /**< If true, the driver is owner of handle memory. */
	void *lock; /**< set and used by capi thin layer if locking is available */
	void *priv; /**< set and used by user optionally */
};

/**
 * @brief Mailbox transaction context.
 */
struct capi_mailbox_transaction {
	capi_mailbox_id_t src_id;  /**< Identifies the sender. */
	capi_mailbox_id_t dest_id; /**< Identifies the receiver. */
	uint8_t *buf;              /**< Points to the message buffer. */
	uint32_t size;             /**< Size of the buffer in bytes. */
	uint32_t msg_len;          /**< Length of the message in bytes. */
};

/**
 * @brief  Mailbox Driver Call Back type
 * @param[in] event async event type
 * @param[in] arg Pointer to user specific data.
 * @param[in] event_extra optional, platform/driver specific extra information for event
 */
typedef void (*capi_mailbox_callback)(enum capi_mailbox_async_event event, void *arg,
				      struct capi_mailbox_transaction *msg);

/**
 * @brief Initialize an instance of the Mailbox controller.
 *
 * @param [in out] handle Points to a pointer of the context structure. If this
 *                 pointer is set to NULL, then the controller will allocate
 *	               the context structure and be the owner. If the pointer
 *	               is not NULL, the application has allocated the structure and is
 *	               the owner.
 * @param [in] config Points to the configuration for the Mailbox controller.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_init(struct capi_mailbox_handle **handle,
		      const struct capi_mailbox_config *config);

/**
 * @brief Deinitialize the mailbox controller, disable, and bring to default settings.
 *
 * @param [in] handle Points to the Mailbox controller context.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_deinit(struct capi_mailbox_handle *handle);

/**
 * @brief Send a message to the destination and wait for the acknowledge from the destination.
 *
 * @param [in] handle Points to the Mailbox controller context.
 * @param [in] msg Points to the transaction context. For send this must include, dest_id, buf, and
 * msg_len.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_transmit_sync(struct capi_mailbox_handle *handle,
			       struct capi_mailbox_transaction *msg);

/**
 * @brief Wait for a message to be received from your mailbox.
 *
 * @param [in] handle Points to the Mailbox controller context.
 * @param [inout] msg Points to the transaction context. For receive this must include, buf, and
 * size. When complete will contain the src_id and msg_len.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_receive_sync(struct capi_mailbox_handle *handle,
			      struct capi_mailbox_transaction *msg);

/**
 * @brief Register callback to get async events
 *
 * @param [in] handle Points to the Mailbox controller context.
 * @param [in] callback User callback function
 * @param [in] callback_arg User callback arg
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_register_callback(struct capi_mailbox_handle *handle,
				   capi_mailbox_callback const callback, void *const callback_arg);

/**
 * @brief Send a message and don't wait for the acknowledge.
 *
 * @param [in] handle Points to the Mailbox controller context.
 * @param [in] msg Points to the transaction context. For send this must include, dest_id, buf, and
 * msg_len.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_transmit_async(struct capi_mailbox_handle *handle,
				struct capi_mailbox_transaction *msg);

/**
 * @brief Start a receive message operation but don't wait for it to arrive.
 *
 * @param [in] handle Points to the Mailbox controller context.
 * @param [in] msg Points to the transaction context. For receive this must include, buf, and size.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_receive_async(struct capi_mailbox_handle *handle,
			       struct capi_mailbox_transaction *msg);

/**
 * @brief Acknowledge receipt of the mailbox message.
 *
 * @param [in] handle Points to the Mailbox controller context.
 * @param [in] dest_id Defines the mailbox to acknowledge.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_acknowledge(struct capi_mailbox_handle *handle, capi_mailbox_id_t dest_id);

/**
 * @brief Mailbox Driver Interrupt handler. If interrupt vectors are managed and implemented by
 * user, then user shall call this function in the relevant interrupt vector function.
 *
 * @param [in] handle Points to the Mailbox controller context
 */
void capi_mailbox_isr(void *handle);

/**
 * @brief Ring a mailbox doorbell.
 *
 * @param [in] handle Points to the Mailbox controller context.
 * @param [in] dest_id Defines the mailbox to ring.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_ring_doorbell(struct capi_mailbox_handle *handle, capi_mailbox_id_t dest_id);

/**
 * @brief Flush a mailbox FIFO.
 *
 * @param [in] handle Points to the Mailbox controller context.
 * @param [in] id Defines the mailbox to flush.
 *
 * @return int 0 for success or error code.
 */
int capi_mailbox_flush(struct capi_mailbox_handle *handle, capi_mailbox_id_t id);

/**
 * @brief Structure holding Mailbox function pointers that point to the platform
 * specific function. See API functions for relevant descriptions.
 */
struct capi_mailbox_ops {
	/** See capi_mailbox_init() */
	int (*init)(struct capi_mailbox_handle **handle, const struct capi_mailbox_config *config);
	/** See capi_mailbox_deinit() */
	int (*deinit)(struct capi_mailbox_handle *handle);
	/** See capi_mailbox_transmit_sync() */
	int (*transmit_sync)(struct capi_mailbox_handle *handle,
			     struct capi_mailbox_transaction *msg);
	/** See capi_mailbox_receive_sync() */
	int (*receive_sync)(struct capi_mailbox_handle *handle,
			    struct capi_mailbox_transaction *msg);
	/** See capi_mailbox_register_callback() */
	int (*register_callback)(struct capi_mailbox_handle *handle,
				 capi_mailbox_callback const callback, void *const callback_arg);
	/** See capi_mailbox_transmit_async() */
	int (*transmit_async)(struct capi_mailbox_handle *handle,
			      struct capi_mailbox_transaction *msg);
	/** See capi_mailbox_receive_sync() */
	int (*receive_async)(struct capi_mailbox_handle *handle,
			     struct capi_mailbox_transaction *msg);
	/** set capi_mailbox_acknowledge() */
	int (*acknowledge)(struct capi_mailbox_handle *handle, capi_mailbox_id_t dest_id);
	/** see capi_mailbox_ring_doorbel() */
	int (*ring_doorbell)(struct capi_mailbox_handle *handle, capi_mailbox_id_t dest_id);
	/** see capi_mailbox_flush() */
	int (*flush)(struct capi_mailbox_handle *handle, capi_mailbox_id_t id);
	/** See capi_mailbox_isr() */
	void (*isr)(void *handle);
};

#if defined(__cplusplus)
}
#endif

#endif
