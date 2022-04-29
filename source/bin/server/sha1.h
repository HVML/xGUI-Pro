/*
 * sha1 - Implementation of SHA1 hash function.
 *
 * From http://www.mirrors.wiretapped.net/security/cryptography/hashes/sha1/sha1.c
 *
 * Modify the source code with Unix C style by Vincent Wei
 *  - Nov. 2020
 *
 * Copyright (C) 2020 FMSoft <https://www.fmsoft.cn>
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
 *
 */

#ifndef __MC_RENDERER_SHA1_H
#define __MC_RENDERER_SHA1_H

#include <sys/types.h>
#include <stdint.h>

typedef struct {
  uint32_t      state[5];
  uint32_t      count[2];
  uint8_t       buffer[64];
} Sha1Context;

#define SHA1_DIGEST_SIZE          (20)

#ifdef __cplusplus
extern "C" {
#endif

void sha1_init (Sha1Context *context);
void sha1_update (Sha1Context *context, uint8_t *data, uint32_t len);
void sha1_finalize (Sha1Context *context, uint8_t *digest);

#ifdef __cplusplus
}
#endif

#endif /* __MC_RENDERER_SHA1_H */

