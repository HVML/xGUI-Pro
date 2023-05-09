/*
 * load-asset - Load asset content.
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>

#include "hvml-uri.h"
#include "load-asset.h"

char *open_and_load_asset(const char *env, const char *prefix,
        const char *file, ssize_t *max_to_load, int *fd, size_t *length)
{
    char *contents = NULL;

    const char *webext_dir = env ? g_getenv(env) : NULL;
    if (webext_dir == NULL) {
        webext_dir = prefix;
    }

    gchar *path = g_strdup_printf("%s/%s", webext_dir ? webext_dir : ".", file);
    if (path && length) {

        *fd = open(path, O_RDONLY);
        if (*fd < 0)
            goto failed;

        struct stat st;
        if (fstat(*fd, &st)) {
            goto failed;
        }

        *length = st.st_size;

        size_t len;
        if (*max_to_load > 0) {
            if (*length < *max_to_load)
                len = *length;
            else
                len = *max_to_load;
        }
        else {
            len = *length;
        }

        contents = malloc(len + 1);
        if (contents == NULL)
            goto failed;

        *max_to_load = read(*fd, contents, len);
        if (*max_to_load < 0)
            goto failed;

        lseek(*fd, 0, SEEK_SET);
        contents[len] = '\0';
    }
    else {
        goto failed;
    }

    free(path);
    return contents;

failed:
    if (*fd >= 0)
        close(*fd);
    if (contents)
        free(contents);
    if (path)
        free(path);
    return NULL;
}

char *load_asset_content(const char *env, const char *prefix,
        const char *file, size_t *length, unsigned flags)
{
    char *buf = NULL;

    const char *webext_dir = env ? g_getenv(env) : NULL;
    if (webext_dir == NULL) {
        webext_dir = prefix;
    }

    gchar *path = g_strdup_printf("%s/%s", webext_dir ? webext_dir : ".", file);

    if (path) {
        FILE *f = fopen(path, "r");

        if (f) {
            if (fseek(f, 0, SEEK_END))
                goto failed;

            long len = ftell(f);
            if (len < 0)
                goto failed;

            buf = malloc(len + 1);
            if (buf == NULL)
                goto failed;

            fseek(f, 0, SEEK_SET);
            if (fread(buf, 1, len, f) < (size_t)len) {
                free(buf);
                buf = NULL;
            }
            buf[len] = '\0';

            if (length)
                *length = (size_t)len;
failed:
            fclose(f);

            if (flags & ASSET_FLAG_ONCE)
                remove(path);
        }
        else {
            goto done;
        }

    }

done:
    if (path)
        free(path);
    return buf;
}

