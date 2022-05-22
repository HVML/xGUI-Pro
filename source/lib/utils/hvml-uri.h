/*
 * hvml-uri - utilities for HVML URI.
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

#ifndef __LIB_UTILS_HVML_URI_H
#define __LIB_UTILS_HVML_URI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Break down an HVML URI in the following pattern:
 *
 *      hvml://<host>/<app>/<runner>/[<group>/]<page>
 *
 */
bool hvml_uri_split(const char *uri,
        char **host, char **app, char **runner, char **group, char **page);

#ifdef __cplusplus
}
#endif

#endif  /* __LIB_UTILS_HVML_URI_H */
