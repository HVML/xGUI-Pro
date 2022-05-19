/*
 * @file log.h
 * @author Vincent Wei
 * @date 2022/05/19
 * @brief The internal interfaces for log.
 *
 * Copyright (C) 2022 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of xGUI Pro, an advanced HVML renderer.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdarg.h>
#include <stdbool.h>

#if __has_attribute(format)
#define MY_ATTRIBUTE_PRINTF(formatStringArgument, extraArguments) __attribute__((format(printf, formatStringArgument, extraArguments)))
#else
#define MY_ATTRIBUTE_PRINTF
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool my_log_enable(bool enable, const char* syslog_ident);
void my_log_with_tag(const char* tag, const char *msg, va_list ap)
    MY_ATTRIBUTE_PRINTF(2, 0);

#ifdef __cplusplus
}
#endif

MY_ATTRIBUTE_PRINTF(1, 2)
static inline void
my_log_info(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    my_log_with_tag("INFO", msg, ap);
    va_end(ap);
}

MY_ATTRIBUTE_PRINTF(1, 2)
static inline void
my_log_debug(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    my_log_with_tag("DEBUG", msg, ap);
    va_end(ap);
}

MY_ATTRIBUTE_PRINTF(1, 2)
static inline void
my_log_warn(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    my_log_with_tag("WARN", msg, ap);
    va_end(ap);
}

MY_ATTRIBUTE_PRINTF(1, 2)
static inline void
my_log_error(const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    my_log_with_tag("ERROR", msg, ap);
    va_end(ap);
}

#ifdef NDEBUG
#define LOG_DEBUG(x, ...)
#else
#define LOG_DEBUG(x, ...)   \
    my_log_debug("%s: " x, __func__, ##__VA_ARGS__)
#endif /* not defined NDEBUG */

#define LOG_ERROR(x, ...)   \
    my_log_error("%s: " x, __func__, ##__VA_ARGS__)

#define LOG_WARN(x, ...)    \
    my_log_warn("%s: " x, __func__, ##__VA_ARGS__)

#define LOG_INFO(x, ...)    \
    my_log_info("%s: " x, __func__, ##__VA_ARGS__)

