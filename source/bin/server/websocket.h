/**
 *    _______       _______            __        __
 *   / ____/ |     / / ___/____  _____/ /_____  / /_
 *  / / __ | | /| / /\__ \/ __ \/ ___/ //_/ _ \/ __/
 * / /_/ / | |/ |/ /___/ / /_/ / /__/ ,< /  __/ /_
 * \____/  |__/|__//____/\____/\___/_/|_|\___/\__/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2020 Gerardo Orellana <hello @ goaccess.io>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MC_RENDERER_WEBSOCKET_H
#define MC_RENDERER_WEBSOCKET_H

#include <config.h>

#include <time.h>
#include <limits.h>

#include <netinet/in.h>
#include <sys/select.h>

#if HAVE(LIBSSL)
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#if defined(__linux__) || defined(__CYGWIN__)
#  include <endian.h>
#if ((__GLIBC__ == 2) && (__GLIBC_MINOR__ < 9))
#if defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)
#  include <arpa/inet.h>
#  define htobe16(x) htons(x)
#  define htobe64(x) (((uint64_t)htonl(((uint32_t)(((uint64_t)(x)) >> 32)))) | \
   (((uint64_t)htonl(((uint32_t)(x)))) << 32))
#  define be16toh(x) ntohs(x)
#  define be32toh(x) ntohl(x)
#  define be64toh(x) (((uint64_t)ntohl(((uint32_t)(((uint64_t)(x)) >> 32)))) | \
   (((uint64_t)ntohl(((uint32_t)(x)))) << 32))
#else
#  error Byte Order not supported!
#endif
#endif
#elif defined(__sun__)
#  include <sys/byteorder.h>
#  define htobe16(x) BE_16(x)
#  define htobe64(x) BE_64(x)
#  define be16toh(x) BE_IN16(x)
#  define be32toh(x) BE_IN32(x)
#  define be64toh(x) BE_IN64(x)
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  if !defined(be16toh)
#    define be16toh(x) betoh16(x)
#  endif
#  if !defined(be32toh)
#    define be32toh(x) betoh32(x)
#  endif
#  if !defined(be64toh)
#    define be64toh(x) betoh64(x)
#  endif
#elif defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define htobe16(x) OSSwapHostToBigInt16(x)
#  define htobe64(x) OSSwapHostToBigInt64(x)
#  define be16toh(x) OSSwapBigToHostInt16(x)
#  define be32toh(x) OSSwapBigToHostInt32(x)
#  define be64toh(x) OSSwapBigToHostInt64(x)
#else
#  error Platform not supported!
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define WS_BAD_REQUEST_STR "HTTP/1.1 400 Invalid Request\r\n\r\n"
#define WS_SWITCH_PROTO_STR "HTTP/1.1 101 Switching Protocols"
#define WS_TOO_BUSY_STR "HTTP/1.1 503 Service Unavailable\r\n\r\n"
#define WS_INTERNAL_ERROR_STR "HTTP/1.1 505 Internal Server Error\r\n\r\n"

#define CRLF "\r\n"
#define SHA_DIGEST_LENGTH     20

/* packet header is 3 unit32_t : type, size, listener */
#define HDR_SIZE              3 * 4
#define WS_MAX_HEAD_SZ        8192 /* a reasonable size for request headers */

#define WS_MAGIC_STR "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_PAYLOAD_EXT16      126
#define WS_PAYLOAD_EXT64      127
#define WS_PAYLOAD_FULL       125
#define WS_FRM_HEAD_SZ         16       /* frame header size */

#define WS_FRM_FIN(x)         (((x) >> 7) & 0x01)
#define WS_FRM_MASK(x)        (((x) >> 7) & 0x01)
#define WS_FRM_R1(x)          (((x) >> 6) & 0x01)
#define WS_FRM_R2(x)          (((x) >> 5) & 0x01)
#define WS_FRM_R3(x)          (((x) >> 4) & 0x01)
#define WS_FRM_OPCODE(x)      ((x) & 0x0F)
#define WS_FRM_PAYLOAD(x)     ((x) & 0x7F)

#define WS_CLOSE_NORMAL       1000
#define WS_CLOSE_GOING_AWAY   1001
#define WS_CLOSE_PROTO_ERR    1002
#define WS_CLOSE_INVALID_UTF8 1007
#define WS_CLOSE_TOO_LARGE    1009
#define WS_CLOSE_UNEXPECTED   1011

typedef enum WSSTATUS
{
  WS_OK = 0,
  WS_ERR = (1 << 0),
  WS_CLOSE = (1 << 1),
  WS_READING = (1 << 2),
  WS_SENDING = (1 << 3),
  WS_THROTTLING = (1 << 4),
  WS_TLS_ACCEPTING = (1 << 5),
  WS_TLS_READING = (1 << 6),
  WS_TLS_WRITING = (1 << 7),
  WS_TLS_SHUTTING = (1 << 8),
} WSStatus;

typedef enum WSOPCODE
{
  WS_OPCODE_CONTINUATION = 0x00,
  WS_OPCODE_TEXT = 0x01,
  WS_OPCODE_BIN = 0x02,
  WS_OPCODE_END = 0x03,
  WS_OPCODE_CLOSE = 0x08,
  WS_OPCODE_PING = 0x09,
  WS_OPCODE_PONG = 0x0A,
} WSOpcode;

typedef struct WSQueue_
{
  char *queued;                 /* queue data */
  int qlen;                     /* queue length */
} WSQueue;

/* WS HTTP Headers */
typedef struct WSHeaders_
{
  int reading;
  int buflen;
  char buf[WS_MAX_HEAD_SZ + 1];

  char *agent;
  char *path;
  char *method;
  char *protocol;
  char *host;
  char *origin;
  char *upgrade;
  char *referer;
  char *connection;
  char *ws_protocol;
  char *ws_key;
  char *ws_sock_ver;

  char *ws_accept;
  char *ws_resp;
} WSHeaders;

/* A WebSocket Message */
typedef struct WSFrame_
{
  /* frame format */
  WSOpcode opcode;              /* frame opcode */
  unsigned char fin;            /* frame fin flag */
  unsigned char mask[4];        /* mask key */
  uint8_t res;                  /* extensions */
  int payload_offset;           /* end of header/start of payload */
  int payloadlen;               /* payload length (for each frame) */

  /* status flags */
  int reading;                  /* still reading frame's header part? */
  int masking;                  /* are we masking the frame? */

  char buf[WS_FRM_HEAD_SZ + 1]; /* frame's header */
  int buflen;                   /* recv'd buf length so far (for each frame) */
} WSFrame;

/* A WebSocket Message */
typedef struct WSMessage_
{
  WSOpcode opcode;              /* frame opcode */
  int fragmented;               /* reading a fragmented frame */
  int mask_offset;              /* for fragmented frames */

  char *payload;                /* payload message */
  int payloadsz;                /* total payload size (whole message) */
  int buflen;                   /* recv'd buf length so far (for each frame) */
} WSMessage;

/* A WebSocket Client */
typedef struct WSClient_
{
  /* the following fields are same as struct SocketClient_ */
  int             ct;         /* the connection type of the client */
  int             fd;         /* WebSocket fd */
  struct timespec ts;         /* time got the first frame of the current message */
  UpperEntity    *entity;     /* pointer to the uppper entity */

  char remote_ip[INET6_ADDRSTRLEN];     /* client IP */

  WSQueue *sockqueue;           /* sending buffer */
  WSHeaders *headers;           /* HTTP headers */
  WSFrame *frame;               /* frame headers */
  WSMessage *message;           /* message */
  WSStatus status;              /* connection status */

  struct timeval start_proc;
  struct timeval end_proc;

#if HAVE(LIBSSL)
  SSL *ssl;
  WSStatus sslstatus;           /* ssl connection status */
#endif
} WSClient;

struct SockClient_;

/* A WebSocket Instance */
typedef struct WSServer_
{
  /* Server Status */
  int closing;
  int nr_clients;

  /* Callbacks */
  int (*on_accepted) (void *server, struct SockClient_* client);
  int (*on_packet) (void *server, struct SockClient_ * client,
          char* body, unsigned int sz_body, int type);
  int (*on_pending) (void *server, struct SockClient_* client);
  int (*on_close) (void *server, struct SockClient_ * client);
  void (*on_error) (void *server, struct SockClient_* client, int err_code);

#if HAVE(LIBSSL)
  SSL_CTX *ctx;
#endif

  ServerConfig* config;
} WSServer;

size_t pack_uint32 (void *buf, uint32_t val, int convert);
size_t unpack_uint32 (const void *buf, uint32_t * val, int convert);

int ws_ping_client (WSServer * server, WSClient * client);
int ws_close_client (WSServer * server, WSClient * client);
int ws_send_packet (WSServer * server, WSClient * client,
        WSOpcode op, const char *data, int sz);
int ws_send_packet_safe (WSServer * server, WSClient * client,
        WSOpcode op, const char *data, int sz);
int ws_validate_string (const char *str, int len);

WSServer *ws_init (ServerConfig * config);
int ws_initialize_ssl_ctx (WSServer * server);
int ws_listen (WSServer *server);
void ws_stop (WSServer *server);

WSClient* ws_handle_accept (WSServer * server, int listener);
int ws_handle_reads (WSServer * server, WSClient * client);
int ws_handle_writes (WSServer * server, WSClient * client);
int ws_remove_dangling_client (WSServer * server, WSClient *client);
void ws_cleanup_client (WSServer * server, WSClient * client);

#endif // MC_RENDERER_WEBSOCKET_H
