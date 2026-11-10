/*
 * Copyright (c) 2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_can.h
 * @brief Common HAL CAN bus API
 *
 * Provides a vendor-neutral abstraction over Controller Area Network
 * controllers, spanning Classic CAN 2.0B and CAN-FD (ISO 11898-1). The frame
 * struct is an FD-first superset: classic-only controllers simply never see
 * @c fd/@c brs set and never exceed an 8-byte payload.
 */

#ifndef _CAPI_CAN_H_
#define _CAPI_CAN_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/** Maximum CAN-FD payload length in bytes. Classic CAN frames use at most the
 *  first 8 bytes of @c data. */
#define CAPI_CAN_MAX_DLEN 64U

/** Pass to capi_can_send()/capi_can_receive() to block indefinitely. */
#define CAPI_CAN_TIMEOUT_FOREVER UINT32_MAX

/**
 * @brief CAN controller operating mode.
 */
enum capi_can_mode {
	CAPI_CAN_MODE_CLASSIC,     /**< Classic CAN 2.0B only, no FD */
	CAPI_CAN_MODE_FD,          /**< CAN-FD capable (BRS-eligible) */
	CAPI_CAN_MODE_LOOPBACK,    /**< Internal loopback, no bus activity (test mode) */
	CAPI_CAN_MODE_LISTEN_ONLY, /**< Bus-monitor / silent mode, no ACK/error frames driven */
};

/**
 * @brief CAN controller bus-error state, per ISO 11898-1 fault confinement.
 */
enum capi_can_bus_state {
	CAPI_CAN_STATE_ERROR_ACTIVE,  /**< Normal operation */
	CAPI_CAN_STATE_ERROR_WARNING, /**< One or both error counters >= 96 */
	CAPI_CAN_STATE_ERROR_PASSIVE, /**< One or both error counters >= 128 */
	CAPI_CAN_STATE_BUS_OFF,       /**< TEC >= 256, controller disconnected from bus */
};

/**
 * @brief CAN async event types delivered to the registered controller-wide callback.
 */
enum capi_can_async_event {
	CAPI_CAN_EVENT_TX_COMPLETE,   /**< Async transmit finished (event_extra: 0 = ok) */
	CAPI_CAN_EVENT_RX_INDICATION, /**< One or more frames available in the RX queue */
	CAPI_CAN_EVENT_STATE_CHANGE,  /**< Bus error-state transitioned; event_extra is the new
				       *   enum capi_can_bus_state cast to int */
	CAPI_CAN_EVENT_ERROR,         /**< Frame-level bus error (stuff/form/CRC/ACK/bit error);
				       *   event_extra is a driver-specific error code, 0 if n/a */
	CAPI_CAN_EVENT_TX_ABORTED, /**< In-flight async TX was aborted (e.g. by capi_can_stop()) */
};

/**
 * @brief Controller capability bitmask, queried via capi_can_get_capabilities().
 */
enum capi_can_cap {
	CAPI_CAN_CAP_FD = (1U << 0),          /**< Supports CAN-FD (64-byte payload) */
	CAPI_CAN_CAP_BRS = (1U << 1),         /**< Supports Bit Rate Switching in FD data phase */
	CAPI_CAN_CAP_LOOPBACK = (1U << 2),    /**< Supports internal loopback test mode */
	CAPI_CAN_CAP_LISTEN_ONLY = (1U << 3), /**< Supports listen-only/bus-monitor mode */
	CAPI_CAN_CAP_TRIPLE_SAMPLING =
		(1U << 4), /**< Supports triple-sampling at bit sample point */
};

/**
 * @brief Raw bit-timing quanta.
 *
 * The same struct type is reused for both the nominal (arbitration-phase)
 * and data-phase (CAN-FD BRS) timing; pass two separate instances to
 * capi_can_set_timing(). For classic-CAN-only controllers, pass NULL for
 * the data-phase argument. No driver performs bitrate-to-quanta math; use
 * capi_can_calc_timing() to derive a starting point from a target bitrate.
 */
struct capi_can_timing {
	uint16_t prescaler; /**< Clock prescaler, divides peripheral clock into time quanta */
	uint8_t sjw;        /**< Synchronization Jump Width, in time quanta */
	uint8_t seg1;       /**< Propagation + Phase Segment 1, in time quanta */
	uint8_t seg2;       /**< Phase Segment 2, in time quanta */
};

/**
 * @brief Hardware field-width limits for capi_can_calc_timing()'s search.
 *
 * Pass NULL to capi_can_calc_timing() to use its built-in generic caps
 * instead. A platform whose controller has tighter field widths than the
 * generic caps should pass its real limits here so the search only returns
 * combinations that capi_can_set_timing() will actually accept.
 */
struct capi_can_timing_limits {
	uint16_t max_prescaler; /**< Maximum prescaler value the search may use */
	uint8_t max_sjw;        /**< Maximum Synchronization Jump Width, in time quanta */
	uint8_t max_seg1;       /**< Maximum Propagation + Phase Segment 1, in time quanta */
	uint8_t max_seg2;       /**< Maximum Phase Segment 2, in time quanta */
};

/**
 * @brief CAN acceptance filter: ID + mask match against incoming frames.
 *
 * A bit set in @p mask means the corresponding bit of the incoming frame ID
 * must match @p id; a cleared mask bit is a don't-care. All frames pass
 * through a single software RX queue regardless of which filter matched.
 */
struct capi_can_filter {
	uint32_t id;      /**< Reference identifier */
	uint32_t mask;    /**< 1 bits = must match; 0 bits = don't-care */
	bool extended_id; /**< true = match 29-bit extended IDs, false = 11-bit standard IDs */
	bool rtr_only;    /**< true = match only Remote Transmission Request frames */
};

/**
 * @brief CAN frame descriptor, superset covering Classic CAN 2.0A/B and CAN-FD.
 *
 * @p len is the raw payload length in bytes (0-8 for classic CAN, 0-64 for
 * CAN-FD). Callers never deal with the non-linear wire DLC-to-byte-length
 * mapping; each driver converts @p len to/from the wire DLC code internally.
 */
struct capi_can_frame {
	uint32_t id;                     /**< 11-bit standard or 29-bit extended identifier */
	uint8_t len;                     /**< Payload length in bytes: 0-8 classic, 0-64 FD */
	uint8_t data[CAPI_CAN_MAX_DLEN]; /**< Payload buffer */
	bool extended_id;                /**< true: 29-bit extended ID, false: 11-bit standard ID */
	bool rtr;                        /**< true: Remote Transmission Request (no data phase) */
	bool fd;                         /**< true: CAN-FD frame format */
	bool brs;           /**< true: Bit Rate Switch active in data phase (fd must be true) */
	bool esi;           /**< Error State Indicator (RX: as received; TX: ignored, driver
			     *   sets it based on the controller's own error state) */
	uint64_t timestamp; /**< Optional hardware RX timestamp, driver-defined units, 0 if
			     *   unsupported */
};

/**
 * @brief CAN bus error counters and current fault-confinement state.
 */
struct capi_can_error_counters {
	uint8_t tx_error_count;        /**< Transmit Error Counter (TEC) */
	uint8_t rx_error_count;        /**< Receive Error Counter (REC) */
	enum capi_can_bus_state state; /**< Current fault-confinement state */
};

/**
 * @brief CAN controller configuration
 */
struct capi_can_config {
	/** CAN controller identifier: base address / instance index for internal controllers. */
	uint64_t identifier;
	/** Frequency of peripheral clock in Hz feeding the bit-timing prescaler; if 0, driver uses
	 *  platform default. */
	uint32_t clk_freq_hz;
	/** Requested operating mode at init time. Can also be changed later while stopped via
	 *  capi_can_set_mode(). */
	enum capi_can_mode mode;
	/** Optional platform specific extra configuration. */
	void *extra;
	/** Platform specific implementation of the API. (Mandatory field, but can be NULL and we'll
	 * make sure it gets auto populated when using our build system.) */
	const struct capi_can_ops *ops;
};

/**
 * @brief CAN controller handle
 *
 * Drivers may need own handle type to handle internals.
 * Driver developer shall declare this as the first field of private handle structure.
 */
struct capi_can_controller_handle {
	const struct capi_can_ops *ops; /**< set and used by capi thin layer */
	bool init_allocated;            /**< If true, the driver is owner of handle memory. */
	void *lock;                     /**< set and used by capi thin layer if mutex available */
	void *priv;                     /**< set and used by user optionally */
};

/**
 * @brief CAN driver callback type
 * @param[in] event CAN event type
 * @param[in] arg Pointer to user specific data.
 * @param[in] event_extra optional, platform/driver specific extra information for event
 */
typedef void (*capi_can_callback)(enum capi_can_async_event event, void *arg, int event_extra);

/**
 * @brief Initialize an instance of the CAN controller.
 *
 * Only allocates/configures the controller; it does not join the bus. Call
 * capi_can_set_timing() and capi_can_start() before transmitting or receiving.
 *
 * @param[in,out] handle Points to a pointer of the context structure. If this
 *                pointer is set to NULL, then the controller will allocate
 *                the context structure and be the owner. If the pointer
 *                is not NULL, the application has allocated the structure and is
 *                the owner.
 * @param[in] config Points to the configuration for the CAN controller.
 *
 * @return int 0 for success or error code.
 */
int capi_can_init(struct capi_can_controller_handle **handle, const struct capi_can_config *config);

/**
 * @brief Deinitialize the CAN controller, disable, and bring to default settings.
 *
 * @param[in] handle Points to the CAN controller context.
 *
 * @return int 0 for success or error code.
 */
int capi_can_deinit(struct capi_can_controller_handle *handle);

/**
 * @brief Enable the transceiver and join bus arbitration ("go on-bus").
 *
 * Bit timing must be configured via capi_can_set_timing() before starting.
 * Required before capi_can_send()/capi_can_send_async()/capi_can_receive().
 *
 * @param[in] handle Points to the CAN controller context.
 *
 * @return int 0 for success or error code.
 */
int capi_can_start(struct capi_can_controller_handle *handle);

/**
 * @brief Leave the bus ("go off-bus") so timing/mode/filters can be reconfigured.
 *
 * Aborts any in-flight async transfer, reporting CAPI_CAN_EVENT_TX_ABORTED via
 * the registered callback if one is set.
 *
 * @param[in] handle Points to the CAN controller context.
 *
 * @return int 0 for success or error code.
 */
int capi_can_stop(struct capi_can_controller_handle *handle);

/**
 * @brief Program raw bit-timing quanta.
 *
 * Must be called while the controller is stopped.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[in] nominal Nominal (arbitration-phase) bit timing. Must not be NULL.
 * @param[in] data FD data-phase bit timing, or NULL for classic-CAN-only operation.
 *
 * @return int 0 for success or error code.
 */
int capi_can_set_timing(struct capi_can_controller_handle *handle,
			const struct capi_can_timing *nominal, const struct capi_can_timing *data);

/**
 * @brief Compute bit-timing quanta closest to a requested bitrate for a given clock.
 *
 * Pure-math helper with no hardware access: does not touch any controller
 * handle, and can be called before capi_can_init(). Intended as a starting
 * point for capi_can_set_timing().
 *
 * @param[in] clk_freq_hz Peripheral clock frequency in Hz feeding the prescaler.
 * @param[in] bitrate Target bitrate in bits per second.
 * @param[in] limits Platform field-width limits to search within, or NULL to
 *                   use the function's built-in generic caps. Pass a
 *                   platform's real hardware limits when they are tighter
 *                   than the generic caps, so the result is guaranteed
 *                   acceptable to that platform's capi_can_set_timing().
 * @param[out] timing Receives the computed prescaler/sjw/seg1/seg2.
 *
 * @return int 0 for success or error code.
 */
int capi_can_calc_timing(uint32_t clk_freq_hz, uint32_t bitrate,
			 const struct capi_can_timing_limits *limits,
			 struct capi_can_timing *timing);

/**
 * @brief Change operating mode.
 *
 * Must be called while the controller is stopped.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[in] mode Requested operating mode.
 *
 * @return int 0 for success or error code.
 */
int capi_can_set_mode(struct capi_can_controller_handle *handle, enum capi_can_mode mode);

/**
 * @brief Query the capability bitmask of this controller instance.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[out] caps Receives a bitwise-OR of enum capi_can_cap values.
 *
 * @return int 0 for success or error code.
 */
int capi_can_get_capabilities(struct capi_can_controller_handle *handle, uint32_t *caps);

/**
 * @brief Add an ID+mask acceptance filter.
 *
 * Must be called while the controller is stopped. All matching frames land in
 * the single software RX queue drained by capi_can_receive() and signaled via
 * CAPI_CAN_EVENT_RX_INDICATION. If no filter has been added, controllers
 * default to accepting nothing.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[in] filter Points to the filter descriptor to add.
 *
 * @return int a non-negative filter id for success, or a negative error code
 *         (e.g. -ENOSPC if the filter table is full).
 */
int capi_can_add_filter(struct capi_can_controller_handle *handle,
			const struct capi_can_filter *filter);

/**
 * @brief Remove a previously added filter.
 *
 * Must be called while the controller is stopped.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[in] filter_id Filter id returned by capi_can_add_filter().
 *
 * @return int 0 for success or error code.
 */
int capi_can_remove_filter(struct capi_can_controller_handle *handle, int filter_id);

/**
 * @brief Sync/Blocking transmit function.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[in] frame Points to the frame to transmit.
 * @param[in] timeout_ms Maximum time to wait in milliseconds. 0 = non-blocking poll,
 *                       CAPI_CAN_TIMEOUT_FOREVER = block indefinitely.
 *
 * @return int 0 for success or error code.
 */
int capi_can_send(struct capi_can_controller_handle *handle, const struct capi_can_frame *frame,
		  uint32_t timeout_ms);

/**
 * @brief Sync/Blocking receive function.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[out] frame Points to storage for the received frame.
 * @param[in] timeout_ms Maximum time to wait in milliseconds. 0 = non-blocking poll,
 *                       CAPI_CAN_TIMEOUT_FOREVER = block indefinitely.
 *
 * @return int 0 for success or error code.
 */
int capi_can_receive(struct capi_can_controller_handle *handle, struct capi_can_frame *frame,
		     uint32_t timeout_ms);

/**
 * @brief Async/Non-blocking transmit function.
 *
 * Only a single asynchronous transmit may be in flight per handle at a time;
 * completion is reported via CAPI_CAN_EVENT_TX_COMPLETE.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[in] frame Points to the frame to transmit.
 *
 * @return int 0 for success or error code (e.g. -EBUSY if a previous async
 *         send has not yet completed).
 */
int capi_can_send_async(struct capi_can_controller_handle *handle,
			const struct capi_can_frame *frame);

/**
 * @brief Register callback to get async events
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[in] callback User callback function, or NULL to deregister.
 * @param[in] callback_arg User callback arg
 *
 * @return int 0 for success or error code.
 */
int capi_can_register_callback(struct capi_can_controller_handle *handle,
			       capi_can_callback const callback, void *const callback_arg);

/**
 * @brief Read current transmit/receive error counters and fault-confinement state.
 *
 * @param[in] handle Points to the CAN controller context.
 * @param[out] counters Points to storage for the error counters.
 *
 * @return int 0 for success or error code.
 */
int capi_can_get_error_counters(struct capi_can_controller_handle *handle,
				struct capi_can_error_counters *counters);

/**
 * @brief Request bus-off recovery.
 *
 * Per ISO 11898-1, bus-off recovery normally requires observing 128
 * occurrences of 11 consecutive recessive bits on the bus; this call
 * requests that sequence. A no-op (returns 0) if the controller is not
 * currently bus-off.
 *
 * @param[in] handle Points to the CAN controller context.
 *
 * @return int 0 for success or error code.
 */
int capi_can_recover(struct capi_can_controller_handle *handle);

/**
 * @brief CAN Driver Interrupt handler. If interrupt vectors are managed and implemented by user,
 * then user shall call this function in the relevant interrupt vector function.
 *
 * @param[in] handle Points to the CAN controller context
 */
void capi_can_isr(void *handle);

/**
 * @brief Structure holding CAN function pointers that point to the platform
 * specific function. See API functions for relevant descriptions.
 */
struct capi_can_ops {
	/** See capi_can_init() */
	int (*init)(struct capi_can_controller_handle **handle,
		    const struct capi_can_config *config);
	/** See capi_can_deinit() */
	int (*deinit)(struct capi_can_controller_handle *handle);
	/** See capi_can_start() */
	int (*start)(struct capi_can_controller_handle *handle);
	/** See capi_can_stop() */
	int (*stop)(struct capi_can_controller_handle *handle);
	/** See capi_can_set_timing() */
	int (*set_timing)(struct capi_can_controller_handle *handle,
			  const struct capi_can_timing *nominal,
			  const struct capi_can_timing *data);
	/** See capi_can_set_mode() */
	int (*set_mode)(struct capi_can_controller_handle *handle, enum capi_can_mode mode);
	/** See capi_can_get_capabilities() */
	int (*get_capabilities)(struct capi_can_controller_handle *handle, uint32_t *caps);
	/** See capi_can_add_filter() */
	int (*add_filter)(struct capi_can_controller_handle *handle,
			  const struct capi_can_filter *filter);
	/** See capi_can_remove_filter() */
	int (*remove_filter)(struct capi_can_controller_handle *handle, int filter_id);
	/** See capi_can_send() */
	int (*send)(struct capi_can_controller_handle *handle, const struct capi_can_frame *frame,
		    uint32_t timeout_ms);
	/** See capi_can_receive() */
	int (*receive)(struct capi_can_controller_handle *handle, struct capi_can_frame *frame,
		       uint32_t timeout_ms);
	/** See capi_can_send_async() */
	int (*send_async)(struct capi_can_controller_handle *handle,
			  const struct capi_can_frame *frame);
	/** See capi_can_register_callback() */
	int (*register_callback)(struct capi_can_controller_handle *handle,
				 capi_can_callback const callback, void *const callback_arg);
	/** See capi_can_get_error_counters() */
	int (*get_error_counters)(struct capi_can_controller_handle *handle,
				  struct capi_can_error_counters *counters);
	/** See capi_can_recover() */
	int (*recover)(struct capi_can_controller_handle *handle);
	/** See capi_can_isr() */
	void (*isr)(void *handle);
};

#if defined(__cplusplus)
}
#endif

#endif /* _CAPI_CAN_H_ */
