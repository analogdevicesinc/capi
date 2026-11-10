/*
 * Copyright (c) 2026 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file
 * @brief Common HAL Semaphore API
 *
 * Unified interface for semaphore instances shared between multiple
 * initiators. Organized around three concepts:
 *
 * 1. Type — BINARY, COUNTING, or OWNED, chosen at init.
 * 2. Lifecycle — init/attach create or bind a handle; detach/deinit
 *    tear one down without or with destroying the shared instance.
 * 3. Operations — acquire/release/get/set, with per-type semantics.
 *
 * Drivers may leave unsupported ops NULL; the thin layer returns
 * -ENOTSUP.
 */

#ifndef _CAPI_SEM_H_
#define _CAPI_SEM_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/** @brief Semaphore behavior type. Frozen at capi_sem_init(). */
enum capi_sem_type {
	CAPI_SEM_TYPE_BINARY,   /**< Mutual exclusion; no ownership tracking. */
	CAPI_SEM_TYPE_COUNTING, /**< Counter; decrement on acquire, increment on release. */
	CAPI_SEM_TYPE_OWNED,    /**< Mutual exclusion with caller-supplied owner tag. */
};

/**
 * @brief Semaphore configuration.
 *
 * Passed to both capi_sem_init() and capi_sem_attach(); some fields
 * have call-specific interpretation.
 */
struct capi_sem_config {
	/** Driver-defined instance identifier (typically MMIO address or
	 *  pool index). Must match between init and later attach calls. */
	uint64_t identifier;

	/** Semaphore type. See ::capi_sem_type. */
	enum capi_sem_type type;

	/** Type-dependent value:
	 *  - BINARY  : ignored.
	 *  - COUNTING: initial counter at init(); ignored at attach().
	 *  - OWNED   : this handle's owner tag (must be non-zero); stored in
	 *              the handle only, hardware not written at init/attach. */
	uint32_t initial_value;

	/** Optional driver/platform-specific configuration. */
	void *extra;

	/** Platform-specific ops table. May be NULL; auto-populated by the
	 *  build system. */
	const struct capi_sem_ops *ops;
};

/**
 * @brief Semaphore instance handle.
 *
 * Drivers may embed this as the first field of their private handle
 * struct, or attach their state through the priv pointer.
 */
struct capi_sem_handle {
	/** Dispatch table copied from config->ops at init/attach. */
	const struct capi_sem_ops *ops;

	/** True if the driver allocated this handle (caller passed
	 *  *handle == NULL to init/attach); deinit will free it. */
	bool init_allocated;

	/** Semaphore type; frozen at init/attach. */
	enum capi_sem_type type;

	/** For OWNED: this handle's owner tag, copied from
	 *  config->initial_value at init/attach. Ignored otherwise. */
	uint32_t owner_tag;

	/** Optional serialization lock used by the CAPI thin layer;
	 *  NULL if the driver's primitive is already race-safe. */
	void *lock;

	/** Driver-private state; opaque to the CAPI layer. Typically
	 *  holds the decoded instance target (e.g. MMIO pointer) so that
	 *  runtime calls can act on the right instance without re-parsing
	 *  the config. */
	void *priv;
};

/**
 * @brief Create a new semaphore instance and initialize its storage.
 *
 * Driver applies @p config->initial_value per type. Semaphore is free
 * on successful return.
 *
 * @param[in,out] handle  If *handle is NULL, driver allocates; otherwise
 *                        the application owns the memory.
 * @param[in]     config  Configuration for the new instance.
 *
 * @return 0 or negative errno.
 */
int capi_sem_init(struct capi_sem_handle **handle, const struct capi_sem_config *config);

/**
 * @brief Attach to an existing instance without touching its live state.
 *
 * For OWNED, @p config->initial_value is this handle's own owner tag
 * (not the creator's).
 *
 * @return 0 or negative errno.
 */
int capi_sem_attach(struct capi_sem_handle **handle, const struct capi_sem_config *config);

/**
 * @brief Release a handle without destroying the shared instance.
 *
 * @return 0 or negative errno.
 */
int capi_sem_detach(struct capi_sem_handle *handle);

/**
 * @brief Destroy the instance previously created via capi_sem_init() and
 *        return its storage to the reset state.
 *
 * Caller must ensure no other initiator is still using the instance.
 *
 * @return 0 or negative errno.
 */
int capi_sem_deinit(struct capi_sem_handle *handle);

/**
 * @brief Non-blocking acquire.
 *
 * Per type:
 *  - BINARY, OWNED : succeeds if the instance is free.
 *  - COUNTING      : succeeds if the counter is greater than zero.
 *
 * @retval 0        Acquired.
 * @retval -EBUSY   Not available.
 * @retval -ENOTSUP Not supported by this driver.
 */
int capi_sem_try_acquire(struct capi_sem_handle *handle);

/**
 * @brief Blocking acquire. Blocking primitive is driver-defined.
 *
 * @retval 0        Acquired.
 * @retval -ENOTSUP Not supported by this driver.
 */
int capi_sem_acquire(struct capi_sem_handle *handle);

/**
 * @brief Blocking acquire with a timeout in microseconds.
 *
 * @retval 0          Acquired.
 * @retval -ETIMEDOUT Timeout expired.
 * @retval -ENOTSUP   Not supported by this driver.
 */
int capi_sem_acquire_timeout(struct capi_sem_handle *handle, uint32_t timeout_us);

/**
 * @brief Release.
 *
 * Per type:
 *  - BINARY  : unconditionally clears the instance.
 *  - COUNTING: increments the counter.
 *  - OWNED   : clears the instance only if the recorded owner tag
 *              matches this handle's tag; otherwise -EPERM.
 *
 * @retval 0        Released.
 * @retval -EPERM   Caller is not the owner (OWNED only).
 * @retval -ENOTSUP Not supported by this driver.
 */
int capi_sem_release(struct capi_sem_handle *handle);

/**
 * @brief Read the current storage value.
 *
 * Per type:
 *  - BINARY  : 0 = free, non-zero = held.
 *  - COUNTING: number of available resources; 0 means next acquire blocks.
 *  - OWNED   : raw content; 0 = free, non-zero = current owner tag.
 *
 * @retval 0        Success.
 * @retval -EINVAL  @p value is NULL.
 * @retval -ENOTSUP Not supported by this driver.
 */
int capi_sem_get_value(struct capi_sem_handle *handle, uint32_t *value);

/**
 * @brief Write a value directly to the storage.
 *
 * Administrative primitive (recovery, self-test). Unsafe under
 * concurrent access. For OWNED, intentionally bypasses the owner-tag
 * check so a non-owning initiator can recover a stuck lock; caller
 * coordinates recovery out-of-band.
 *
 * @retval 0        Success.
 * @retval -ENOTSUP Not supported by this driver.
 */
int capi_sem_set_value(struct capi_sem_handle *handle, uint32_t value);

/**
 * @brief Driver ops table. NULL slots return -ENOTSUP.
 */
struct capi_sem_ops {
	int (*init)(struct capi_sem_handle **handle, const struct capi_sem_config *config);
	int (*attach)(struct capi_sem_handle **handle, const struct capi_sem_config *config);
	int (*detach)(struct capi_sem_handle *handle);
	int (*deinit)(struct capi_sem_handle *handle);

	int (*try_acquire)(struct capi_sem_handle *handle);
	int (*acquire)(struct capi_sem_handle *handle);
	int (*acquire_timeout)(struct capi_sem_handle *handle, uint32_t timeout_us);
	int (*release)(struct capi_sem_handle *handle);

	int (*get_value)(struct capi_sem_handle *handle, uint32_t *value);
	int (*set_value)(struct capi_sem_handle *handle, uint32_t value);
};

#if defined(__cplusplus)
}
#endif

#endif /* _CAPI_SEM_H_ */
