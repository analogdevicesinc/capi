/*
 * Copyright (c) 2025 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _CAPI_GPIO_H_
#define _CAPI_GPIO_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stdint.h>

/**
 * GPIO direction options.
 */
#define CAPI_GPIO_OUTPUT 0x0
#define CAPI_GPIO_INPUT  0x1

/**
 * GPIO pin value options.
 */
#define CAPI_GPIO_LOW  0x0
#define CAPI_GPIO_HIGH 0x1

/**
 * GPIO pin flags options.
 */
#define CAPI_GPIO_ACTIVE_HIGH 0x00
#define CAPI_GPIO_ACTIVE_LOW  0x01

/**
 * GPIO pull resistor configuration options.
 */
enum capi_gpio_pull {
	CAPI_GPIO_PULL_NONE = 0, /**< No pull resistor (high impedance) */
	CAPI_GPIO_PULL_DOWN = 1, /**< Pull-down resistor enabled */
	CAPI_GPIO_PULL_UP = 2,   /**< Pull-up resistor enabled */
};

/**
 * GPIO pin electrical configuration.
 *
 * Used with capi_gpio_pin_set_config() / capi_gpio_pin_get_config()
 * to configure pin electrical characteristics. Callers should first
 * call get_config to obtain current settings, modify the desired
 * fields, then call set_config.
 */
struct capi_gpio_pin_config {
	uint8_t direction;        /**< Pin direction (CAPI_GPIO_INPUT or CAPI_GPIO_OUTPUT) */
	enum capi_gpio_pull pull; /**< Pull resistor configuration */
	uint8_t drive_strength;   /**< Drive strength (0 to max, hardware-specific range) */
	bool open_drain;          /**< Open-drain output mode enable */
	bool schmitt_trigger;     /**< Schmitt trigger input enable */
	bool slew_rate;           /**< Slew rate control enable */
	void *extra;              /**< Optional driver-specific extra configuration */
};

/**
 * GPIO pin configuration field selectors.
 *
 * Bit flags naming the members of ::capi_gpio_pin_config, for
 * capi_gpio_pin_set_config_mask(). There is no flag for @c extra.
 */
enum capi_gpio_config_field {
	CAPI_GPIO_CONFIG_DIRECTION = (1U << 0),       /**< direction */
	CAPI_GPIO_CONFIG_PULL = (1U << 1),            /**< pull */
	CAPI_GPIO_CONFIG_DRIVE_STRENGTH = (1U << 2),  /**< drive_strength */
	CAPI_GPIO_CONFIG_OPEN_DRAIN = (1U << 3),      /**< open_drain */
	CAPI_GPIO_CONFIG_SCHMITT_TRIGGER = (1U << 4), /**< schmitt_trigger */
	CAPI_GPIO_CONFIG_SLEW_RATE = (1U << 5),       /**< slew_rate */
};

/** Every flag of ::capi_gpio_config_field. */
#define CAPI_GPIO_CONFIG_ALL                                                                       \
	((uint32_t)CAPI_GPIO_CONFIG_DIRECTION | (uint32_t)CAPI_GPIO_CONFIG_PULL |                  \
	 (uint32_t)CAPI_GPIO_CONFIG_DRIVE_STRENGTH | (uint32_t)CAPI_GPIO_CONFIG_OPEN_DRAIN |       \
	 (uint32_t)CAPI_GPIO_CONFIG_SCHMITT_TRIGGER | (uint32_t)CAPI_GPIO_CONFIG_SLEW_RATE)

/**
 * GPIO port handle type
 */
struct capi_gpio_port_handle {
	const struct capi_gpio_ops *ops; /**< set and used by capi thin layer */
	bool init_allocated;             /**< If true, the driver is owner of handle memory. */
	void *lock;                      /**< set and used by capi thin layer if mux is available */
	void *priv;                      /**< set and used by user optionally */
};

/**
 * GPIO port configuration.
 */
struct capi_gpio_port_config {
	/** GPIO port specific operations. */
	const struct capi_gpio_ops *ops;
	/** GPIO port identifier:
	 * - Base address for internal GPIO controllers.
	 * - Device specific address for external GPIO controllers (e.g., GPIO
	 *   expanders).
	 */
	uint64_t identifier;
	/** Number of GPIO pins. */
	uint8_t num_pins;
	/** GPIO pins flags. */
	uint32_t *flags;
	/** This is intended to store GPIO specific configurations. */
	void *extra;
};

/**
 * GPIO pin configuration/descriptor.
 */
struct capi_gpio_pin {
	/** Corresponding GPIO port handle. */
	struct capi_gpio_port_handle *port_handle;
	/** GPIO pin number. */
	uint32_t number;
	/** GPIO pin flags. */
	uint32_t flags;
};

/**
 * @brief Initialize the GPIO port.
 *
 * @param [in,out] handle If NULL, the function must allocate the required
 *                        memory. Otherwise, init() will use the preallocated
 *                        structure pointed to by the handle.
 * @param [in] config The GPIO port configuration.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_init(struct capi_gpio_port_handle **handle,
			const struct capi_gpio_port_config *config);

/**
 * @brief Deinitialize the GPIO port.
 *
 * @param [in] handle The GPIO port handle. If allocated by the init()
 *                    function, deinit() must free the memory.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_deinit(struct capi_gpio_port_handle **handle);

/**
 * @brief Set the direction of all the pins belonging to the specified port.
 *
 * @param [in] handle The GPIO port.
 * @param [in] direction_bitmask The direction (bitmask) to be set for the
 *                               GPIO pins.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_set_direction(struct capi_gpio_port_handle *handle, uint64_t direction_bitmask);

/**
 * @brief Get the current direction of all the pins belonging to the
 *        specified port.
 *
 * @param [in] handle The GPIO port.
 * @param [out] direction_bitmask The current direction (bitmask) of the
 *                                GPIO pins.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_get_direction(struct capi_gpio_port_handle *handle, uint64_t *direction_bitmask);

/**
 * @brief Set the value of all the pins belonging to the specified port.
 *
 * @param [in] handle The GPIO port.
 * @param [in] value_bitmask The value (bitmask) to be set for the GPIO pins.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_set_value(struct capi_gpio_port_handle *handle, uint64_t value_bitmask);

/**
 * @brief Get the current value of all the pins belonging to the
 *        specified port.
 *
 * @param [in] handle The GPIO port.
 * @param [out] value_bitmask The current value (bitmask) of the GPIO pins.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_get_value(struct capi_gpio_port_handle *handle, uint64_t *value_bitmask);

/**
 * @brief Set the raw value of all the pins belonging to the specified port.
 *
 * @param [in] handle The GPIO port.
 * @param [in] value_bitmask The raw value (bitmask) to be set for the GPIO
 *                           pins, regardless of the ACTIVE_LOW flag.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_set_raw_value(struct capi_gpio_port_handle *handle, uint64_t value_bitmask);

/**
 * @brief Get the current raw value of all the pins belonging to the
 *        specified port.
 *
 * @param [in] handle The GPIO port.
 * @param [out] value_bitmask The current raw value (bitmask) of the GPIO pins,
 *                            regardless of the ACTIVE_LOW flag.
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_get_raw_value(struct capi_gpio_port_handle *handle, uint64_t *value_bitmask);

/**
 * @brief Toggle the output value of the specified pins in a port.
 *
 * @param [in] handle The GPIO port.
 * @param [in] pins_bitmask Bitmask of pins to toggle.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_toggle(struct capi_gpio_port_handle *handle, uint64_t pins_bitmask);

/**
 * @brief Set the direction of the specified GPIO pin.
 *
 * @param [in] pin The GPIO pin.
 * @param [in] direction The direction to be set for the GPIO pin.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_set_direction(struct capi_gpio_pin *pin, uint8_t direction);

/**
 * @brief Get the current direction of the specified GPIO pin.
 *
 * @param [in] pin The GPIO pin.
 * @param [out] direction The current direction of the GPIO pin.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_get_direction(struct capi_gpio_pin *pin, uint8_t *direction);

/**
 * @brief Set the value of the specified GPIO pin.
 *
 * @param [in] pin The GPIO pin.
 * @param [in] value The value to be set for the GPIO pin.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_set_value(struct capi_gpio_pin *pin, uint8_t value);

/**
 * @brief Get the current value of the specified GPIO pin.
 *
 * @param [in] pin The GPIO pin.
 * @param [out] value The current value of the GPIO pin.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_get_value(struct capi_gpio_pin *pin, uint8_t *value);

/**
 * @brief Set the raw value of the specified GPIO pin.
 *
 * @param [in] pin The GPIO pin.
 * @param [in] value The raw value to be set for the GPIO pin,
 *                   regardless of the ACTIVE_LOW flag.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_set_raw_value(struct capi_gpio_pin *pin, uint8_t value);

/**
 * @brief Get the current raw value of all the pins belonging to the
 *        specified port.
 *
 * @param [in] pin The GPIO pin.
 * @param [out] value The current raw value of the GPIO pin,
 *                    regardless of the ACTIVE_LOW flag.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_get_raw_value(struct capi_gpio_pin *pin, uint8_t *value);

/**
 * @brief Toggle the output value of the specified GPIO pin.
 *
 * @param [in] pin The GPIO pin.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_toggle(struct capi_gpio_pin *pin);

/**
 * @brief Set the electrical configuration of a GPIO pin.
 *
 * Configures direction, pull resistor, drive strength, open-drain mode,
 * Schmitt trigger, and slew rate in a single call. All fields in the
 * config structure are applied.
 *
 * @param [in] pin The GPIO pin descriptor.
 * @param [in] config Pointer to the pin configuration to apply.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_set_config(struct capi_gpio_pin *pin, const struct capi_gpio_pin_config *config);

/**
 * @brief Get the current electrical configuration of a GPIO pin.
 *
 * Reads the current pin configuration. A driver may take some fields from its own
 * state instead of reading them back from hardware, see the driver documentation.
 *
 * @param [in] pin The GPIO pin descriptor.
 * @param [out] config Pointer to store the current pin configuration.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_get_config(struct capi_gpio_pin *pin, struct capi_gpio_pin_config *config);

/**
 * @brief Set selected fields of the electrical configuration of a GPIO pin.
 *
 * Applies only the members of the config structure named in @p fields and
 * leaves the rest of the pin configuration as it is. Members that are not
 * selected are neither read nor validated. Selecting no field succeeds without
 * changing anything.
 *
 * @param [in] pin The GPIO pin descriptor.
 * @param [in] config Pointer to the pin configuration to apply fields from.
 * @param [in] fields OR of ::capi_gpio_config_field flags.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_pin_set_config_mask(struct capi_gpio_pin *pin,
				  const struct capi_gpio_pin_config *config, uint32_t fields);

/**
 * @brief Atomically set the output of the specified pins in a port to HIGH.
 *
 * Only affects output pins selected by pins_bitmask. Other pins are unchanged.
 * The pins are driven at the physical level. Active-low flags are not applied, as for
 * the raw value functions.
 *
 * @param [in] handle The GPIO port.
 * @param [in] pins_bitmask Bitmask of pins to set HIGH.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_set_output(struct capi_gpio_port_handle *handle, uint64_t pins_bitmask);

/**
 * @brief Atomically clear the output of the specified pins in a port to LOW.
 *
 * Only affects output pins selected by pins_bitmask. Other pins are unchanged.
 * The pins are driven at the physical level. Active-low flags are not applied, as for
 * the raw value functions.
 *
 * @param [in] handle The GPIO port.
 * @param [in] pins_bitmask Bitmask of pins to clear to LOW.
 *
 * @return 0 in case of success, negative error code otherwise.
 */
int capi_gpio_port_clear_output(struct capi_gpio_port_handle *handle, uint64_t pins_bitmask);

/**
 * Container for the GPIO specific operations.
 */
struct capi_gpio_ops {
	/** See capi_gpio_port_init() */
	int (*port_init)(struct capi_gpio_port_handle **handle,
			 const struct capi_gpio_port_config *config);
	/** See capi_gpio_port_deinit() */
	int (*port_deinit)(struct capi_gpio_port_handle **handle);
	/** See capi_gpio_port_set_direction() */
	int (*port_set_direction)(struct capi_gpio_port_handle *handle, uint64_t direction_bitmask);
	/** See capi_gpio_port_get_direction() */
	int (*port_get_direction)(struct capi_gpio_port_handle *handle,
				  uint64_t *direction_bitmask);
	/** See capi_gpio_port_set_value() */
	int (*port_set_value)(struct capi_gpio_port_handle *handle, uint64_t value_bitmask);
	/** See capi_gpio_port_get_value() */
	int (*port_get_value)(struct capi_gpio_port_handle *handle, uint64_t *value_bitmask);
	/** See capi_gpio_port_set_raw_value() */
	int (*port_set_raw_value)(struct capi_gpio_port_handle *handle, uint64_t value_bitmask);
	/** See capi_gpio_port_get_raw_value() */
	int (*port_get_raw_value)(struct capi_gpio_port_handle *handle, uint64_t *value_bitmask);
	/** See capi_gpio_port_toggle() */
	int (*port_toggle)(struct capi_gpio_port_handle *handle, uint64_t pins_bitmask);
	/** See capi_gpio_pin_set_direction() */
	int (*pin_set_direction)(struct capi_gpio_pin *pin, uint8_t direction);
	/** See capi_gpio_pin_get_direction() */
	int (*pin_get_direction)(struct capi_gpio_pin *pin, uint8_t *direction);
	/** See capi_gpio_pin_set_value() */
	int (*pin_set_value)(struct capi_gpio_pin *pin, uint8_t value);
	/** See capi_gpio_pin_get_value() */
	int (*pin_get_value)(struct capi_gpio_pin *pin, uint8_t *value);
	/** See capi_gpio_pin_set_raw_value() */
	int (*pin_set_raw_value)(struct capi_gpio_pin *pin, uint8_t value);
	/** See capi_gpio_pin_get_raw_value() */
	int (*pin_get_raw_value)(struct capi_gpio_pin *pin, uint8_t *value);
	/** See capi_gpio_pin_toggle() */
	int (*pin_toggle)(struct capi_gpio_pin *pin);
	/*
	 * Ops below were added after the first release. They stay at the end, so that the ops
	 * above keep their offsets.
	 */
	/** See capi_gpio_pin_set_config() */
	int (*pin_set_config)(struct capi_gpio_pin *pin, const struct capi_gpio_pin_config *config);
	/** See capi_gpio_pin_get_config() */
	int (*pin_get_config)(struct capi_gpio_pin *pin, struct capi_gpio_pin_config *config);
	/** See capi_gpio_pin_set_config_mask() */
	int (*pin_set_config_mask)(struct capi_gpio_pin *pin,
				   const struct capi_gpio_pin_config *config, uint32_t fields);
	/** See capi_gpio_port_set_output() */
	int (*port_set_output)(struct capi_gpio_port_handle *handle, uint64_t pins_bitmask);
	/** See capi_gpio_port_clear_output() */
	int (*port_clear_output)(struct capi_gpio_port_handle *handle, uint64_t pins_bitmask);
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CAPI_GPIO_H_ */
