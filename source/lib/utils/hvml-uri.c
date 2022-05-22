/*
 * hvml-uri - Utilities for HVML URI.
 *
 * Copyright (C) 2022 FMSoft <https://www.fmsoft.cn>
 *
 * Author: Vincent Wei <https://github.com/VincentWei>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include "hvml-uri.h"

#define HVML_SCHEMA     "hvml://"
#define COMP_SEPERATOR  '/'

static unsigned int get_comp_len(const char *str)
{
    unsigned int len = 0;

    while (*str && *str != COMP_SEPERATOR) {
        len++;
        str++;
    }

    return len;
}

bool hvml_uri_split(const char *uri,
        char **host, char **app, char **runner, char **group, char **page)
{
    const static unsigned int sz_schema = sizeof(HVML_SCHEMA) - 1;
    char *my_host = NULL, *my_app = NULL, *my_runner = NULL;
    char *my_group = NULL, *my_page = NULL;
    unsigned int len;

    if (strncasecmp(uri, HVML_SCHEMA, sz_schema))
        return false;

    uri += sz_schema;
    len = get_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        return false;
    my_host = strndup(uri, len);

    uri += len + 1;
    len = get_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        goto failed;
    my_app = strndup(uri, len);

    uri += len + 1;
    len = get_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        goto failed;
    my_runner = strndup(uri, len);

    uri += len + 1;
    len = get_comp_len(uri);
    if (len == 0)
        goto failed;

    if (uri[len] == COMP_SEPERATOR) {
        /* have group */
        my_group = strndup(uri, len);

        uri += len + 1;
        len = get_comp_len(uri);
        if (len == 0 || uri[len] != '\0')
            goto failed;
        my_page = strndup(uri, len);
    }
    else {
        /* no group */
        my_page = strndup(uri, len);
    }

    if (host)
        *host = my_host;
    else
        free(my_host);

    if (app)
        *app = my_app;
    else
        free(my_app);

    if (runner)
        *runner = my_runner;
    else
        free(my_runner);

    if (group)
        *group = my_group;
    else
        free(my_group);

    if (page)
        *page = my_page;
    else
        free(my_page);

    return true;

failed:
    if (my_host)
        free(my_host);
    if (my_app)
        free(my_app);
    if (my_runner)
        free(my_runner);
    if (my_group)
        free(my_group);
    if (my_page)
        free(my_page);

    return false;
}

