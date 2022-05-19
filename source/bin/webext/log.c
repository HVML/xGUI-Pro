/*
 * log.c - The implementation of log facility.
 *
 * Copyright (C) 2022 FMSoft <https://www.fmsoft.cn>
 *
 * Authors:
 *  Vincent Wei (https://github.com/VincentWei), 2022
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

#include "config.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

#if HAVE(SYSLOG_H)
#include <syslog.h>
#else

enum {
    LOG_EMERG,      // system is unusable
    LOG_ALERT,      // action must be taken immediately
    LOG_CRIT,       // critical conditions
    LOG_ERR,        // error conditions
    LOG_WARNING,    // warning conditions
    LOG_NOTICE,     // normal, but significant, condition
    LOG_INFO,       // informational message
    LOG_DEBUG,      // debug-level message
};

static inline void openlog(const char *ident, int option, int facility)
{
    (void)ident;
    (void)option;
    (void)facility;
    // do nothing;
}

static inline void vsyslog(int priority, const char *format, va_list ap)
{
    (void)priority;
    vfprintf(stderr, format, ap);
}
#endif

static FILE *fp_log;

#define LOG_FILE_SYSLOG (FILE *)-1

bool my_log_enable(bool enable, const char* syslog_ident)
{
    if (enable) {
        if (syslog_ident) {
            openlog(syslog_ident, LOG_PID, LOG_USER);
            fp_log = LOG_FILE_SYSLOG;
        }
        else {
            fp_log = stderr;
        }
    }
    else {
        fp_log = NULL;
    }

    return true;
}

void my_log_with_tag(const char *tag, const char *msg, va_list ap)
{
    if (fp_log == LOG_FILE_SYSLOG) {
        int priority = LOG_DEBUG;
        if (strcasecmp(tag, "INFO") == 0)
            priority = LOG_INFO;
        else if (strcasecmp(tag, "WARN") == 0)
            priority = LOG_WARNING;
        else if (strcasecmp(tag, "ERROR") == 0)
            priority = LOG_ERR;

        vsyslog(priority, msg, ap);
    }
    else if (fp_log == stderr) {
        fprintf(fp_log, "%s >> ", tag);
        vfprintf(fp_log, msg, ap);
    }
}

