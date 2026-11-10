/*
 * Copyright (c) 2025 Analog Devices, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @addtogroup drivers
 * @{
 * @addtogroup pmic
 * @{
 */

/**
 * @file capi_regulator.h
 * @brief Common HAL Regulator. This is the generic portion of a regulator which
 *        will be covered by all regulator drivers. Other specific features will
 *        extend the interface based on the HW design.
 */

#ifndef _CAPI_REGULATOR_H_
#define _CAPI_REGULATOR_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * @enum capi_regulator_mode
 * @brief Enumerates the possible modes of a DCDC regulator.
 */
enum capi_regulator_mode {
	/**< PFM */
	CAPI_REGULATOR_MODE_PFM,
	/**< FPWM */
	CAPI_REGULATOR_MODE_FPWM,
	/**< IDLE */
	CAPI_REGULATOR_MODE_IDLE,
	/**< High Impedance */
	CAPI_REGULATOR_MODE_HIGH_IMPEDANCE,
	/**< Shutdown */
	CAPI_REGULATOR_MODE_SHUTDOWN,
	/**< Number of modes */
	CAPI_REGULATOR_NUM_MODES
};

/**
 * @struct capi_regulator_config
 * @brief Structure that holds the regulator configuration.
 */
struct capi_regulator_config {
	/**< Regulator ID. */
	uint32_t identifier;
	/**< Periphereal base address */
	uint32_t base_addr;
	/**< Platform specific regulator ops structure. */
	const struct capi_regulator_ops *ops;
	/**< Regulator specific configuration */
	void *extra;
};

/**
 * @struct capi_regulator_handle
 * @brief Structure for the regulator context.
 *
 */
struct capi_regulator_handle {
	/**< Regulator ID. */
	uint32_t identifier;
	/**< Periphereal base address */
	uint32_t base_addr;
	/**< If true, the driver is owner of handle memory. */
	bool init_allocated;
	/**< Lock */
	void *lock;
	/**< Platform specific regulator platform ops structure. */
	struct capi_regulator_ops *ops;
	/**< This is intended to store regulator specific configurations. */
	void *extra;
};

/**
 * @struct capi_regulator_ops
 * @brief Structure holding regulator specific operations.
 */
struct capi_regulator_ops {
	int (*init)(struct capi_regulator_handle **handle,
		    const struct capi_regulator_config *config);
	int (*deinit)(struct capi_regulator_handle *handle);
	int (*enable)(const struct capi_regulator_handle *handle);
	int (*disable)(const struct capi_regulator_handle *handle);
	int (*get_status)(const struct capi_regulator_handle *handle, void *status);
	int (*set_voltage)(const struct capi_regulator_handle *handle, uint32_t voltage_microvolt);
	int (*get_voltage)(const struct capi_regulator_handle *handle, uint32_t *voltage_microvolt);
	int (*set_mode)(const struct capi_regulator_handle *handle, enum capi_regulator_mode mode);
	int (*get_mode)(const struct capi_regulator_handle *handle, enum capi_regulator_mode *mode);
	int (*set_current_limit)(const struct capi_regulator_handle *handle,
				 uint32_t current_limit_microamp);
	int (*get_current_limit)(const struct capi_regulator_handle *handle,
				 uint32_t *current_limit_microamp);
	int (*set_active_discharge)(const struct capi_regulator_handle *handle, bool enabled);
	int (*get_active_discharge)(const struct capi_regulator_handle *handle, bool *enabled);
};

/**
 * @brief Initialize an instance of the regulator.
 *
 * @param [in out] handle Pointer to the context structure pointer.
 * @param [in] config Pointer to the configuration for the regulator.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_init(struct capi_regulator_handle **handle,
			const struct capi_regulator_config *config);

/**
 * @brief Deinitialize the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_deinit(struct capi_regulator_handle *handle);

/**
 * @brief Enable the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_enable(const struct capi_regulator_handle *handle);

/**
 * @brief Disable the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_disable(const struct capi_regulator_handle *handle);

/**
 * @brief Get the status of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [out] status Pointer to the status variable to be filled.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_get_status(const struct capi_regulator_handle *handle, void *status);

/**
 * @brief Set the output voltage of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [in] voltage_microvolt Voltage in microvolts.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_set_voltage(const struct capi_regulator_handle *handle,
			       uint32_t voltage_microvolt);

/**
 * @brief Get the output voltage of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [out] voltage_microvolt Pointer to the voltage variable to be filled.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_get_voltage(const struct capi_regulator_handle *handle,
			       uint32_t *voltage_microvolt);

/**
 * @brief Set the operating mode of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [in] mode Mode to be set.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_set_mode(const struct capi_regulator_handle *handle,
			    enum capi_regulator_mode mode);

/**
 * @brief Get the operating mode of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [out] mode Pointer to the mode variable to be filled.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_get_mode(const struct capi_regulator_handle *handle,
			    enum capi_regulator_mode *mode);

/**
 * @brief Set the current limit of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [in] current_limit_microamp Current limit in microamps.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_set_current_limit(const struct capi_regulator_handle *handle,
				     uint32_t current_limit_microamp);

/**
 * @brief Get the current limit of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [out] current_limit_microamp Pointer to the current limit variable to be filled.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_get_current_limit(const struct capi_regulator_handle *handle,
				     uint32_t *current_limit_microamp);

/**
 * @brief Set the active discharge mode of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [in] mode Active discharge mode to be set.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_set_active_discharge(const struct capi_regulator_handle *handle, bool enabled);

/**
 * @brief Get the active discharge mode of the regulator.
 *
 * @param [in] handle Pointer to the regulator context.
 * @param [out] mode Pointer to the active discharge mode variable to be filled.
 *
 * @return int 0 for success or error code.
 */
int capi_regulator_get_active_discharge(const struct capi_regulator_handle *handle, bool *enabled);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _CAPI_REGULATOR_H_ */

/**
 * @}
 * @}
 */
