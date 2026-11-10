/*
 * Copyright (c) 2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file
 * @brief Common HAL Ethernet PHY API
 */

#ifndef _CAPI_ETH_PHY_H_
#define _CAPI_ETH_PHY_H_

#include <stdbool.h>
#include <stdint.h>
#include <capi_mdio.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Ethernet link state
 */
enum capi_eth_link_state {
	CAPI_ETH_LINK_DOWN, /**< Link is down */
	CAPI_ETH_LINK_UP    /**< Link is up */
};

/**
 * @brief Ethernet PHY link speed
 */
enum capi_eth_phy_speed {
	CAPI_ETH_PHY_SPEED_UNKNOWN, /**< Speed not yet determined (link down or autoneg in progress)
				     */
	CAPI_ETH_PHY_SPEED_10M,     /**< 10 Mbps */
	CAPI_ETH_PHY_SPEED_100M,    /**< 100 Mbps */
	CAPI_ETH_PHY_SPEED_1G,      /**< 1 Gbps */
};

/**
 * @brief Ethernet PHY duplex mode
 */
enum capi_eth_phy_duplex {
	CAPI_ETH_PHY_DUPLEX_UNKNOWN, /**< Duplex not yet determined (link down or autoneg in
					progress) */
	CAPI_ETH_PHY_DUPLEX_HALF,    /**< Half duplex */
	CAPI_ETH_PHY_DUPLEX_FULL,    /**< Full duplex */
};

/**
 * @brief Ethernet MDI/MDIX crossover mode
 */
enum capi_eth_mdix {
	CAPI_ETH_MDIX_AUTO, /**< Auto-MDIX: PHY detects and corrects cable crossover automatically
			     */
	CAPI_ETH_MDIX_MDI,  /**< Force MDI (straight-through cable) */
	CAPI_ETH_MDIX_MDIX, /**< Force MDIX (crossover cable) */
};

/**
 * @brief Ethernet link information
 */
struct capi_eth_link_info {
	enum capi_eth_link_state state;  /**< Link state (up/down) */
	enum capi_eth_phy_speed speed;   /**< Negotiated link speed */
	enum capi_eth_phy_duplex duplex; /**< Negotiated duplex mode */
	enum capi_eth_mdix mdix; /**< Resolved MDI/MDIX state (AUTO if not yet determined) */
};

/**
 * @brief Ethernet PHY media interface type
 *
 * Passed to capi_eth_phy_set_interface() to specify the MAC-PHY interconnect.
 */
enum capi_eth_interface {
	CAPI_ETH_INTERFACE_INTERNAL, /**< Integrated MACPHY -- no external media interface */
	CAPI_ETH_INTERFACE_MII,      /**< Media Independent Interface (MII) */
	CAPI_ETH_INTERFACE_RMII,     /**< Reduced Media Independent Interface (RMII) */
	CAPI_ETH_INTERFACE_SMII,     /**< Serial Media Independent Interface (SMII) */
	CAPI_ETH_INTERFACE_RGMII,    /**< Reduced Gigabit Media Independent Interface (RGMII) */
	CAPI_ETH_INTERFACE_SGMII,    /**< Serial Gigabit Media Independent Interface (SGMII) */
};

/**
 * @brief Ethernet PHY operation mode configuration for capi_eth_phy_set_mode()
 */
struct capi_eth_phy_mode_config {
	enum capi_eth_phy_speed speed;   /**< Desired link speed */
	enum capi_eth_phy_duplex duplex; /**< Desired duplex mode */
	bool auto_negotiate;             /**< true = auto-negotiate transmission parameters */
	bool loopback;                   /**< true = loopback test mode */
	bool isolate;                    /**< true = isolate PHY from MAC interface */
	enum capi_eth_mdix mdix;         /**< MDI/MDIX crossover mode (AUTO, MDI, or MDIX) */
};

/**
 * @brief Ethernet PHY events
 */
enum capi_eth_phy_event {
	CAPI_ETH_PHY_EVENT_LINK_UP = (1U << 0),   /**< Link has come up */
	CAPI_ETH_PHY_EVENT_LINK_DOWN = (1U << 1), /**< Link has gone down */
};

/**
 * @brief Ethernet PHY power state
 */
enum capi_eth_phy_power_state {
	CAPI_ETH_PHY_POWER_OFF,  /**< Power off */
	CAPI_ETH_PHY_POWER_LOW,  /**< Low power mode */
	CAPI_ETH_PHY_POWER_FULL, /**< Full power mode */
};

/**
 * @brief PLCA (Physical Layer Collision Avoidance) RS configuration.
 *
 * Modelled after IEEE 802.3cg Clause 148 and the Linux kernel's
 * phy_plca_cfg structure.
 */
struct capi_eth_phy_plca_cfg {
	bool enabled;     /**< true = PLCA enabled, false = CSMA/CD mode */
	uint8_t node_id;  /**< Local node identifier (0 = coordinator, 1-254 = follower, 255 = node
			     disabled) */
	uint8_t node_cnt; /**< Maximum node count (coordinator only, 1-255) */
	uint8_t to_tmr; /**< Transmit opportunity timer in bit-times (must be equal on all nodes) */
	uint8_t burst_cnt; /**< Max additional frames per transmit opportunity (0 = one frame per
			      TO) */
	uint8_t burst_tmr; /**< Inter-burst timer in bit-times (should exceed MAC IPG, typically
			      >96) */
};

/**
 * @brief PLCA RS status information.
 */
struct capi_eth_phy_plca_status {
	bool pst; /**< PLCA Status Transmit: true when BEACON activity is detected */
};

/**
 * @brief Ethernet PHY Event Callback type
 * @param[in] event Ethernet PHY event flags (see capi_eth_phy_event)
 * @param[in] arg   Pointer to user specific data
 */
typedef void (*capi_eth_phy_event_callback)(uint32_t event, void *arg);

/**
 * @brief Ethernet PHY initialization configuration
 */
struct capi_eth_phy_init_config {
	/**
	 * MDIO bus handle used to reach this PHY's registers.
	 * Mandatory for discrete PHY drivers.  For an integrated MACPHY whose
	 * PHY driver is instantiated internally by the MAC driver, the MAC
	 * driver may set this to NULL and supply its own register access
	 * directly through the ops.
	 */
	struct capi_mdio_handle *mdio;
	/** PHY device address on the MDIO bus (0-31). Set to 0 to auto-scan. */
	uint8_t phy_addr;
	/** Optional platform specific extra configuration */
	void *extra;
	/** Platform specific implementation of the API (Mandatory field) */
	const struct capi_eth_phy_ops *ops;
};

/**
 * @brief Ethernet PHY handle
 *
 * Driver developer shall declare this as the first field of private handle structure.
 */
struct capi_eth_phy_handle {
	const struct capi_eth_phy_ops *ops; /**< Set and used by capi thin layer */
	bool init_allocated;                /**< If true, the driver is owner of handle memory */
	void *lock; /**< Set and used by capi thin layer if locking is available */
	void *priv; /**< Set and used by user optionally */
	/**
	 * MDIO bus this PHY sits on. Copied from init config by the thin
	 * layer after ops->init succeeds. NULL for integrated MACPHYs.
	 */
	struct capi_mdio_handle *mdio;
	/** PHY address on the MDIO bus, resolved by capi_eth_phy_init(). */
	uint8_t phy_addr;
};

/**
 * @brief Initialize an instance of the Ethernet PHY driver.
 *
 * @param[in,out] handle Points to a pointer of the context structure. If this
 *                       pointer is set to NULL, then the driver will allocate
 *                       the context structure and be the owner.
 * @param[in] config     Points to the initialization configuration for the PHY.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_init(struct capi_eth_phy_handle **handle,
		      const struct capi_eth_phy_init_config *config);

/**
 * @brief Deinitialize the Ethernet PHY driver.
 *
 * @param[in] handle Points to the Ethernet PHY context.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_deinit(struct capi_eth_phy_handle *handle);

/**
 * @brief Control Ethernet PHY Device Power.
 *
 * @param[in] handle Points to the Ethernet PHY context.
 * @param[in] state  Power state to set.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_power_control(struct capi_eth_phy_handle *handle,
			       enum capi_eth_phy_power_state state);

/**
 * @brief Set Ethernet Media Interface between MAC and PHY.
 *
 * Should be called after init using the interface type reported by
 * capi_eth_mac_get_capabilities().media_interface.
 *
 * @param[in] handle    Points to the Ethernet PHY context.
 * @param[in] interface Media interface type (see capi_eth_interface).
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_set_interface(struct capi_eth_phy_handle *handle,
			       enum capi_eth_interface interface);

/**
 * @brief Set Ethernet PHY operation mode.
 *
 * When config->auto_negotiate is true, config->speed and config->duplex
 * are used as the advertised capability; the PHY will still auto-negotiate
 * and may settle on a lower setting.
 *
 * @param[in] handle Points to the Ethernet PHY context.
 * @param[in] config Desired PHY mode (see capi_eth_phy_mode_config).
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_set_mode(struct capi_eth_phy_handle *handle,
			  const struct capi_eth_phy_mode_config *config);

/**
 * @brief Get Ethernet PHY link state.
 *
 * @param[in]  handle Points to the Ethernet PHY context.
 * @param[out] state  Returns the current link state.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_get_link_state(struct capi_eth_phy_handle *handle,
				enum capi_eth_link_state *state);

/**
 * @brief Get Ethernet PHY link information (state, speed, duplex).
 *
 * @param[in]  handle Points to the Ethernet PHY context.
 * @param[out] info   Returns the current link information.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_get_link_info(struct capi_eth_phy_handle *handle, struct capi_eth_link_info *info);

/**
 * @brief Register callback for Ethernet PHY events.
 *
 * @param[in] handle        Points to the Ethernet PHY context.
 * @param[in] callback      User callback function.
 * @param[in] callback_arg  User callback argument.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_register_callback(struct capi_eth_phy_handle *handle,
				   capi_eth_phy_event_callback const callback,
				   void *const callback_arg);

/**
 * @brief Read a PHY register using Clause 22 access.
 *
 * Convenience wrapper: forwards to capi_mdio_read() using the MDIO
 * handle and PHY address stored in the PHY handle.
 *
 * @param[in]  handle Points to the Ethernet PHY context.
 * @param[in]  reg    Register address (5-bit, 0-31).
 * @param[out] data   Returns the 16-bit register value.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_read_c22(struct capi_eth_phy_handle *handle, uint8_t reg, uint16_t *data);

/**
 * @brief Write a PHY register using Clause 22 access.
 *
 * Convenience wrapper: forwards to capi_mdio_write() using the MDIO
 * handle and PHY address stored in the PHY handle.
 *
 * @param[in] handle Points to the Ethernet PHY context.
 * @param[in] reg    Register address (5-bit, 0-31).
 * @param[in] data   16-bit data to write.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_write_c22(struct capi_eth_phy_handle *handle, uint8_t reg, uint16_t data);

/**
 * @brief Read a PHY register using Clause 45 access.
 *
 * Convenience wrapper: forwards to capi_mdio_read_c45() using the MDIO
 * handle and PHY address stored in the PHY handle.
 *
 * @param[in]  handle Points to the Ethernet PHY context.
 * @param[in]  devnum MMD device number (e.g. 1 = PMA/PMD, 3 = PCS, 31 = Vendor Specific 2).
 * @param[in]  reg    16-bit register address within the MMD.
 * @param[out] data   Returns the 16-bit register value.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_read_c45(struct capi_eth_phy_handle *handle, uint8_t devnum, uint16_t reg,
			  uint16_t *data);

/**
 * @brief Write a PHY register using Clause 45 access.
 *
 * Convenience wrapper: forwards to capi_mdio_write_c45() using the MDIO
 * handle and PHY address stored in the PHY handle.
 *
 * @param[in] handle Points to the Ethernet PHY context.
 * @param[in] devnum MMD device number (e.g. 1 = PMA/PMD, 3 = PCS, 31 = Vendor Specific 2).
 * @param[in] reg    16-bit register address within the MMD.
 * @param[in] data   16-bit data to write.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_write_c45(struct capi_eth_phy_handle *handle, uint8_t devnum, uint16_t reg,
			   uint16_t data);

/**
 * @brief Get the current PLCA RS configuration.
 *
 * @param[in]  handle Points to the Ethernet PHY context.
 * @param[out] cfg    Returns the current PLCA configuration.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_plca_get_cfg(struct capi_eth_phy_handle *handle,
			      struct capi_eth_phy_plca_cfg *cfg);

/**
 * @brief Set the PLCA RS configuration.
 *
 * @param[in] handle Points to the Ethernet PHY context.
 * @param[in] cfg    PLCA configuration to apply.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_plca_set_cfg(struct capi_eth_phy_handle *handle,
			      const struct capi_eth_phy_plca_cfg *cfg);

/**
 * @brief Get the current PLCA RS status.
 *
 * @param[in]  handle Points to the Ethernet PHY context.
 * @param[out] status Returns the current PLCA status.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_plca_get_status(struct capi_eth_phy_handle *handle,
				 struct capi_eth_phy_plca_status *status);

/**
 * @brief Poll the Ethernet PHY driver for events.
 *
 * Call periodically when the PHY INT pin is not connected to a hardware
 * interrupt. The driver checks for link-state changes and fires registered
 * callbacks as if the interrupt had fired.
 *
 * If the driver does not implement the poll op (e.g. it is purely interrupt-
 * driven), this function returns 0 without doing anything.
 *
 * @param[in] handle Points to the Ethernet PHY context.
 *
 * @return int 0 for success or error code.
 */
int capi_eth_phy_poll(struct capi_eth_phy_handle *handle);

/**
 * @brief Ethernet PHY Driver Interrupt handler.
 *
 * If interrupt vectors are managed by the user, call this function from the
 * relevant interrupt vector.
 *
 * @param[in] handle Points to the Ethernet PHY context.
 */
void capi_eth_phy_isr(void *handle);

/**
 * @brief Structure holding Ethernet PHY function pointers.
 * See API functions for relevant descriptions.
 */
struct capi_eth_phy_ops {
	/** See capi_eth_phy_init() */
	int (*init)(struct capi_eth_phy_handle **handle,
		    const struct capi_eth_phy_init_config *config);
	/** See capi_eth_phy_deinit() */
	int (*deinit)(struct capi_eth_phy_handle *handle);
	/** See capi_eth_phy_power_control() */
	int (*power_control)(struct capi_eth_phy_handle *handle,
			     enum capi_eth_phy_power_state state);
	/** See capi_eth_phy_set_interface() */
	int (*set_interface)(struct capi_eth_phy_handle *handle, enum capi_eth_interface interface);
	/** See capi_eth_phy_set_mode() */
	int (*set_mode)(struct capi_eth_phy_handle *handle,
			const struct capi_eth_phy_mode_config *config);
	/** See capi_eth_phy_get_link_state() */
	int (*get_link_state)(struct capi_eth_phy_handle *handle, enum capi_eth_link_state *state);
	/** See capi_eth_phy_get_link_info() */
	int (*get_link_info)(struct capi_eth_phy_handle *handle, struct capi_eth_link_info *info);
	/** See capi_eth_phy_plca_get_cfg() */
	int (*plca_get_cfg)(struct capi_eth_phy_handle *handle, struct capi_eth_phy_plca_cfg *cfg);
	/** See capi_eth_phy_plca_set_cfg() */
	int (*plca_set_cfg)(struct capi_eth_phy_handle *handle,
			    const struct capi_eth_phy_plca_cfg *cfg);
	/** See capi_eth_phy_plca_get_status() */
	int (*plca_get_status)(struct capi_eth_phy_handle *handle,
			       struct capi_eth_phy_plca_status *status);
	/** See capi_eth_phy_register_callback() */
	int (*register_callback)(struct capi_eth_phy_handle *handle,
				 capi_eth_phy_event_callback const callback,
				 void *const callback_arg);
	/** See capi_eth_phy_poll() */
	int (*poll)(struct capi_eth_phy_handle *handle);
	/** See capi_eth_phy_isr() */
	void (*isr)(void *handle);
};

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _CAPI_ETH_PHY_H_ */
