/*
 * Copyright (c) 2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_mdio.c
 * @brief This file contains a set of small wrapper functions that can be used to access
 *        the MDIO driver via its ops pointer structure. Note, it does not implement any
 * 	  thread safety such as mutually excluding calls to the MDIO functions.
 *        If this is needed, it is suggested that this file and all of the other driver
 *        wrappers be copied and enhanced in your project.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include "capi_mdio.h"

int capi_mdio_init(struct capi_mdio_handle **handle, const struct capi_mdio_init_config *config)
{
	if (config == NULL || config->ops == NULL || config->ops->init == NULL)
		return -EINVAL;

	return config->ops->init(handle, config);
}

int capi_mdio_deinit(struct capi_mdio_handle *handle)
{
	if (handle == NULL || handle->ops == NULL || handle->ops->deinit == NULL)
		return -EINVAL;

	return handle->ops->deinit(handle);
}

int capi_mdio_read(struct capi_mdio_handle *handle, uint8_t phy_addr, uint8_t reg, uint16_t *data)
{
	if (handle == NULL || handle->ops == NULL)
		return -EINVAL;

	if (handle->ops->read_c22 == NULL)
		return -ENOSYS;

	return handle->ops->read_c22(handle, phy_addr, reg, data);
}

int capi_mdio_write(struct capi_mdio_handle *handle, uint8_t phy_addr, uint8_t reg, uint16_t data)
{
	if (handle == NULL || handle->ops == NULL)
		return -EINVAL;

	if (handle->ops->write_c22 == NULL)
		return -ENOSYS;

	return handle->ops->write_c22(handle, phy_addr, reg, data);
}

int capi_mdio_read_c45(struct capi_mdio_handle *handle, uint8_t phy_addr, uint8_t devnum,
		       uint16_t reg, uint16_t *data)
{
	if (handle == NULL || handle->ops == NULL)
		return -EINVAL;

	if (handle->ops->read_c45 == NULL)
		return -ENOSYS;

	return handle->ops->read_c45(handle, phy_addr, devnum, reg, data);
}

int capi_mdio_write_c45(struct capi_mdio_handle *handle, uint8_t phy_addr, uint8_t devnum,
			uint16_t reg, uint16_t data)
{
	if (handle == NULL || handle->ops == NULL)
		return -EINVAL;

	if (handle->ops->write_c45 == NULL)
		return -ENOSYS;

	return handle->ops->write_c45(handle, phy_addr, devnum, reg, data);
}

int capi_mdio_scan(struct capi_mdio_handle *handle, uint8_t *phy_addr)
{
	uint16_t id1, id2;
	uint8_t i;
	int ret;

	if (handle == NULL || handle->ops == NULL || phy_addr == NULL)
		return -EINVAL;

	if (handle->ops->read_c22 == NULL)
		return -ENOSYS;

	for (i = 1; i <= 31; i++) {
		ret = handle->ops->read_c22(handle, i, CAPI_MDIO_REG_PHYSID1, &id1);
		if (ret)
			return ret;

		ret = handle->ops->read_c22(handle, i, CAPI_MDIO_REG_PHYSID2, &id2);
		if (ret)
			return ret;

		if (id1 > 0 && id1 < 0xFFFF && id2 > 0 && id2 < 0xFFFF) {
			*phy_addr = i;

			return 0;
		}
	}

	ret = handle->ops->read_c22(handle, 0, CAPI_MDIO_REG_PHYSID1, &id1);
	if (ret)
		return ret;

	ret = handle->ops->read_c22(handle, 0, CAPI_MDIO_REG_PHYSID2, &id2);
	if (ret)
		return ret;

	if (id1 > 0 && id1 < 0xFFFF && id2 > 0 && id2 < 0xFFFF) {
		*phy_addr = 0;

		return 0;
	}

	return -ENODEV;
}
