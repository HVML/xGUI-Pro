/*
 * hvml-uri - Utilities for HVML URI (copied from PurC).
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

#define LEN_HOST_NAME             127
#define LEN_APP_NAME              127
#define LEN_RUNNER_NAME           63
#define LEN_IDENTIFIER            63
#define LEN_ENDPOINT_NAME         \
    (LEN_HOST_NAME + LEN_APP_NAME + LEN_RUNNER_NAME + 3)
#define LEN_UNIQUE_ID             63

#define HVML_SCHEMA         "hvml://"
#define COMP_SEPERATOR      '/'
#define QUERY_SEPERATOR     '?'
#define FRAG_SEPERATOR      '#'
#define PAIR_SEPERATOR      '&'
#define KV_SEPERATOR        '='

size_t hvml_uri_assemble(char *uri, const char *host, const char* app,
        const char* runner, const char *group, const char *page)
{
    char *start = uri;

    uri = stpcpy(uri, "hvml://");
    uri = stpcpy(uri, host);
    uri[0] = '/';
    uri++;

    uri = stpcpy(uri, app);
    uri[0] = '/';
    uri++;

    uri = stpcpy(uri, runner);
    uri[0] = '/';
    uri++;

    if (group) {
        uri = stpcpy(uri, group);
        uri[0] = '/';
        uri++;
    }

    if (page) {
        uri = stpcpy(uri, page);
    }

    uri[0] = '\0';
    return uri - start;
}

char* hvml_uri_assemble_alloc(const char* host, const char* app,
        const char* runner, const char *group, const char *page)
{
    char* uri;
    static const int schema_len = sizeof(HVML_SCHEMA) - 1;
    int host_len, app_len, runner_len, group_len = 0, page_len = 0;

    if ((host_len = strlen (host)) > LEN_HOST_NAME)
        return NULL;

    if ((app_len = strlen (app)) > LEN_APP_NAME)
        return NULL;

    if ((runner_len = strlen (runner)) > LEN_RUNNER_NAME)
        return NULL;

    if (group) {
        group_len = strlen(group);
    }
    if (page)
        page_len = strlen(page);

    if ((uri = malloc(schema_len + host_len + app_len + runner_len +
                    group_len + page_len + 8)) == NULL)
        return NULL;


    hvml_uri_assemble(uri, host, app, runner, group, page);
    return uri;
}

static unsigned int get_path_comp_len(const char *str)
{
    unsigned int len = 0;

    while (*str && *str != COMP_SEPERATOR) {
        len++;
        str++;
    }

    return len;
}

static unsigned int get_path_trail_len(const char *str)
{
    unsigned int len = 0;

    while (*str && *str != COMP_SEPERATOR && *str != QUERY_SEPERATOR) {
        len++;
        str++;
    }

    return len;
}

bool hvml_uri_split(const char *uri,
        char *host, char *app, char *runner, char *group, char *page)
{
    static const unsigned int sz_schema = sizeof(HVML_SCHEMA) - 1;
    unsigned int len;

    if (strncasecmp(uri, HVML_SCHEMA, sz_schema))
        return false;

    uri += sz_schema;
    len = get_path_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        return false;
    if (host) {
        strncpy(host, uri, len);
        host[len] = '\0';
    }

    uri += len + 1;
    len = get_path_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        goto failed;
    if (app) {
        strncpy(app, uri, len);
        app[len] = '\0';
    }

    uri += len + 1;
    len = get_path_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        goto failed;
    if (runner) {
        strncpy(runner, uri, len);
        runner[len] = '\0';
    }

    if (group) group[0] = 0;
    if (page) page[0] = 0;

    do {
        uri += len + 1;
        len = get_path_comp_len(uri);
        if (len == 0)
            break;

        if (uri[len] == COMP_SEPERATOR) {
            /* have group */
            if (group) {
                strncpy(group, uri, len);
                group[len] = '\0';
            }

            uri += len + 1;
            len = get_path_trail_len(uri);
            if (len == 0 || (uri[len] != 0 && uri[len] != QUERY_SEPERATOR))
                goto failed;
            if (page) {
                strncpy(page, uri, len);
                page[len] = '\0';
            }
        }
        else {
            /* no group */
            if (page) {
                len = get_path_trail_len(uri);
                if (len == 0 || (uri[len] != 0 && uri[len] != QUERY_SEPERATOR))
                    goto failed;
                strncpy(page, uri, len);
                page[len] = '\0';
            }
        }
    } while (0);

    return true;

failed:
    return false;
}


bool hvml_uri_split_alloc(const char *uri,
        char **host, char **app, char **runner, char **group, char **page)
{
    static const unsigned int sz_schema = sizeof(HVML_SCHEMA) - 1;
    char *my_host = NULL, *my_app = NULL, *my_runner = NULL;
    char *my_group = NULL, *my_page = NULL;
    unsigned int len;

    if (strncasecmp(uri, HVML_SCHEMA, sz_schema))
        return false;

    uri += sz_schema;
    len = get_path_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        return false;
    my_host = strndup(uri, len);

    uri += len + 1;
    len = get_path_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        goto failed;
    my_app = strndup(uri, len);

    uri += len + 1;
    len = get_path_comp_len(uri);
    if (len == 0 || uri[len] != COMP_SEPERATOR)
        goto failed;
    my_runner = strndup(uri, len);

    do {
        uri += len + 1;
        len = get_path_comp_len(uri);
        if (len == 0)
            break;

        if (uri[len] == COMP_SEPERATOR) {
            /* have group */
            my_group = strndup(uri, len);

            uri += len + 1;
            len = get_path_trail_len(uri);
            if (len == 0 || (uri[len] != 0 && uri[len] != QUERY_SEPERATOR))
                goto failed;
            my_page = strndup(uri, len);
        }
        else {
            /* no group */
            len = get_path_trail_len(uri);
            if (len == 0 || (uri[len] != 0 && uri[len] != QUERY_SEPERATOR))
                goto failed;
            my_page = strndup(uri, len);
        }
    } while (0);

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
    else if (my_group)
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

static size_t get_key_len(const char *str)
{
    size_t len = 0;

    while (*str && *str != KV_SEPERATOR && *str != FRAG_SEPERATOR) {
        len++;
        str++;
    }

    return len;
}

static size_t get_value_len(const char *str)
{
    size_t len = 0;

    while (*str && *str != PAIR_SEPERATOR && *str != FRAG_SEPERATOR) {
        len++;
        str++;
    }

    return len;
}

static const char *locate_query_value(const char *uri, const char *key)
{
    size_t key_len = strlen(key);
    if (key_len == 0)
        return NULL;

    while (*uri && *uri != QUERY_SEPERATOR) {
        uri++;
    }

    if (uri[0] == 0)
        return NULL;

    char my_key[key_len + 2];
    strcpy(my_key, key);
    my_key[key_len] = KV_SEPERATOR;
    key_len++;
    my_key[key_len] = 0;

    const char *left = uri + 1;
    while (*left) {
        if (strncasecmp(left, my_key, key_len) == 0) {
            return left + key_len;
        }
        else {
            const char *value = left + get_key_len(left);
            unsigned int value_len = get_value_len(value);
            left = value + value_len;
            if (*left == PAIR_SEPERATOR)
                left++;

            if (*left == FRAG_SEPERATOR)
                break;
        }
    }

    return NULL;
}

bool hvml_uri_get_query_value(const char *uri, const char *key,
        char *value_buff)
{
    const char *value = locate_query_value(uri, key);

    if (value == NULL) {
        return false;
    }

    size_t value_len = get_value_len(value);
    if (value_len == 0) {
        return false;
    }

    strncpy(value_buff, value, value_len);
    value_buff[value_len] = 0;
    return true;
}

bool hvml_uri_get_query_value_alloc(const char *uri, const char *key,
        char **value_buff)
{
    const char *value = locate_query_value(uri, key);

    if (value == NULL) {
        return false;
    }

    size_t value_len = get_value_len(value);
    if (value_len == 0) {
        return false;
    }

    *value_buff = strndup(value, value_len);
    return true;
}

