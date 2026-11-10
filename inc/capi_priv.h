/*
 * Copyright (c) 2024-2025 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file
 * @brief CAPI common definitions
 */

#ifndef _CAPI_PRIV_H
#define _CAPI_PRIV_H

#if defined(__cplusplus)
extern "C" {
#endif /* _cplusplus */

struct capi_device {
	const void *ops;     /**< set and used by capi thin layer */
	bool init_allocated; /**< If true, the driver is owner of handle memory. */
};

#if defined(__cplusplus)
}
#endif

#endif /* _CAPI_PRIV_H */