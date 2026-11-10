/*
 * Copyright (c) 2025 Analog Devices, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CAPI_LOG_ENABLE
#define CAPI_LOG_ENABLE 0 /* strip by default */
#endif

#if defined(__GNUC__) || defined(__clang__)
#define CAPI_PRINTF(a, b) __attribute__((format(printf, a, b)))
#else
#define CAPI_PRINTF(a, b)
#endif

/* Allow integrators to customize the file token if they want a basename. */
#ifndef CAPI_LOG_FILE
#define CAPI_LOG_FILE __FILE__
#endif

#if CAPI_LOG_ENABLE
/* Public entry the drivers call via macro */
void capi_log_prefix(const char *file, int line, const char *func, const char *fmt, ...)
	CAPI_PRINTF(4, 5);

/* Weak hook symbol to be overridden by the platform/app (don’t call directly). */
void capi_vlog_prefix_impl(const char *file, int line, const char *func, const char *fmt,
			   va_list ap);

/* Macro captures the call site */
#define CAPI_LOG(...) capi_log_prefix(CAPI_LOG_FILE, __LINE__, __func__, __VA_ARGS__)

#else
#define CAPI_LOG(...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif
