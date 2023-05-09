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

#ifndef __LIB_UTILS_LOAD_ASSET_H
#define __LIB_UTILS_LOAD_ASSET_H

#include <stddef.h>
#include <sys/types.h>

#define ASSET_FLAG_ONCE             0x01

#ifdef __cplusplus
extern "C" {
#endif

char *open_and_load_asset(const char *env, const char *prefix,
        const char *file, ssize_t *max_to_load, int *fd, size_t *length);

char *load_asset_content(const char *env, const char *prefix,
        const char *file, size_t *length, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif  /* __LIB_UTILS_LOAD_ASSET_H */
