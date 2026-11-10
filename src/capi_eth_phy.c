/*
 * Copyright (c) 2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_eth_phy.c
 * @brief Thin wrapper functions that dispatch Ethernet PHY driver calls
 *        through the ops pointer structure.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <capi_mdio.h>
#include <capi_eth_phy.h>

int capi_eth_phy_init(struct capi_eth_phy_handle **handle,
		      const struct capi_eth_phy_init_config *config)
{
	struct capi_eth_phy_init_config patched;
	uint8_t found;
	int ret;

	if (handle == NULL || config == NULL || config->ops == NULL || config->ops->init == NULL) {
		return -EINVAL;
	}

	if (config->phy_addr == 0 && config->mdio != NULL) {
		ret = capi_mdio_scan(config->mdio, &found);
		if (ret != 0) {
			return ret;
		}

		patched = *config;
		patched.phy_addr = found;
		config = &patched;
	}

	ret = config->ops->init(handle, config);
	if (ret != 0) {
		return ret;
	}

	(*handle)->mdio = config->mdio;
	(*handle)->phy_addr = config->phy_addr;

	return 0;
}

int capi_eth_phy_deinit(struct capi_eth_phy_handle *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->deinit != NULL) {
		return handle->ops->deinit(handle);
	}

	return -EINVAL;
}

int capi_eth_phy_power_control(struct capi_eth_phy_handle *handle,
			       enum capi_eth_phy_power_state state)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->power_control != NULL) {
		return handle->ops->power_control(handle, state);
	}

	return -EINVAL;
}

int capi_eth_phy_set_interface(struct capi_eth_phy_handle *handle,
			       enum capi_eth_interface interface)
{
	if (handle == NULL) {
		return -EINVAL;
	}

	/* Integrated MACPHYs have no external media interface to configure. */
	if (interface == CAPI_ETH_INTERFACE_INTERNAL) {
		return 0;
	}

	if (handle->ops == NULL || handle->ops->set_interface == NULL) {
		return 0;
	}

	return handle->ops->set_interface(handle, interface);
}

int capi_eth_phy_set_mode(struct capi_eth_phy_handle *handle,
			  const struct capi_eth_phy_mode_config *config)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->set_mode != NULL) {
		return handle->ops->set_mode(handle, config);
	}

	return -EINVAL;
}

int capi_eth_phy_get_link_state(struct capi_eth_phy_handle *handle, enum capi_eth_link_state *state)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->get_link_state != NULL) {
		return handle->ops->get_link_state(handle, state);
	}

	return -EINVAL;
}

int capi_eth_phy_get_link_info(struct capi_eth_phy_handle *handle, struct capi_eth_link_info *info)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->get_link_info != NULL) {
		return handle->ops->get_link_info(handle, info);
	}

	return -EINVAL;
}

int capi_eth_phy_read_c22(struct capi_eth_phy_handle *handle, uint8_t reg, uint16_t *data)
{
	if (handle == NULL || handle->mdio == NULL) {
		return -EINVAL;
	}

	return capi_mdio_read(handle->mdio, handle->phy_addr, reg, data);
}

int capi_eth_phy_write_c22(struct capi_eth_phy_handle *handle, uint8_t reg, uint16_t data)
{
	if (handle == NULL || handle->mdio == NULL) {
		return -EINVAL;
	}

	return capi_mdio_write(handle->mdio, handle->phy_addr, reg, data);
}

int capi_eth_phy_read_c45(struct capi_eth_phy_handle *handle, uint8_t devnum, uint16_t reg,
			  uint16_t *data)
{
	if (handle == NULL || handle->mdio == NULL) {
		return -EINVAL;
	}

	return capi_mdio_read_c45(handle->mdio, handle->phy_addr, devnum, reg, data);
}

int capi_eth_phy_write_c45(struct capi_eth_phy_handle *handle, uint8_t devnum, uint16_t reg,
			   uint16_t data)
{
	if (handle == NULL || handle->mdio == NULL) {
		return -EINVAL;
	}

	return capi_mdio_write_c45(handle->mdio, handle->phy_addr, devnum, reg, data);
}

int capi_eth_phy_plca_get_cfg(struct capi_eth_phy_handle *handle, struct capi_eth_phy_plca_cfg *cfg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->plca_get_cfg != NULL) {
		return handle->ops->plca_get_cfg(handle, cfg);
	}

	return -EINVAL;
}

int capi_eth_phy_plca_set_cfg(struct capi_eth_phy_handle *handle,
			      const struct capi_eth_phy_plca_cfg *cfg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->plca_set_cfg != NULL) {
		return handle->ops->plca_set_cfg(handle, cfg);
	}

	return -EINVAL;
}

int capi_eth_phy_plca_get_status(struct capi_eth_phy_handle *handle,
				 struct capi_eth_phy_plca_status *status)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->plca_get_status != NULL) {
		return handle->ops->plca_get_status(handle, status);
	}

	return -EINVAL;
}

int capi_eth_phy_poll(struct capi_eth_phy_handle *handle)
{
	if (handle == NULL) {
		return -EINVAL;
	}

	if (handle->ops == NULL || handle->ops->poll == NULL) {
		return 0;
	}

	return handle->ops->poll(handle);
}

int capi_eth_phy_register_callback(struct capi_eth_phy_handle *handle,
				   capi_eth_phy_event_callback const callback,
				   void *const callback_arg)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->register_callback != NULL) {
		return handle->ops->register_callback(handle, callback, callback_arg);
	}

	return -EINVAL;
}

void capi_eth_phy_isr(void *handle)
{
	struct capi_eth_phy_handle *phy_handle = (struct capi_eth_phy_handle *)handle;

	if (phy_handle != NULL && phy_handle->ops != NULL && phy_handle->ops->isr != NULL) {
		phy_handle->ops->isr(handle);
	}
}
