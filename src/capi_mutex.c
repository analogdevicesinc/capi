/*
 * Copyright (c) 2025-2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file capi_mutex.c
 * @brief This file contains a set of small wrapper functions that can be used to access
 *        the MUTEX driver via its ops pointer structure. Note, it does not
 *        implement any thread safety such as mutually excluding calls to the MUTEX functions.
 *        If this is needed, it is suggested that this file and all of the other driver
 *        wrappers be copied and enhanced in your project.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <capi_mutex.h>

int capi_mutex_init(struct capi_mutex **handle, const struct capi_mutex_config *config)
{
	if (handle != NULL && config != NULL && config->ops != NULL && config->ops->init != NULL) {
		return config->ops->init(handle, config);
	}

	return -EINVAL;
}

int capi_mutex_deinit(struct capi_mutex *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->deinit != NULL) {
		return handle->ops->deinit(handle);
	}

	return -EINVAL;
}

int capi_mutex_lock(struct capi_mutex *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->lock != NULL) {
		return handle->ops->lock(handle);
	}

	return -EINVAL;
}

int capi_mutex_trylock(struct capi_mutex *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->trylock != NULL) {
		return handle->ops->trylock(handle);
	}

	return -EINVAL;
}

int capi_mutex_unlock(struct capi_mutex *handle)
{
	if (handle != NULL && handle->ops != NULL && handle->ops->unlock != NULL) {
		return handle->ops->unlock(handle);
	}

	return -EINVAL;
}
