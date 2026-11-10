/*
 * Copyright (c) 2025 Analog Devices, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "capi_log.h"

#if CAPI_LOG_ENABLE

/* Default weak sink: drops everything; pulls in no stdio. */
__attribute__((weak)) void capi_vlog_prefix_impl(const char *file, int line, const char *func,
						 const char *fmt, va_list ap)
{
	(void)file;
	(void)line;
	(void)func;
	(void)fmt;
	(void)ap;
}

/* Strong wrapper that sets up varargs and calls the (possibly overridden) sink. */
void capi_log_prefix(const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	capi_vlog_prefix_impl(file, line, func, fmt, ap);
	va_end(ap);
}

#endif /* CAPI_LOG_ENABLE */
