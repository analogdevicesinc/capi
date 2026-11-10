/*
 * Copyright (c) 2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file
 * @brief Common HAL Ethernet MAC API
 */

#ifndef _CAPI_ETH_MAC_H_
#define _CAPI_ETH_MAC_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Forward declarations -- avoid circular includes */
struct capi_eth_phy_handle;
struct capi_mdio_handle;

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Ethernet MAC address type (6 bytes)
 */
typedef uint8_t capi_eth_mac_addr[6];

/**
 * @brief Ethernet MAC PTP time structure
 */
struct capi_eth_mac_time {
	uint32_t ns;  /**< Nanoseconds */
	uint64_t sec; /**< Seconds */
};

/**
 * @brief Ethernet MAC capabilities
 */
struct capi_eth_mac_capabilities {
	uint32_t checksum_offload_rx_ip4: 1; /**< IPv4 header checksum verified on receive */
	uint32_t checksum_offload_rx_ip6: 1; /**< IPv6 checksum verification supported on receive */
	uint32_t checksum_offload_rx_udp: 1; /**< UDP payload checksum verified on receive */
	uint32_t checksum_offload_rx_tcp: 1; /**< TCP payload checksum verified on receive */
	uint32_t checksum_offload_rx_icmp: 1; /**< ICMP payload checksum verified on receive */
	uint32_t checksum_offload_tx_ip4: 1;  /**< IPv4 header checksum generated on transmit */
	uint32_t checksum_offload_tx_ip6: 1;  /**< IPv6 checksum generation supported on transmit */
	uint32_t checksum_offload_tx_udp: 1;  /**< UDP payload checksum generated on transmit */
	uint32_t checksum_offload_tx_tcp: 1;  /**< TCP payload checksum generated on transmit */
	uint32_t checksum_offload_tx_icmp: 1; /**< ICMP payload checksum generated on transmit */
	uint32_t media_interface: 3; /**< Ethernet Media Interface type (see capi_eth_interface) */
	uint32_t mac_address: 1;     /**< Driver provides initial valid MAC address */
	uint32_t event_rx_frame: 1;  /**< Callback event RX_FRAME generated */
	uint32_t event_tx_frame: 1;  /**< Callback event TX_FRAME generated */
	uint32_t event_wakeup: 1;    /**< Wakeup event generated */
	uint32_t ptp: 1;             /**< Hardware provides PTP primitives: free-running timer,
					  RX/TX frame timestamping, alarm, and frequency
					  adjustment. The IEEE 1588 protocol state machine is the
					  responsibility of the application. */
	uint32_t tx_queues: 3;       /**< Number of hardware TX priority queues (0 = single queue,
					  no priority support; 1-7 = number of queues available) */
	uint32_t reserved: 11;       /**< Reserved (must be zero) */
};

/**
 * @brief Ethernet MAC events
 */
enum capi_eth_mac_event {
	CAPI_ETH_MAC_EVENT_RX_FRAME = (1U << 0),  /**< Frame received, can be read */
	CAPI_ETH_MAC_EVENT_TX_FRAME = (1U << 1),  /**< Frame transmission complete */
	CAPI_ETH_MAC_EVENT_WAKEUP = (1U << 2),    /**< Magic Packet received (in Sleep mode) */
	CAPI_ETH_MAC_EVENT_PTP_ALARM = (1U << 3), /**< PTP alarm occurred */
};

/**
 * @brief Ethernet MAC link speed
 */
enum capi_eth_mac_speed {
	CAPI_ETH_MAC_SPEED_UNKNOWN, /**< Speed not yet determined */
	CAPI_ETH_MAC_SPEED_10M,     /**< 10 Mbps */
	CAPI_ETH_MAC_SPEED_100M,    /**< 100 Mbps */
	CAPI_ETH_MAC_SPEED_1G,      /**< 1 Gbps */
};

/**
 * @brief Ethernet MAC duplex mode
 */
enum capi_eth_mac_duplex {
	CAPI_ETH_MAC_DUPLEX_UNKNOWN, /**< Duplex not yet determined */
	CAPI_ETH_MAC_DUPLEX_HALF,    /**< Half duplex */
	CAPI_ETH_MAC_DUPLEX_FULL,    /**< Full duplex */
};

/**
 * @brief Ethernet MAC link configuration for capi_eth_mac_set_link_config()
 */
struct capi_eth_mac_link_config {
	enum capi_eth_mac_speed speed;
	enum capi_eth_mac_duplex duplex;
	bool loopback;
};

/**
 * @brief Ethernet VLAN filter configuration for capi_eth_mac_set_vlan_filter()
 */
struct capi_eth_vlan_filter {
	uint16_t tag;  /**< VLAN tag value (bits 0-11: VID, bits 12-15: PCP+DEI) */
	bool vid_only; /**< true = match 12-bit VID only, false = match full 16-bit tag */
	bool enabled;  /**< true = enable VLAN filter, false = disable */
};

/**
 * @brief Ethernet MAC power state
 */
enum capi_eth_mac_power_state {
	CAPI_ETH_MAC_POWER_OFF,  /**< Power off */
	CAPI_ETH_MAC_POWER_LOW,  /**< Low power mode */
	CAPI_ETH_MAC_POWER_FULL, /**< Full power mode */
};

/**
 * @brief Ethernet MAC initialization configuration
 */
struct capi_eth_mac_init_config {
	/** Optional platform specific extra configuration */
	void *extra;
	/** Platform specific implementation of the API (Mandatory field) */
	const struct capi_eth_mac_ops *ops;
};

/**
 * @brief Ethernet MAC handle
 *
 * Driver developer shall declare this as the first field of private handle structure.
 */
struct capi_eth_mac_handle {
	const struct capi_eth_mac_ops *ops; /**< Set and used by capi thin layer */
	bool init_allocated;                /**< If true, the driver is owner of handle memory */
	bool started;                       /**< true while the interface is running */
	void *lock; /**< Set and used by capi thin layer if locking is available */
	void *priv; /**< Set and used by user optionally */
	/**
	 * MDIO bus embedded in or driven by this MAC.
	 * Set by the driver during init. Callers use this to initialize
	 * discrete PHYs after the MAC is ready.
	 */
	struct capi_mdio_handle *mdio;
	/**
	 * PHY handle associated with this MAC.
	 * - Integrated MACPHY: set by the driver during init.
	 * - Discrete PHY: set by the caller via capi_eth_mac_set_phy() after
	 *   both the MAC and PHY have been initialized independently.
	 */
	struct capi_eth_phy_handle *phy;
};

/**
 * @brief Ethernet MAC statistics
 *
 * Counters are cumulative since the last reset or driver initialization.
 * Unsupported counters shall be set to zero by the driver.
 */
struct capi_eth_mac_stats {
	uint64_t rx_frames;         /**< Total frames received (good and bad) */
	uint64_t rx_octets;         /**< Total octets received (good frames only) */
	uint64_t rx_unicast;        /**< Unicast frames received */
	uint64_t rx_multicast;      /**< Multicast frames received */
	uint64_t rx_broadcast;      /**< Broadcast frames received */
	uint64_t rx_crc_errors;     /**< Frames discarded due to CRC error */
	uint64_t rx_align_errors;   /**< Frames discarded due to alignment error */
	uint64_t rx_overrun;        /**< Frames lost due to receive buffer overflow */
	uint64_t rx_drop_host_full; /**< Frames dropped: host RX FIFO full */
	uint64_t rx_drop_port_full; /**< Frames dropped: port RX FIFO full */
	uint64_t rx_drop_filtered;  /**< Frames dropped by the RX address/type filter */
	uint64_t tx_frames;         /**< Total frames transmitted */
	uint64_t tx_octets;         /**< Total octets transmitted */
	uint64_t tx_unicast;        /**< Unicast frames transmitted */
	uint64_t tx_multicast;      /**< Multicast frames transmitted */
	uint64_t tx_broadcast;      /**< Broadcast frames transmitted */
	uint64_t tx_errors;         /**< Frames not transmitted due to error */
	uint64_t tx_collisions;     /**< Total collisions (half-duplex only) */
};

/**
 * @brief Ethernet MAC frame buffer descriptor.
 *
 * Wraps a caller-supplied byte array and tracks head/end offsets so that
 * protocol layers can prepend or append data in-place without copying.
 * Use the capi_eth_fb_*() helpers to manipulate the offsets safely.
 *
 * Memory layout within buffer:
 *
 *   [0 ... head)        : headroom -- available for prepending headers.
 *   [head ... end)      : active frame data (DA + SA + EtherType + payload).
 *   [end ... max_length): tailroom -- available for appending data.
 */
struct capi_eth_frame_buffer {
	uint32_t data_length;               /**< Active frame length in bytes (end - head) */
	struct capi_eth_mac_time timestamp; /**< PTP timestamp.
						[TX] written after completion if capture_timestamp
					       is set. [RX] written by driver on receive. */
	struct capi_eth_frame_buffer *next; /**< Next fragment, NULL = last (or only) */

	bool fragment;          /**< [TX] true = more fragments follow in the chain */
	bool event;             /**< [TX] true = generate TX_FRAME event on completion */
	bool capture_timestamp; /**< [TX] true = capture TX timestamp into timestamp */
	uint8_t priority;       /**< [TX] Queue priority (0 = best-effort).
				     Valid range 0 to (tx_queues - 1) from capabilities.
				     Ignored if tx_queues capability is 0. */

	uint32_t head;       /**< Byte offset of first active frame byte */
	uint32_t end;        /**< Byte offset one past the last active byte */
	uint32_t max_length; /**< Total allocated size of buffer in bytes */

	void *buffer; /**< Pointer to the underlying memory allocation */
};

/**
 * @brief Initialize a frame buffer descriptor around a pre-allocated buffer.
 *
 * Sets head = end = head_start, data_length = 0, max_length = size,
 * and zeroes all control fields.
 *
 * @param[out] fb         Frame buffer descriptor to initialize.
 * @param[in]  buffer     Pre-allocated byte array.
 * @param[in]  size       Total size of buffer in bytes.
 * @param[in]  head_start Initial head offset (headroom reserved for prepending).
 *
 * @return int 0 for success, -EINVAL on bad parameters.
 */
int capi_eth_fb_init(struct capi_eth_frame_buffer *fb, uint8_t *buffer, uint32_t size,
		     uint32_t head_start);

/**
 * @brief Prepend data before head.
 *
 * Decrements head by size and copies data into the new headroom.
 * If data is NULL the head is moved but no copy is performed.
 *
 * @param[in,out] fb   Frame buffer descriptor.
 * @param[in]     data Data to copy, or NULL to just reserve space.
 * @param[in]     size Number of bytes to prepend.
 *
 * @return Pointer to the new head position, or NULL if insufficient headroom.
 */
uint8_t *capi_eth_fb_head_push(struct capi_eth_frame_buffer *fb, const uint8_t *data,
			       uint32_t size);

/**
 * @brief Strip data from the front of the active region.
 *
 * Advances head by size.
 *
 * @param[in,out] fb   Frame buffer descriptor.
 * @param[in]     size Number of bytes to strip.
 *
 * @return Pointer to the new head position, or NULL if size exceeds data_length.
 */
uint8_t *capi_eth_fb_head_pop(struct capi_eth_frame_buffer *fb, uint32_t size);

/**
 * @brief Append data after end.
 *
 * Advances end by size and copies data into the new tailroom.
 * If data is NULL the end is moved but no copy is performed.
 *
 * @param[in,out] fb   Frame buffer descriptor.
 * @param[in]     data Data to copy, or NULL to just reserve space.
 * @param[in]     size Number of bytes to append.
 *
 * @return Pointer to where the new data starts (old end), or NULL if
 *         insufficient tailroom.
 */
uint8_t *capi_eth_fb_end_push(struct capi_eth_frame_buffer *fb, const uint8_t *data, uint32_t size);

/**
 * @brief Trim data from the tail of the active region.
 *
 * Retracts end by size.
 *
 * @param[in,out] fb   Frame buffer descriptor.
 * @param[in]     size Number of bytes to trim.
 *
 * @return Pointer to the new end position, or NULL if size exceeds data_length.
 */
uint8_t *capi_eth_fb_end_pop(struct capi_eth_frame_buffer *fb, uint32_t size);

/**
 * @brief Return a pointer to the first active byte of the frame.
 *
 * @param[in] fb Frame buffer descriptor.
 *
 * @return Pointer to buffer[head], or NULL if fb is NULL.
 */
static inline uint8_t *capi_eth_fb_data(const struct capi_eth_frame_buffer *fb)
{
	return fb ? (uint8_t *)fb->buffer + fb->head : NULL;
}

/**
 * @brief Return the number of bytes available for prepending (headroom).
 *
 * @param[in] fb Frame buffer descriptor.
 *
 * @return Number of bytes before head, or 0 if fb is NULL.
 */
static inline uint32_t capi_eth_fb_headroom(const struct capi_eth_frame_buffer *fb)
{
	return fb ? fb->head : 0;
}

/**
 * @brief Return the number of bytes available for appending (tailroom).
 *
 * @param[in] fb Frame buffer descriptor.
 *
 * @return Number of bytes after end, or 0 if fb is NULL.
 */
static inline uint32_t capi_eth_fb_tailroom(const struct capi_eth_frame_buffer *fb)
{
	return fb ? fb->max_length - fb->end : 0;
}

/**
 * @brief Ethernet MAC Event Callback type
 * @param[in] event Ethernet MAC event flags (see capi_eth_mac_event)
 * @param[in] arg Pointer to user specific data
 */
typedef void (*capi_eth_mac_event_callback)(uint32_t event, void *arg);

/**
 * @brief Get Ethernet MAC statistics.
 *
 * Retrieves cumulative statistics counters, aggregated across all egress
 * queues for a multi-queue device. Counters are not reset by this call. To
 * reset counters, reinitialize the driver.
 *
 * @param[in]  handle Points to the Ethernet MAC context.
 * @param[out] stats  Returns the current statistics counters.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_get_stats(struct capi_eth_mac_handle *handle, struct capi_eth_mac_stats *stats);

/**
 * @brief Get per-queue Ethernet MAC statistics.
 *
 * Retrieves the cumulative statistics counters for a single egress queue,
 * from that queue's dedicated hardware counter bank. Counters are not reset by
 * this call. Octet totals, which are tracked in software without a per-queue
 * breakdown, are reported only by capi_eth_mac_get_stats() and are zero here.
 *
 * @param[in]  handle Points to the Ethernet MAC context.
 * @param[in]  queue  Queue index, 0 to (tx_queues - 1) from capabilities.
 * @param[out] stats  Returns the current statistics counters for that queue.
 *
 * @return int 0 for success, -EINVAL on a bad queue index or arguments.
 */
int capi_eth_mac_get_queue_stats(struct capi_eth_mac_handle *handle, unsigned int queue,
				 struct capi_eth_mac_stats *stats);

/**
 * @brief Initialize an instance of the Ethernet MAC controller.
 *
 * @param[in,out] handle Points to a pointer of the context structure. If this
 *                       pointer is set to NULL, then the controller will allocate
 *                       the context structure and be the owner. If the pointer
 *                       is not NULL, the application has allocated the structure and is
 *                       the owner.
 * @param[in] config Points to the initialization configuration for the Ethernet MAC.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_init(struct capi_eth_mac_handle **handle,
		      const struct capi_eth_mac_init_config *config);

/**
 * @brief Deinitialize the Ethernet MAC controller.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_deinit(struct capi_eth_mac_handle *handle);

/**
 * @brief Control Ethernet MAC Device Power.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] state Power state to set.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_power_control(struct capi_eth_mac_handle *handle,
			       enum capi_eth_mac_power_state state);

/**
 * @brief Get Ethernet MAC capabilities.
 *
 * @param[in]  handle       Points to the Ethernet MAC context.
 * @param[out] capabilities Returns the MAC capabilities.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_get_capabilities(struct capi_eth_mac_handle *handle,
				  struct capi_eth_mac_capabilities *capabilities);

/**
 * @brief Get Ethernet MAC Address.
 *
 * @param[in]  handle Points to the Ethernet MAC context.
 * @param[out] addr   Returns the MAC address (6 bytes).
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_get_address(struct capi_eth_mac_handle *handle, capi_eth_mac_addr addr);

/**
 * @brief Set Ethernet MAC Address.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] addr   MAC address to set (6 bytes).
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_set_address(struct capi_eth_mac_handle *handle, const capi_eth_mac_addr addr);

/**
 * @brief Add an address to the MAC address filter table.
 *
 * Adds a single address to the next available filter slot. The slot number
 * assigned by the driver is written to @p slot so the caller can reference
 * it later (e.g. to remove the entry via capi_eth_mac_clear_address_filter()).
 *
 * @param[in]  handle Points to the Ethernet MAC context.
 * @param[in]  addr   MAC address to add to the filter table (6 bytes).
 * @param[out] slot   Returns the filter slot index assigned to this address.
 *
 * @return int 0 for success, -ENOSPC if the filter table is full, or error code.
 */
int capi_eth_mac_set_address_filter(struct capi_eth_mac_handle *handle,
				    const capi_eth_mac_addr addr, uint8_t *slot);

/**
 * @brief Remove an address from the MAC address filter table.
 *
 * Clears the filter slot previously returned by capi_eth_mac_set_address_filter().
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] slot   Filter slot index to clear.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_clear_address_filter(struct capi_eth_mac_handle *handle, uint8_t slot);

/**
 * @brief Send Ethernet frame.
 *
 * The active frame data at buffer[head .. end) must contain
 * DA + SA + EtherType/Length + payload.  The hardware generates and appends
 * the FCS; it must not be included in the buffer.  The driver may decrement
 * frame->head to prepend a transport header in-place before transmission.
 * TX control flags (fragment, event, capture_timestamp, priority) are read
 * from the descriptor.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] frame  Frame buffer descriptor (see capi_eth_frame_buffer).
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_send_frame(struct capi_eth_mac_handle *handle,
			    const struct capi_eth_frame_buffer *frame);

/**
 * @brief Get size of the next received Ethernet frame.
 *
 * Call this before capi_eth_mac_read_frame() to determine the required buffer
 * size. Returns 0 in size when no frame is available.
 *
 * @param[in]  handle Points to the Ethernet MAC context.
 * @param[out] size   Size of the pending frame in bytes (0 if none available).
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_get_rx_frame_size(struct capi_eth_mac_handle *handle, uint32_t *size);

/**
 * @brief Read the next received Ethernet frame.
 *
 * The caller provides a frame buffer descriptor with buffer pointing to an
 * allocation of at least max_length bytes and head set to the desired
 * start offset (normally 0).  The driver writes the received frame into
 * buffer[head .. head+frame_size) and sets data_length and end
 * accordingly.  If the hardware supports PTP timestamping, timestamp is
 * populated; otherwise it is left unchanged.  Discard the pending frame
 * without copying by passing buffer = NULL (driver behaviour
 * implementation-defined).
 *
 * @param[in]     handle Points to the Ethernet MAC context.
 * @param[in,out] frame  Frame buffer descriptor to receive into (see capi_eth_frame_buffer).
 *
 * @return int Number of bytes written into the buffer (>=0) or error code (<0).
 */
int capi_eth_mac_read_frame(struct capi_eth_mac_handle *handle,
			    struct capi_eth_frame_buffer *frame);

/**
 * @brief Set Ethernet MAC link parameters (speed, duplex, loopback).
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] config Link configuration.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_set_link_config(struct capi_eth_mac_handle *handle,
				 const struct capi_eth_mac_link_config *config);

/**
 * @brief Enable or disable the broadcast address filter.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] enable true = accept broadcast frames, false = reject.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_set_broadcast(struct capi_eth_mac_handle *handle, bool enable);

/**
 * @brief Enable or disable the all-multicast address filter.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] enable true = accept all multicast frames, false = reject.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_set_all_multicast(struct capi_eth_mac_handle *handle, bool enable);

/**
 * @brief Enable or disable promiscuous mode.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] enable true = accept all frames, false = filter by address.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_set_promiscuous(struct capi_eth_mac_handle *handle, bool enable);

/**
 * @brief Start the Ethernet MAC interface (enable TX and RX).
 *
 * @param[in] handle Points to the Ethernet MAC context.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_start(struct capi_eth_mac_handle *handle);

/**
 * @brief Stop the Ethernet MAC interface (disable TX and RX).
 *
 * @param[in] handle Points to the Ethernet MAC context.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_stop(struct capi_eth_mac_handle *handle);

/**
 * @brief Flush Ethernet MAC receive buffer.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_flush_rx_buffer(struct capi_eth_mac_handle *handle);

/**
 * @brief Flush Ethernet MAC transmit buffer.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_flush_tx_buffer(struct capi_eth_mac_handle *handle);

/**
 * @brief Control Ethernet MAC sleep mode.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] enable true = enter sleep and wait for Magic Packet, false = exit sleep.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_sleep(struct capi_eth_mac_handle *handle, bool enable);

/**
 * @brief Configure Ethernet MAC VLAN filter.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] filter VLAN filter configuration (see capi_eth_vlan_filter).
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_set_vlan_filter(struct capi_eth_mac_handle *handle,
				 const struct capi_eth_vlan_filter *filter);

/**
 * @brief Get current PTP clock time.
 *
 * @param[in]  handle Points to the Ethernet MAC context.
 * @param[out] time   Returns the current PTP clock time.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_ptp_get_time(struct capi_eth_mac_handle *handle, struct capi_eth_mac_time *time);

/**
 * @brief Set PTP clock time to an absolute value.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] time   PTP clock time to set.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_ptp_set_time(struct capi_eth_mac_handle *handle,
			      const struct capi_eth_mac_time *time);

/**
 * @brief Increment PTP clock time by a given offset.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] time   Time offset to add.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_ptp_inc_time(struct capi_eth_mac_handle *handle,
			      const struct capi_eth_mac_time *time);

/**
 * @brief Decrement PTP clock time by a given offset.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] time   Time offset to subtract.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_ptp_dec_time(struct capi_eth_mac_handle *handle,
			      const struct capi_eth_mac_time *time);

/**
 * @brief Set PTP alarm time.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] time   Alarm target time.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_ptp_set_alarm(struct capi_eth_mac_handle *handle,
			       const struct capi_eth_mac_time *time);

/**
 * @brief Adjust PTP clock frequency for drift correction.
 *
 * @param[in] handle          Points to the Ethernet MAC context.
 * @param[in] correction_q31  Frequency correction factor in Q31 fixed-point format.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_ptp_adjust_freq(struct capi_eth_mac_handle *handle, int32_t correction_q31);

/**
 * @brief Register callback for Ethernet MAC events.
 *
 * @param[in] handle        Points to the Ethernet MAC context.
 * @param[in] callback      User callback function.
 * @param[in] callback_arg  User callback argument.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_register_callback(struct capi_eth_mac_handle *handle,
				   capi_eth_mac_event_callback const callback,
				   void *const callback_arg);

/**
 * @brief Get the PHY handle associated with this MAC.
 *
 * @param[in]  handle Points to the Ethernet MAC context.
 * @param[out] phy    Returns the associated PHY handle, or NULL if none.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_get_phy(struct capi_eth_mac_handle *handle, struct capi_eth_phy_handle **phy);

/**
 * @brief Associate a PHY with this MAC and configure the MAC-PHY interface.
 *
 * Stores the PHY handle in handle->phy. If the MAC implements
 * get_capabilities(), its reported media_interface type is passed to
 * capi_eth_phy_set_interface() automatically; if either is unimplemented the
 * step is skipped. Pass NULL to dissociate the current PHY.
 *
 * For integrated MACPHYs capi_eth_mac_init() calls this automatically when
 * the driver pre-populates handle->phy, so the caller does not need to call
 * it explicitly.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] phy    PHY handle to associate, or NULL to clear.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_set_phy(struct capi_eth_mac_handle *handle, struct capi_eth_phy_handle *phy);

/**
 * @brief Read an Ethernet MAC register.
 *
 * @param[in]  handle Points to the Ethernet MAC context.
 * @param[in]  addr   Register address (device-specific).
 * @param[out] data   Returns the register value.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_reg_read(struct capi_eth_mac_handle *handle, uint32_t addr, uint32_t *data);

/**
 * @brief Write an Ethernet MAC register.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 * @param[in] addr   Register address (device-specific).
 * @param[in] data   Value to write.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_mac_reg_write(struct capi_eth_mac_handle *handle, uint32_t addr, uint32_t data);

/**
 * @brief Process pending receive data from non-interrupt context.
 *
 * Called from the application main loop after the ISR has flagged that work
 * is pending. The driver performs the actual bus transfers (e.g. SPI chunk
 * reads for a MACPHY) and delivers completed frames via the registered
 * RX event callback.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 *
 * @return int Number of frames processed (>= 0) or error code (< 0).
 */
int capi_eth_mac_poll(struct capi_eth_mac_handle *handle);

/**
 * @brief Ethernet MAC Driver Interrupt handler.
 *
 * If interrupt vectors are managed by the user, call this function from the
 * relevant interrupt vector.
 *
 * @param[in] handle Points to the Ethernet MAC context.
 */
void capi_eth_mac_isr(void *handle);

/**
 * @brief Structure holding Ethernet MAC function pointers.
 * See API functions for relevant descriptions.
 */
struct capi_eth_mac_ops {
	/** See capi_eth_mac_init() */
	int (*init)(struct capi_eth_mac_handle **handle,
		    const struct capi_eth_mac_init_config *config);
	/** See capi_eth_mac_deinit() */
	int (*deinit)(struct capi_eth_mac_handle *handle);
	/** See capi_eth_mac_power_control() */
	int (*power_control)(struct capi_eth_mac_handle *handle,
			     enum capi_eth_mac_power_state state);
	/** See capi_eth_mac_get_capabilities() */
	int (*get_capabilities)(struct capi_eth_mac_handle *handle,
				struct capi_eth_mac_capabilities *capabilities);
	/** See capi_eth_mac_get_address() */
	int (*get_address)(struct capi_eth_mac_handle *handle, capi_eth_mac_addr addr);
	/** See capi_eth_mac_set_address() */
	int (*set_address)(struct capi_eth_mac_handle *handle, const capi_eth_mac_addr addr);
	/** See capi_eth_mac_set_address_filter() */
	int (*set_address_filter)(struct capi_eth_mac_handle *handle, const capi_eth_mac_addr addr,
				  uint8_t *slot);
	/** See capi_eth_mac_clear_address_filter() */
	int (*clear_address_filter)(struct capi_eth_mac_handle *handle, uint8_t slot);
	/** See capi_eth_mac_send_frame() */
	int (*send_frame)(struct capi_eth_mac_handle *handle,
			  const struct capi_eth_frame_buffer *frame);
	/** See capi_eth_mac_get_rx_frame_size() */
	int (*get_rx_frame_size)(struct capi_eth_mac_handle *handle, uint32_t *size);
	/** See capi_eth_mac_read_frame() */
	int (*read_frame)(struct capi_eth_mac_handle *handle, struct capi_eth_frame_buffer *frame);
	/** See capi_eth_mac_set_link_config() */
	int (*set_link_config)(struct capi_eth_mac_handle *handle,
			       const struct capi_eth_mac_link_config *config);
	/** See capi_eth_mac_set_broadcast() */
	int (*set_broadcast)(struct capi_eth_mac_handle *handle, bool enable);
	/** See capi_eth_mac_set_all_multicast() */
	int (*set_all_multicast)(struct capi_eth_mac_handle *handle, bool enable);
	/** See capi_eth_mac_set_promiscuous() */
	int (*set_promiscuous)(struct capi_eth_mac_handle *handle, bool enable);
	/** See capi_eth_mac_start() */
	int (*start)(struct capi_eth_mac_handle *handle);
	/** See capi_eth_mac_stop() */
	int (*stop)(struct capi_eth_mac_handle *handle);
	/** See capi_eth_mac_flush_rx_buffer() */
	int (*flush_rx_buffer)(struct capi_eth_mac_handle *handle);
	/** See capi_eth_mac_flush_tx_buffer() */
	int (*flush_tx_buffer)(struct capi_eth_mac_handle *handle);
	/** See capi_eth_mac_sleep() */
	int (*sleep)(struct capi_eth_mac_handle *handle, bool enable);
	/** See capi_eth_mac_set_vlan_filter() */
	int (*set_vlan_filter)(struct capi_eth_mac_handle *handle,
			       const struct capi_eth_vlan_filter *filter);
	/** See capi_eth_mac_ptp_get_time() */
	int (*ptp_get_time)(struct capi_eth_mac_handle *handle, struct capi_eth_mac_time *time);
	/** See capi_eth_mac_ptp_set_time() */
	int (*ptp_set_time)(struct capi_eth_mac_handle *handle,
			    const struct capi_eth_mac_time *time);
	/** See capi_eth_mac_ptp_inc_time() */
	int (*ptp_inc_time)(struct capi_eth_mac_handle *handle,
			    const struct capi_eth_mac_time *time);
	/** See capi_eth_mac_ptp_dec_time() */
	int (*ptp_dec_time)(struct capi_eth_mac_handle *handle,
			    const struct capi_eth_mac_time *time);
	/** See capi_eth_mac_ptp_set_alarm() */
	int (*ptp_set_alarm)(struct capi_eth_mac_handle *handle,
			     const struct capi_eth_mac_time *time);
	/** See capi_eth_mac_ptp_adjust_freq() */
	int (*ptp_adjust_freq)(struct capi_eth_mac_handle *handle, int32_t correction_q31);
	/** See capi_eth_mac_get_stats() */
	int (*get_stats)(struct capi_eth_mac_handle *handle, struct capi_eth_mac_stats *stats);
	/** See capi_eth_mac_get_queue_stats() */
	int (*get_queue_stats)(struct capi_eth_mac_handle *handle, unsigned int queue,
			       struct capi_eth_mac_stats *stats);
	/** See capi_eth_mac_register_callback() */
	int (*register_callback)(struct capi_eth_mac_handle *handle,
				 capi_eth_mac_event_callback const callback,
				 void *const callback_arg);
	/** See capi_eth_mac_reg_read() */
	int (*reg_read)(struct capi_eth_mac_handle *handle, uint32_t addr, uint32_t *data);
	/** See capi_eth_mac_reg_write() */
	int (*reg_write)(struct capi_eth_mac_handle *handle, uint32_t addr, uint32_t data);
	/** See capi_eth_mac_poll() */
	int (*poll)(struct capi_eth_mac_handle *handle);
	/** See capi_eth_mac_isr() */
	void (*isr)(void *handle);
};

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _CAPI_ETH_MAC_H_ */
