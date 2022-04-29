/**
 * websocket.c -- Utilities for WebSocket server.
 *
 * The original code comes from gwsocket
 *  - An rfc6455-complaint Web Socket Server.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018~2021 FMSoft <https://www.fmsoft.cn>
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "lib/hiboxcompat.h"

#include "sha1.h"
#include "base64.h"
#include "server.h"
#include "websocket.h"

/* *INDENT-OFF* */

/* UTF-8 Decoder */
/* Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details. */
#define UTF8_VALID 0
#define UTF8_INVAL 1
static const uint8_t utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 00..1f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 20..3f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40..5f */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 60..7f */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, /* 80..9f */
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, /* a0..bf */
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* c0..df */
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, /* e0..ef */
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, /* f0..ff */
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, /* s0..s0 */
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, /* s1..s2 */
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, /* s3..s4 */
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, /* s5..s6 */
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* s7..s8 */
};
/* *INDENT-ON* */

static void handle_ws_read_close (WSServer * server, WSClient * client);
#if HAVE(LIBSSL)
static int shutdown_ssl (WSClient * client);
#endif

/* Determine if the given string is valid UTF-8.
 *
 * The state after the by has been processed is returned. */
static uint32_t
verify_utf8 (uint32_t * state, const char *str, int len)
{
  int i;
  uint32_t type;

  for (i = 0; i < len; ++i) {
    type = utf8d[(uint8_t) str[i]];
    *state = utf8d[256 + (*state) * 16 + type];

    if (*state == UTF8_INVAL)
      break;
  }

  return *state;
}

/* Decode a character maintaining state and a byte, and returns the
 * state achieved after processing the byte.
 *
 * The state after the by has been processed is returned. */
static uint32_t
utf8_decode (uint32_t * state, uint32_t * p, uint32_t b)
{
  uint32_t type = utf8d[(uint8_t) b];

  *p = (*state != UTF8_VALID) ? (b & 0x3fu) | (*p << 6) : (0xff >> type) & (b);
  *state = utf8d[256 + *state * 16 + type];

  return *state;
}

/* Replace malformed sequences with a substitute character.
 *
 * On success, it replaces the whole sequence and return a malloc'd buffer. */
static char *
sanitize_utf8 (const char *str, int len)
{
  char *buf = NULL;
  uint32_t state = UTF8_VALID, prev = UTF8_VALID, cp = 0;
  int i = 0, j = 0, k = 0, l = 0;

  buf = calloc (len + 1, sizeof (char));
  for (; i < len; prev = state, ++i) {
    switch (utf8_decode (&state, &cp, (unsigned char) str[i])) {
    case UTF8_INVAL:
      /* replace the whole sequence */
      if (k) {
        for (l = i - k; l < i; ++l)
          buf[j++] = '?';
      } else {
        buf[j++] = '?';
      }
      state = UTF8_VALID;
      if (prev != UTF8_VALID)
        i--;
      k = 0;
      break;
    case UTF8_VALID:
      /* fill i - k valid continuation bytes */
      if (k)
        for (l = i - k; l < i; ++l)
          buf[j++] = str[l];
      buf[j++] = str[i];
      k = 0;
      break;
    default:
      /* UTF8_VALID + continuation bytes */
      k++;
      break;
    }
  }

  return buf;
}

/* Allocate memory for a websocket client */
static WSClient *
new_wsclient (void)
{
    WSClient *ws_client;

    ws_client = calloc (1, sizeof (WSClient));

    ws_client->ct = CT_WEB_SOCKET;
    ws_client->status = WS_OK;

    return ws_client;
}

/* Allocate memory for a websocket header */
static WSHeaders *
new_wsheader (void)
{
  WSHeaders *headers = calloc (1, sizeof (WSHeaders));
  memset (headers->buf, 0, sizeof (headers->buf));
  headers->reading = 1;

  return headers;
}

/* Allocate memory for a websocket frame */
static WSFrame *
new_wsframe (void)
{
  WSFrame *frame = calloc (1, sizeof (WSFrame));
  memset (frame->buf, 0, sizeof (frame->buf));
  frame->reading = 1;

  return frame;
}

/* Allocate memory for a websocket message */
static WSMessage *
new_wsmessage (void)
{
  WSMessage *msg = calloc (1, sizeof (WSMessage));

  return msg;
}

/* Escapes the special characters, e.g., '\n', '\r', '\t', '\'
 * in the string source by inserting a '\' before them.
 *
 * On error NULL is returned.
 * On success the escaped string is returned */
static char *
escape_http_request (const char *src)
{
  char *dest, *q;
  const unsigned char *p;

  if (src == NULL || *src == '\0')
    return NULL;

  p = (unsigned char *) src;
  q = dest = malloc (strlen (src) * 4 + 1);

  while (*p) {
    switch (*p) {
    case '\\':
      *q++ = '\\';
      *q++ = '\\';
      break;
    case '\n':
      *q++ = '\\';
      *q++ = 'n';
      break;
    case '\r':
      *q++ = '\\';
      *q++ = 'r';
      break;
    case '\t':
      *q++ = '\\';
      *q++ = 't';
      break;
    case '"':
      *q++ = '\\';
      *q++ = '"';
      break;
    default:
      if ((*p < ' ') || (*p >= 0177)) {
        /* not ASCII */
      } else {
        *q++ = *p;
      }
      break;
    }
    p++;
  }
  *q = 0;
  return dest;
}

/* Make a string uppercase.
 *
 * On error the original string is returned.
 * On success, the uppercased string is returned. */
static char *
strtoupper (char *str)
{
  char *p = str;
  if (str == NULL || *str == '\0')
    return str;

  while (*p != '\0') {
    *p = toupper (*p);
    p++;
  }

  return str;
}

/* Chop n characters from the beginning of the supplied buffer.
 *
 * The new length of the string is returned. */
static size_t
chop_nchars (char *str, size_t n, size_t len)
{
  if (n == 0 || str == 0)
    return 0;

  if (n > len)
    n = len;
  memmove (str, str + n, len - n);

  return (len - n);
}

/* Free a frame structure and its data for the given client. */
static void
ws_free_frame (WSClient * client)
{
  if (client->frame)
    free (client->frame);
  client->frame = NULL;
}

/* Free a message structure and its data for the given client. */
static void
ws_free_message (WSClient * client)
{
  if (client->message && client->message->payload)
    free (client->message->payload);
  if (client->message)
    free (client->message);
  client->message = NULL;

  update_upper_entity_stats (client->entity,
          client->sockqueue ? client->sockqueue->qlen : 0, 0);
}

/* Free all HTTP handshake headers data for the given client. */
static void
ws_free_header_fields (WSHeaders * headers)
{
  if (headers->connection)
    free (headers->connection);
  if (headers->host)
    free (headers->host);
  if (headers->agent)
    free (headers->agent);
  if (headers->method)
    free (headers->method);
  if (headers->origin)
    free (headers->origin);
  if (headers->path)
    free (headers->path);
  if (headers->protocol)
    free (headers->protocol);
  if (headers->upgrade)
    free (headers->upgrade);
  if (headers->ws_accept)
    free (headers->ws_accept);
  if (headers->ws_key)
    free (headers->ws_key);
  if (headers->ws_protocol)
    free (headers->ws_protocol);
  if (headers->ws_resp)
    free (headers->ws_resp);
  if (headers->ws_sock_ver)
    free (headers->ws_sock_ver);
  if (headers->referer)
    free (headers->referer);
}

/* Clear the client's sent queue and its data. */
static void
ws_clear_queue (WSClient * client)
{
  WSQueue **queue = &client->sockqueue;
  if (!(*queue))
    return;

  if ((*queue)->queued)
    free ((*queue)->queued);
  (*queue)->queued = NULL;
  (*queue)->qlen = 0;

  free ((*queue));
  (*queue) = NULL;

  /* done sending the whole queue, stop throttling */
  client->status &= ~WS_THROTTLING;
  /* done sending, close connection if set to close */
  if ((client->status & WS_CLOSE) && (client->status & WS_SENDING))
    client->status = WS_CLOSE;

  update_upper_entity_stats (client->entity,
          client->sockqueue ? client->sockqueue->qlen : 0,
          client->message ? client->message->payloadsz : 0);
}

/* Free all HTTP handshake headers and structure. */
static void
ws_clear_handshake_headers (WSHeaders * headers)
{
  ws_free_header_fields (headers);
  free (headers);
  headers = NULL;
}

#if HAVE(LIBSSL)
/* Attempt to send the TLS/SSL "close notify" shutdown and and removes
 * the SSL structure pointed to by ssl and frees up the allocated
 * memory. */
static void
ws_shutdown_dangling_clients (WSClient * client)
{
  shutdown_ssl (client);
  SSL_free (client->ssl);
  client->ssl = NULL;
}

/* Attempt to remove the SSL_CTX object pointed to by ctx and frees up
 * the allocated memory and cleans some more generally used TLS/SSL
 * memory.  */
static void
ws_ssl_cleanup (WSServer * server)
{
  if (!server->config->use_ssl)
    return;

  if (server->ctx)
    SSL_CTX_free (server->ctx);

  CRYPTO_cleanup_all_ex_data ();
  CRYPTO_set_id_callback (NULL);
  CRYPTO_set_locking_callback (NULL);
  ERR_free_strings ();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_remove_state (0);
#endif
  EVP_cleanup ();
}
#endif

/* Remove all clients that are still hanging out. */
int
ws_remove_dangling_client (WSServer * server, WSClient *client)
{
  if (client == NULL)
    return 1;

  if (client->headers)
    ws_clear_handshake_headers (client->headers);
  if (client->sockqueue)
    ws_clear_queue (client);
#if HAVE(LIBSSL)
  if (client->ssl)
    ws_shutdown_dangling_clients (client);
#endif

  server->nr_clients--;
  assert (server->nr_clients >= 0);
  return 0;
}

/* Stop the server and do some clearning. */
void
ws_stop (WSServer * server)
{
#if HAVE(LIBSSL)
  ws_ssl_cleanup (server);
#endif

  free (server);
}

/* A wrapper to close a socket. */
static inline void
ws_close (WSClient* client)
{
  close (client->fd);
}

/* Set the connection status for the given client and return the given
 * bytes.
 *
 * The given number of bytes are returned. */
static inline int
ws_set_status (WSClient * client, WSStatus status, int bytes)
{
  client->status = status;
  return bytes;
}

/* Append the source string to destination and reallocates and
 * updating the destination buffer appropriately. */
static void
ws_append_str (char **dest, const char *src)
{
  size_t curlen = strlen (*dest);
  size_t srclen = strlen (src);
  size_t newlen = curlen + srclen;

  char *str = realloc (*dest, newlen + 1);
  memcpy (str + curlen, src, srclen + 1);
  *dest = str;
}

#if HAVE(LIBSSL)
/* Create a new SSL_CTX object as framework to establish TLS/SSL
 * enabled connections.
 *
 * On error 1 is returned.
 * On success, SSL_CTX object is malloc'd and 0 is returned.
 */
int
ws_initialize_ssl_ctx (WSServer * server)
{
  int ret = 1;
  SSL_CTX *ctx = NULL;

  SSL_load_error_strings ();
  /* Ciphers and message digests */
  OpenSSL_add_ssl_algorithms ();

  /* ssl context */
  if (!(ctx = SSL_CTX_new (SSLv23_server_method ())))
    goto out;
  /* set certificate */
  if (!SSL_CTX_use_certificate_file (ctx, server->config->sslcert, SSL_FILETYPE_PEM))
    goto out;
  /* ssl private key */
  if (!SSL_CTX_use_PrivateKey_file (ctx, server->config->sslkey, SSL_FILETYPE_PEM))
    goto out;
  if (!SSL_CTX_check_private_key (ctx))
    goto out;

  /* since we queued up the send data, a retry won't be the same buffer,
   * thus we need the following flags */
  SSL_CTX_set_mode (ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER |
                    SSL_MODE_ENABLE_PARTIAL_WRITE);

  server->ctx = ctx;
  ret = 0;
out:
  if (ret) {
    SSL_CTX_free (ctx);
    ULOG_NOTE ("Error: %s\n", ERR_error_string (ERR_get_error (), NULL));
  }

  return ret;
}

/* Log result code for TLS/SSL I/O operation */
static void
log_return_message (int ret, int err, const char *fn)
{
  unsigned long e;

  switch (err) {
  case SSL_ERROR_NONE:
    ULOG_NOTE ("SSL: %s -> SSL_ERROR_NONE\n", fn);
    ULOG_NOTE ("SSL: TLS/SSL I/O operation completed\n");
    break;
  case SSL_ERROR_WANT_READ:
    ULOG_NOTE ("SSL: %s -> SSL_ERROR_WANT_READ\n", fn);
    ULOG_NOTE ("SSL: incomplete, data available for reading\n");
    break;
  case SSL_ERROR_WANT_WRITE:
    ULOG_NOTE ("SSL: %s -> SSL_ERROR_WANT_WRITE\n", fn);
    ULOG_NOTE ("SSL: incomplete, data available for writing\n");
    break;
  case SSL_ERROR_ZERO_RETURN:
    ULOG_NOTE ("SSL: %s -> SSL_ERROR_ZERO_RETURN\n", fn);
    ULOG_NOTE ("SSL: TLS/SSL connection has been closed\n");
    break;
  case SSL_ERROR_WANT_X509_LOOKUP:
    ULOG_NOTE ("SSL: %s -> SSL_ERROR_WANT_X509_LOOKUP\n", fn);
    break;
  case SSL_ERROR_SYSCALL:
    ULOG_NOTE ("SSL: %s -> SSL_ERROR_SYSCALL\n", fn);

    e = ERR_get_error ();
    if (e > 0)
      ULOG_NOTE ("SSL: %s -> %s\n", fn, ERR_error_string (e, NULL));

    /* call was not successful because a fatal error occurred either at the
     * protocol level or a connection failure occurred. */
    if (ret != 0) {
      ULOG_NOTE ("SSL bogus handshake interrupt: %s\n", strerror (errno));
      break;
    }
    /* call not yet finished. */
    ULOG_NOTE ("SSL: handshake interrupted, got EOF\n");
    if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
      ULOG_NOTE ("SSL: %s -> not yet finished %s\n", fn, strerror (errno));
    break;
  default:
    ULOG_NOTE ("SSL: %s -> failed fatal error code: %d\n", fn, err);
    ULOG_NOTE ("SSL: %s\n", ERR_error_string (ERR_get_error (), NULL));
    break;
  }
}

/* Shut down the client's TLS/SSL connection
 *
 * On fatal error, 1 is returned.
 * If data still needs to be read/written, -1 is returned.
 * On success, the TLS/SSL connection is closed and 0 is returned */
static int
shutdown_ssl (WSClient * client)
{
  int ret = -1, err = 0;

  /* all good */
  if ((ret = SSL_shutdown (client->ssl)) > 0)
    return ws_set_status (client, WS_CLOSE, 0);

  err = SSL_get_error (client->ssl, ret);
  log_return_message (ret, err, "SSL_shutdown");

  switch (err) {
  case SSL_ERROR_WANT_READ:
  case SSL_ERROR_WANT_WRITE:
    client->sslstatus = WS_TLS_SHUTTING;
    break;
  case SSL_ERROR_SYSCALL:
    if (ret == 0) {
      ULOG_NOTE ("SSL: SSL_shutdown, connection unexpectedly closed by peer.\n");
      /* The shutdown is not yet finished. */
      if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
        client->sslstatus = WS_TLS_SHUTTING;
      break;
    }
    ULOG_NOTE ("SSL: SSL_shutdown, probably unrecoverable, forcing close.\n");
  case SSL_ERROR_ZERO_RETURN:
  case SSL_ERROR_WANT_X509_LOOKUP:
  default:
    return ws_set_status (client, WS_ERR | WS_CLOSE, 1);
  }

  return ret;
}

/* Wait for a TLS/SSL client to initiate a TLS/SSL handshake
 *
 * On fatal error, the connection is shut down.
 * If data still needs to be read/written, -1 is returned.
 * On success, the TLS/SSL connection is completed and 0 is returned */
static int
accept_ssl (WSClient * client)
{
  int ret = -1, err = 0;

  /* all good on TLS handshake */
  if ((ret = SSL_accept (client->ssl)) > 0) {
    client->sslstatus &= ~WS_TLS_ACCEPTING;
    return 0;
  }

  err = SSL_get_error (client->ssl, ret);
  log_return_message (ret, err, "SSL_accept");

  switch (err) {
  case SSL_ERROR_WANT_READ:
  case SSL_ERROR_WANT_WRITE:
    client->sslstatus = WS_TLS_ACCEPTING;
    break;
  case SSL_ERROR_SYSCALL:
    /* Wait for more activity else bail out, for instance if the socket is closed
     * during the handshake. */
    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      client->sslstatus = WS_TLS_ACCEPTING;
      break;
    }
    /* The peer notified that it is shutting down through a SSL "close_notify" so
     * we shutdown too */
  case SSL_ERROR_ZERO_RETURN:
  case SSL_ERROR_WANT_X509_LOOKUP:
  default:
    client->sslstatus &= ~WS_TLS_ACCEPTING;
    return ws_set_status (client, WS_ERR | WS_CLOSE, 1);
  }

  return ret;
}

/* Create a new SSL structure for a connection and perform handshake */
static void
handle_accept_ssl (WSServer * server, WSClient * client)
{
  /* attempt to create SSL connection if we don't have one yet */
  if (!client->ssl) {
    if (!(client->ssl = SSL_new (server->ctx))) {
      ULOG_NOTE ("SSL: SSL_new, new SSL structure failed.\n");
      return;
    }
    if (!SSL_set_fd (client->ssl, client->fd)) {
      ULOG_NOTE ("SSL: unable to set file descriptor\n");
      return;
    }
  }

  /* attempt to initiate the TLS/SSL handshake */
  if (accept_ssl (client) == 0) {
    ULOG_NOTE ("SSL Accepted: %d %s\n", client->fd, client->remote_ip);
  }
}

/* Given the current status of the SSL buffer, perform that action.
 *
 * On error or if no SSL pending status, 1 is returned.
 * On success, the TLS/SSL pending action is called and 0 is returned */
static int
handle_ssl_pending_rw (WSServer * server, WSClient * client)
{
  if (!server->config->use_ssl)
    return 1;

  /* trying to write but still waiting for a successful SSL_accept */
  if (client->sslstatus & WS_TLS_ACCEPTING) {
    handle_accept_ssl (server, client);
    return 0;
  }
  /* trying to read but still waiting for a successful SSL_read */
  if (client->sslstatus & WS_TLS_READING) {
    ws_handle_reads (server, client);
    return 0;
  }
  /* trying to write but still waiting for a successful SSL_write */
  if (client->sslstatus & WS_TLS_WRITING) {
    ws_handle_writes (server, client);
    return 0;
  }
  /* trying to write but still waiting for a successful SSL_shutdown */
  if (client->sslstatus & WS_TLS_SHUTTING) {
    if (shutdown_ssl (client) == 0)
      handle_ws_read_close (server, client);
    return 0;
  }

  return 1;
}

/* Write bytes to a TLS/SSL connection for a given client.
 *
 * On error or if no write is performed <=0 is returned.
 * On success, the number of bytes actually written to the TLS/SSL
 * connection are returned */
static int
send_ssl_buffer (WSClient * client, const char *buffer, int len)
{
  int bytes = 0, err = 0;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_clear_error ();
#endif
  if ((bytes = SSL_write (client->ssl, buffer, len)) > 0)
    return bytes;

  err = SSL_get_error (client->ssl, bytes);
  log_return_message (bytes, err, "SSL_write");

  switch (err) {
  case SSL_ERROR_WANT_WRITE:
    break;
  case SSL_ERROR_WANT_READ:
    client->sslstatus = WS_TLS_WRITING;
    break;
  case SSL_ERROR_SYSCALL:
    if ((bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)))
      break;
    /* The connection was shut down cleanly */
  case SSL_ERROR_ZERO_RETURN:
  case SSL_ERROR_WANT_X509_LOOKUP:
  default:
    return ws_set_status (client, WS_ERR | WS_CLOSE, -1);
  }

  return bytes;
}

/* Read data from the given client's socket and set a connection
 * status given the output of recv().
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes read is returned. */
static int
read_ssl_socket (WSClient * client, char *buffer, int size)
{
  int bytes = 0, done = 0, err = 0;
  do {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    ERR_clear_error ();
#endif

    done = 0;
    if ((bytes = SSL_read (client->ssl, buffer, size)) > 0)
      break;

    err = SSL_get_error (client->ssl, bytes);
    log_return_message (bytes, err, "SSL_read");

    switch (err) {
    case SSL_ERROR_WANT_WRITE:
      client->sslstatus = WS_TLS_READING;
      done = 1;
      break;
    case SSL_ERROR_WANT_READ:
      done = 1;
      break;
    case SSL_ERROR_SYSCALL:
      if ((bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)))
        break;
    case SSL_ERROR_ZERO_RETURN:
    case SSL_ERROR_WANT_X509_LOOKUP:
    default:
      return ws_set_status (client, WS_ERR | WS_CLOSE, -1);
    }
  } while (SSL_pending (client->ssl) && !done);

  return bytes;
}
#endif

/* Get sockaddr, either IPv4 or IPv6 */
static void *
ws_get_raddr (struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
    return &(((struct sockaddr_in *) (void *)sa)->sin_addr);

  return &(((struct sockaddr_in6 *) (void *)sa)->sin6_addr);
}

/* Set the given file descriptor as NON BLOCKING. */
inline static void
set_nonblocking (int sock)
{
  if (fcntl (sock, F_SETFL, fcntl (sock, F_GETFL, 0) | O_NONBLOCK) == -1)
    ULOG_ERR ("Unable to set socket as non-blocking: %s.", strerror (errno));
}

/* Accept a new connection on a socket and add it to the list of
 * current connected clients.
 *
 * The newly assigned socket is returned. */
static WSClient *
accept_client (int listener /*, GSLList ** colist */)
{
  WSClient *client;
  struct sockaddr_storage raddr;
  int newfd;
  const void *src = NULL;
  socklen_t alen;

  alen = sizeof (raddr);
  if ((newfd = accept (listener, (struct sockaddr *) &raddr, &alen)) == -1)
    ULOG_ERR ("Unable to set accept: %s.", strerror (errno));

  fcntl (newfd, F_SETFD, FD_CLOEXEC);

  if (newfd == -1) {
    ULOG_NOTE ("Unable to accept: %s.", strerror (errno));
    return NULL;
  }
  src = ws_get_raddr ((struct sockaddr *) &raddr);

  /* malloc a new client */
  client = new_wsclient ();
  client->fd = newfd;
  inet_ntop (raddr.ss_family, src, client->remote_ip, INET6_ADDRSTRLEN);

  /* make the socket non-blocking */
  set_nonblocking (client->fd);

  return client;
}

/* Extract the HTTP method.
 *
 * On error, or if not found, NULL is returned.
 * On success, the HTTP method is returned. */
static const char *
ws_get_method (const char *token)
{
  const char *lookfor = NULL;

  if ((lookfor = "GET", !memcmp (token, "GET ", 4)) ||
      (lookfor = "get", !memcmp (token, "get ", 4)))
    return lookfor;
  return NULL;
}

/* Parse a request containing the method and protocol.
 *
 * On error, or unable to parse, NULL is returned.
 * On success, the HTTP request is returned and the method and
 * protocol are assigned to the corresponding buffers. */
static char *
ws_parse_request (char *line, char **method, char **protocol)
{
  const char *meth;
  char *req = NULL, *request = NULL, *proto = NULL;
  ptrdiff_t rlen;

  if ((meth = ws_get_method (line)) == NULL) {
    return NULL;
  } else {
    req = line + strlen (meth);
    if ((proto = strstr (line, " HTTP/1.0")) == NULL &&
        (proto = strstr (line, " HTTP/1.1")) == NULL)
      return NULL;

    req++;
    if ((rlen = proto - req) <= 0)
      return NULL;

    request = malloc (rlen + 1);
    strncpy (request, req, rlen);
    request[rlen] = 0;

    (*method) = strtoupper (strdup (meth));
    (*protocol) = strtoupper (strdup (++proto));
  }

  return request;
}

/* Given a pair of key/values, assign it to our HTTP headers
 * structure. */
static void
ws_set_header_key_value (WSHeaders * headers, char *key, char *value)
{
  if (strcasecmp ("Host", key) == 0)
    headers->host = strdup (value);
  else if (strcasecmp ("Origin", key) == 0)
    headers->origin = strdup (value);
  else if (strcasecmp ("Upgrade", key) == 0)
    headers->upgrade = strdup (value);
  else if (strcasecmp ("Connection", key) == 0)
    headers->connection = strdup (value);
  else if (strcasecmp ("Sec-WebSocket-Protocol", key) == 0)
    headers->ws_protocol = strdup (value);
  else if (strcasecmp ("Sec-WebSocket-Key", key) == 0)
    headers->ws_key = strdup (value);
  else if (strcasecmp ("Sec-WebSocket-Version", key) == 0)
    headers->ws_sock_ver = strdup (value);
  else if (strcasecmp ("User-Agent", key) == 0)
    headers->agent = strdup (value);
  else if (strcasecmp ("Referer", key) == 0)
    headers->referer = strdup (value);
}

/* Verify that the given HTTP headers were passed upon doing the
 * websocket handshake.
 *
 * On error, or header missing, 1 is returned.
 * On success, 0 is returned. */
static int
ws_verify_req_headers (WSServer *server, WSHeaders * headers)
{
  if (!headers->host)
    return 1;
  if (!headers->method)
    return 1;
  if (!headers->protocol)
    return 1;
  if (!headers->path)
    return 1;
  if (server->config->origin && !headers->origin)
    return 1;
  if (server->config->origin && strcasecmp (server->config->origin, headers->origin) != 0)
    return 1;
  if (!headers->connection)
    return 1;
  if (!headers->ws_key)
    return 1;
  if (!headers->ws_sock_ver)
    return 1;
  return 0;
}

/* From RFC2616, each header field consists of a name followed by a
 * colon (":") and the field value. Field names are case-insensitive.
 * The field value MAY be preceded by any amount of LWS, though a
 * single SP is preferred */
static int
ws_set_header_fields (char *line, WSHeaders * headers)
{
  char *path = NULL, *method = NULL, *proto = NULL, *p, *value;

  if (line[0] == '\n' || line[0] == '\r')
    return 1;

  if ((strstr (line, "GET ")) || (strstr (line, "get "))) {
    if ((path = ws_parse_request (line, &method, &proto)) == NULL)
      return 1;
    headers->path = path;
    headers->method = method;
    headers->protocol = proto;

    return 0;
  }

  if ((p = strchr (line, ':')) == NULL)
    return 1;

  value = p + 1;
  while (p != line && isspace ((unsigned char) *(p - 1)))
    p--;

  if (p == line)
    return 1;

  *p = '\0';
  if (strpbrk (line, " \t") != NULL) {
    *p = ' ';
    return 1;
  }
  while (isspace ((unsigned char) *value))
    value++;

  ws_set_header_key_value (headers, line, value);

  return 0;
}

/* Parse the given HTTP headers and set the expected websocket
 * handshake.
 *
 * On error, or 1 is returned.
 * On success, 0 is returned. */
static int
parse_headers (WSHeaders * headers)
{
  char *tmp = NULL;
  const char *buffer = headers->buf;
  const char *line = buffer, *next = NULL;
  int len = 0;

  while (line) {
    if ((next = strstr (line, "\r\n")) != NULL)
      len = (next - line);
    else
      len = strlen (line);

    if (len <= 0)
      return 1;

    tmp = malloc (len + 1);
    memcpy (tmp, line, len);
    tmp[len] = '\0';

    if (ws_set_header_fields (tmp, headers) == 1) {
      free (tmp);
      return 1;
    }

    free (tmp);
    line = next ? (next + 2) : NULL;

    if (strcmp (next, "\r\n\r\n") == 0)
      break;
  }

  return 0;
}

/* Set into a queue the data that couldn't be sent. */
static void
ws_queue_sockbuf (WSClient * client, const char *buffer, int len, int bytes)
{
  WSQueue *queue = calloc (1, sizeof (WSQueue));

  if (bytes < 1)
    bytes = 0;

  queue->queued = calloc (len - bytes, sizeof (char));
  memcpy (queue->queued, buffer + bytes, len - bytes);
  queue->qlen = len - bytes;
  client->sockqueue = queue;

  update_upper_entity_stats (client->entity,
          client->sockqueue ? client->sockqueue->qlen : 0,
          client->message ? client->message->payloadsz : 0);

  client->status |= WS_SENDING;
}

/* Read data from the given client's socket and set a connection
 * status given the output of recv().
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes read is returned. */
static int
read_plain_socket (WSClient * client, char *buffer, int size)
{
  int bytes = 0;

  bytes = recv (client->fd, buffer, size, 0);

  if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return ws_set_status (client, WS_READING, bytes);
  else if (bytes == -1 || bytes == 0)
    return ws_set_status (client, WS_ERR | WS_CLOSE, bytes);

  return bytes;
}

/* Read data from the given client's socket and set a connection
 * status given the output of recv().
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes read is returned. */
static int
read_socket (WSServer * server, WSClient * client, char *buffer, int size)
{
  (void)server;

#if HAVE(LIBSSL)
  if (server->config->use_ssl)
    return read_ssl_socket (client, buffer, size);
  else
    return read_plain_socket (client, buffer, size);
#else
  return read_plain_socket (client, buffer, size);
#endif
}

static int
send_plain_buffer (WSClient * client, const char *buffer, int len)
{
  return send (client->fd, buffer, len, 0);
}

static int
send_buffer (WSServer * server, WSClient * client, const char *buffer, int len)
{
  (void)server;
#if HAVE(LIBSSL)
  if (server->config->use_ssl)
    return send_ssl_buffer (client, buffer, len);
  else
    return send_plain_buffer (client, buffer, len);
#else
  return send_plain_buffer (client, buffer, len);
#endif
}

/* Attmpt to send the given buffer to the given socket.
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned. */
static int
ws_respond_data (WSServer * server, WSClient * client, const char *buffer, int len)
{
  int bytes = 0;

  bytes = send_buffer (server, client, buffer, len);
  if (bytes == -1 && errno == EPIPE)
    return ws_set_status (client, WS_ERR | WS_CLOSE, bytes);

  /* did not send all of it... buffer it for a later attempt */
  if (bytes < len || (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
    ws_queue_sockbuf (client, buffer, len, bytes);

    if (client->status & WS_SENDING && server->on_pending)
        server->on_pending (server, (SockClient *)client);
  }

  return bytes;
}

/* Attempt to send the queued up client's data to the given socket.
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned. */
static int
ws_respond_cache (WSServer* server, WSClient * client)
{
  WSQueue *queue = client->sockqueue;
  int bytes = 0;

  bytes = send_buffer (server, client, queue->queued, queue->qlen);
  if (bytes == -1 && errno == EPIPE)
    return ws_set_status (client, WS_ERR | WS_CLOSE, bytes);

  if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return bytes;

  if (chop_nchars (queue->queued, bytes, queue->qlen) == 0)
    ws_clear_queue (client);
  else
    queue->qlen -= bytes;

  return bytes;
}

/* Attempt to realloc the current sent queue.
 *
 * On error, 1 is returned and the connection status is set.
 * On success, 0 is returned. */
static int
ws_realloc_send_buf (WSClient * client, const char *buf, int len)
{
  WSQueue *queue = client->sockqueue;
  char *tmp = NULL;
  int newlen = 0;

  newlen = queue->qlen + len;
  tmp = realloc (queue->queued, newlen);
  if (tmp == NULL && newlen > 0) {
    ws_clear_queue (client);
    return ws_set_status (client, WS_ERR | WS_CLOSE, 1);
  }
  queue->queued = tmp;
  memcpy (queue->queued + queue->qlen, buf, len);
  queue->qlen += len;

  update_upper_entity_stats (client->entity,
          client->sockqueue ? client->sockqueue->qlen : 0,
          client->message ? client->message->payloadsz : 0);

  /* client probably  too slow, so stop queueing until everything is
   * sent */
  if (queue->qlen >= SOCK_THROTTLE_THLD)
    client->status |= WS_THROTTLING;

  return 0;
}

/* An entry point to attempt to send the client's data.
 *
 * On error, 1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned. */
static int
ws_respond (WSServer * server, WSClient * client, const char *buffer, int len)
{
  int bytes = 0;

  /* attempt to send the whole buffer */
  if (client->sockqueue == NULL)
    bytes = ws_respond_data (server, client, buffer, len);
  /* buffer not empty, just append new data if we're not throttling the
   * client */
  else if (client->sockqueue != NULL && buffer != NULL &&
           !(client->status & WS_THROTTLING)) {
    if (ws_realloc_send_buf (client, buffer, len) == 1)
      return bytes;
  }
  /* send from cache buffer */
  else {
    bytes = ws_respond_cache (server, client);
  }

  return bytes;
}

/* Encode a websocket frame (header/message) and attempt to send it
 * through the client's socket.
 *
 * On success, 0 is returned. */
static int
ws_send_frame (WSServer * server, WSClient * client, WSOpcode opcode, const char *p, int sz)
{
  unsigned char buf[32] = { 0 };
  char *frm = NULL;
  uint64_t payloadlen = 0, u64;
  int hsize = 2;

  if (sz < 126) {
    payloadlen = sz;
  } else if (sz < (1 << 16)) {
    payloadlen = WS_PAYLOAD_EXT16;
    hsize += 2;
  } else {
    payloadlen = WS_PAYLOAD_EXT64;
    hsize += 8;
  }

  buf[0] = 0x80 | ((uint8_t) opcode);
  switch (payloadlen) {
  case WS_PAYLOAD_EXT16:
    buf[1] = WS_PAYLOAD_EXT16;
    buf[2] = (sz & 0xff00) >> 8;
    buf[3] = (sz & 0x00ff) >> 0;
    break;
  case WS_PAYLOAD_EXT64:
    buf[1] = WS_PAYLOAD_EXT64;
    u64 = htobe64 (sz);
    memcpy (buf + 2, &u64, sizeof (uint64_t));
    break;
  default:
    buf[1] = (sz & 0xff);
  }
  frm = calloc (hsize + sz, sizeof (unsigned char));
  memcpy (frm, buf, hsize);
  if (p != NULL && sz > 0)
    memcpy (frm + hsize, p, sz);

  ws_respond (server, client, frm, hsize + sz);
  free (frm);

  return 0;
}

/* Send an error message to the given client.
 *
 * On success, the number of sent bytes is returned. */
static int
ws_error (WSServer * server, WSClient * client, unsigned short code, const char *err)
{
  unsigned int len;
  unsigned short code_be;
  char buf[128] = { 0 };

  len = 2;
  code_be = htobe16 (code);
  memcpy (buf, &code_be, 2);
  if (err)
    len += snprintf (buf + 2, sizeof buf - 4, "%s", err);

  return ws_send_frame (server, client, WS_OPCODE_CLOSE, buf, len);
}

/* Log hit to the access log.
 *
 * On success, the hit/entry is logged. */
static void
access_log (WSClient * client, int status_code)
{
  WSHeaders *hdrs = client->headers;
  char buf[64] = { 0 };
  uint32_t elapsed = 0;
  struct timeval tv;
  char *req = NULL, *ref = NULL, *ua = NULL;

  gettimeofday (&tv, NULL);
  strftime (buf, sizeof (buf) - 1, "[%d/%b/%Y:%H:%M:%S %z]",
            localtime (&tv.tv_sec));

  elapsed = (client->end_proc.tv_sec - client->start_proc.tv_sec) * 1000.0;
  elapsed += (client->end_proc.tv_usec - client->start_proc.tv_usec) / 1000.0;

  req = escape_http_request (hdrs->path);
  ref = escape_http_request (hdrs->referer);
  ua = escape_http_request (hdrs->agent);

  ULOG_INFO ("%s ", client->remote_ip);
  ULOG_INFO ("- - ");
  ULOG_INFO ("%s ", buf);
  ULOG_INFO ("\"%s ", hdrs->method);
  ULOG_INFO ("%s ", req ? req : "-");
  ULOG_INFO ("%s\" ", hdrs->protocol);
  ULOG_INFO ("%d ", status_code);
  ULOG_INFO ("%d ", hdrs->buflen);
  ULOG_INFO ("\"%s\" ", ref ? ref : "-");
  ULOG_INFO ("\"%s\" ", ua ? ua : "-");
  ULOG_INFO ("%u\n", elapsed);

  if (req)
    free (req);
  if (ref)
    free (ref);
  if (ua)
    free (ua);
}

/* Send an HTTP error status to the given client.
 *
 * On success, the number of sent bytes is returned. */
static int
http_error (WSServer *server, WSClient *client, const char *buffer)
{
  /* do access logging */
  gettimeofday (&client->end_proc, NULL);
  if (server->config->accesslog)
    access_log (client, 400);

  return ws_respond (server, client, buffer, strlen (buffer));
}

/* Compute the SHA1 for the handshake. */
static void
ws_sha1_digest (const char *s, int len, unsigned char *digest)
{
  Sha1Context sha;

  sha1_init (&sha);
  sha1_update (&sha, (uint8_t *) s, len);
  sha1_finalize (&sha, digest);
}

/* Set the parsed websocket handshake headers. */
static void
ws_set_handshake_headers (WSHeaders * headers)
{
  size_t klen = strlen (headers->ws_key);
  size_t mlen = strlen (WS_MAGIC_STR);
  size_t len = klen + mlen;
  char *s = malloc (klen + mlen + 1);
  uint8_t digest[SHA_DIGEST_LENGTH];

  memset (digest, 0, sizeof *digest);

  memcpy (s, headers->ws_key, klen);
  memcpy (s + klen, WS_MAGIC_STR, mlen + 1);

  ws_sha1_digest (s, len, digest);

  /* set response headers */
  headers->ws_accept =
    b64_encode_alloc ((unsigned char *) digest, sizeof (digest));
  headers->ws_resp = strdup (WS_SWITCH_PROTO_STR);

  if (!headers->upgrade)
    headers->upgrade = strdup ("websocket");
  if (!headers->connection)
    headers->upgrade = strdup ("Upgrade");

  free (s);
}

/* Send the websocket handshake headers to the given client.
 *
 * On success, the number of sent bytes is returned. */
static int
ws_send_handshake_headers (WSServer * server, WSClient * client, WSHeaders * headers)
{
  int bytes = 0;
  char *str = strdup ("");

  ws_append_str (&str, headers->ws_resp);
  ws_append_str (&str, CRLF);

  ws_append_str (&str, "Upgrade: ");
  ws_append_str (&str, headers->upgrade);
  ws_append_str (&str, CRLF);

  ws_append_str (&str, "Connection: ");
  ws_append_str (&str, headers->connection);
  ws_append_str (&str, CRLF);

  ws_append_str (&str, "Sec-WebSocket-Accept: ");
  ws_append_str (&str, headers->ws_accept);
  ws_append_str (&str, CRLF CRLF);

  bytes = ws_respond (server, client, str, strlen (str));
  free (str);

  return bytes;
}

/* Given the HTTP connection headers, attempt to parse the web socket
 * handshake headers.
 *
 * On success, the number of sent bytes is returned. */
static int
ws_get_handshake (WSServer * server, WSClient * client)
{
  int bytes = 0, readh = 0;
  char *buf = NULL;

  if (client->headers == NULL)
    client->headers = new_wsheader ();

  buf = client->headers->buf;
  readh = client->headers->buflen;
  /* Probably the connection was closed before finishing handshake */
  if ((bytes = read_socket (server, client, buf + readh, WS_MAX_HEAD_SZ - readh)) < 1) {
    if (client->status & WS_CLOSE)
      http_error (server, client, WS_BAD_REQUEST_STR);
    return bytes;
  }
  client->headers->buflen += bytes;

  buf[client->headers->buflen] = '\0';  /* null-terminate */

  /* Must have a \r\n\r\n */
  if (strstr (buf, "\r\n\r\n") == NULL) {
    if (strlen (buf) < WS_MAX_HEAD_SZ)
      return ws_set_status (client, WS_READING, bytes);

    http_error (server, client, WS_BAD_REQUEST_STR);
    return ws_set_status (client, WS_CLOSE, bytes);
  }

  /* Ensure we have valid HTTP headers for the handshake */
  if (parse_headers (client->headers) != 0) {
    http_error (server, client, WS_BAD_REQUEST_STR);
    return ws_set_status (client, WS_CLOSE, bytes);
  }

  /* Ensure we have the required headers */
  if (ws_verify_req_headers (server, client->headers) != 0) {
    http_error (server, client, WS_BAD_REQUEST_STR);
    return ws_set_status (client, WS_CLOSE, bytes);
  }

  ws_set_handshake_headers (client->headers);

  /* handshake response */
  ws_send_handshake_headers (server, client, client->headers);

  client->headers->reading = 0;

  /* do access logging */
  gettimeofday (&client->end_proc, NULL);
  if (server->config->accesslog)
    access_log (client, 101);
  ULOG_NOTE ("Active: %d\n", server->nr_clients);

  return ws_set_status (client, WS_OK, bytes);
}

/* Ping client
 *
 * On success, 0 is returned. */
int
ws_ping_client (WSServer * server, WSClient * client)
{
    return ws_send_frame (server, client, WS_OPCODE_PING, NULL, 0);
}

/* Close client
 *
 * On success, 0 is returned. */
int
ws_close_client (WSServer * server, WSClient * client)
{
    return ws_send_frame (server, client, WS_OPCODE_CLOSE, NULL, 0);
}

/* Send a data message to the given client.
 *
 * On success, 0 is returned. */
int
ws_send_packet (WSServer *server, WSClient *client, WSOpcode opcode, const char *p, int sz)
{
#if 0   /* bad implementation */
  char *buf = NULL;

  if (opcode != WS_OPCODE_BIN) {
    buf = sanitize_utf8 (p, sz);
  } else {
    buf = malloc (sz);
    memcpy (buf, p, sz);
  }
  ws_send_frame (server, client, opcode, buf, sz);
  free (buf);

  return 0;
#endif

    if (sz <= 0)
        return -1;

    switch (opcode) {
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BIN:
            return ws_send_frame (server, client, opcode, p, sz);

        case WS_OPCODE_PING:
            return ws_send_frame (server, client, WS_OPCODE_PING, NULL, 0);

        case WS_OPCODE_CLOSE:
            return ws_send_frame (server, client, WS_OPCODE_CLOSE, NULL, 0);

        default:
            ULOG_WARN ("Unknown WebSocket opcode: %d\n", opcode);
            return -1;
    }

    return 0;
}

/* Send a data message to the given client 
 *
 * On success, 0 is returned. */
int
ws_send_packet_safe (WSServer *server, WSClient *client,
        WSOpcode opcode, const char *p, int sz)
{
    int retv;
    char *buf = NULL;

    if (sz <= 0)
        return -1;

    switch (opcode) {
        case WS_OPCODE_TEXT:
            if ((buf = sanitize_utf8 (p, sz)) == NULL)
                return -1;
            break;

        case WS_OPCODE_BIN:
            return ws_send_frame (server, client, WS_OPCODE_BIN, p, sz);

        case WS_OPCODE_PING:
            return ws_send_frame (server, client, WS_OPCODE_PING, NULL, 0);

        case WS_OPCODE_CLOSE:
            return ws_send_frame (server, client, WS_OPCODE_CLOSE, NULL, 0);

        default:
            ULOG_WARN ("Unknown WebSocket opcode: %d\n", opcode);
            return -1;
    }

    retv = ws_send_frame (server, client, opcode, buf, sz);
    if (buf) {
        free (buf);
    }

    return retv;
}

/* Read a websocket frame's header.
 *
 * On success, the number of bytesr read is returned. */
static int
ws_read_header (WSServer * server, WSClient * client, WSFrame * frm, int pos, int need)
{
  char *buf = frm->buf;
  int bytes = 0;

  /* read the first 2 bytes for basic frame info */
  if ((bytes = read_socket (server, client, buf + pos, need)) < 1) {
    if (client->status & WS_CLOSE)
      ws_error (server, client, WS_CLOSE_UNEXPECTED, "Unable to read header");
    return bytes;
  }
  frm->buflen += bytes;
  frm->buf[frm->buflen] = '\0'; /* null-terminate */

  return bytes;
}

/* Read a websocket frame's payload.
 *
 * On success, the number of bytesr read is returned. */
static int
ws_read_payload (WSServer * server, WSClient * client, WSMessage * msg, int pos, int need)
{
  char *buf = msg->payload;
  int bytes = 0;

  /* read the first 2 bytes for basic frame info */
  if ((bytes = read_socket (server, client, buf + pos, need)) < 1) {
    if (client->status & WS_CLOSE)
      ws_error (server, client, WS_CLOSE_UNEXPECTED, "Unable to read payload");
    return bytes;
  }
  msg->buflen += bytes;
  msg->payloadsz += bytes;

  return bytes;
}

/* Set the basic frame headers on a frame structure.
 *
 * On success, 0 is returned. */
static int
ws_set_front_header_fields (WSClient * client)
{
  WSFrame **frm = &client->frame;
  char *buf = (*frm)->buf;

  (*frm)->fin = WS_FRM_FIN (*(buf));
  (*frm)->masking = WS_FRM_MASK (*(buf + 1));
  (*frm)->opcode = WS_FRM_OPCODE (*(buf));
  (*frm)->res = WS_FRM_R1 (*(buf)) || WS_FRM_R2 (*(buf)) || WS_FRM_R3 (*(buf));

  /* should be masked and can't be using RESVd  bits */
  if (!(*frm)->masking || (*frm)->res)
    return ws_set_status (client, WS_ERR | WS_CLOSE, 1);

  return 0;
}

/* Unmask the payload given the current frame's masking key. */
static void
ws_unmask_payload (char *buf, int len, int offset, unsigned char mask[])
{
  int i, j = 0;

  /* unmask data */
  for (i = offset; i < len; ++i, ++j) {
    buf[i] ^= mask[j % 4];
  }
}

/* Close a websocket connection. */
static int
ws_handle_close (WSServer * server, WSClient * client)
{
  client->status = WS_ERR | WS_CLOSE;
  return ws_send_frame (server, client, WS_OPCODE_CLOSE, NULL, 0);
}

/* Handle a websocket error.
 *
 * On success, the number of bytes sent is returned. */
static int
ws_handle_err (WSServer * server, WSClient * client, unsigned short code, WSStatus status,
               const char *m)
{
  client->status = status;
  return ws_error (server, client, code, m);
}

/* Handle a websocket pong. */
static void
ws_handle_pong (WSServer * server, WSClient * client)
{
  WSFrame **frm = &client->frame;

  if (!(*frm)->fin) {
    ws_handle_err (server, client, WS_CLOSE_PROTO_ERR, WS_ERR | WS_CLOSE, NULL);
    return;
  }
  ws_free_message (client);
}

/* Handle a websocket ping from the client and it attempts to send
 * back a pong as soon as possible. */
static void
ws_handle_ping (WSServer * server, WSClient * client)
{
  WSFrame **frm = &client->frame;
  WSMessage **msg = &client->message;
  char *buf = NULL, *tmp = NULL;
  int pos = 0, len = (*frm)->payloadlen, newlen = 0;

  /* RFC states that Control frames themselves MUST NOT be
   * fragmented. */
  if (!(*frm)->fin) {
    ws_handle_err (server, client, WS_CLOSE_PROTO_ERR, WS_ERR | WS_CLOSE, NULL);
    return;
  }

  /* Control frames are only allowed to have payload up to and
   * including 125 octets */
  if ((*frm)->payloadlen > 125) {
    ws_handle_err (server, client, WS_CLOSE_PROTO_ERR, WS_ERR | WS_CLOSE, NULL);
    return;
  }

  /* No payload from ping */
  if (len == 0) {
    ws_send_frame (server, client, WS_OPCODE_PONG, NULL, 0);
    return;
  }

  /* Copy the ping payload */
  pos = (*msg)->payloadsz - len;
  buf = calloc (len, sizeof (char));
  memcpy (buf, (*msg)->payload + pos, len);

  /* Unmask it */
  ws_unmask_payload (buf, len, 0, (*frm)->mask);

  /* Resize the current payload (keep an eye on this realloc) */
  newlen = (*msg)->payloadsz - len;
  tmp = realloc ((*msg)->payload, newlen);
  if (tmp == NULL && newlen > 0) {
    free ((*msg)->payload);
    free (buf);

    (*msg)->payload = NULL;
    client->status = WS_ERR | WS_CLOSE;

    update_upper_entity_stats (client->entity,
          client->sockqueue ? client->sockqueue->qlen : 0, 0);
    return;
  }

  (*msg)->payload = tmp;
  (*msg)->payloadsz -= len;

  ws_send_frame (server, client, WS_OPCODE_PONG, buf, len);

  (*msg)->buflen = 0;   /* done with the current frame's payload */
  /* Control frame injected in the middle of a fragmented message. */
  if (!(*msg)->fragmented) {
    ws_free_message (client);
  }
  free (buf);
}

/* Ensure we have valid UTF-8 text payload.
 *
 * On error, or if the message is invalid, 1 is returned.
 * On success, or if the message is valid, 0 is returned. */
int
ws_validate_string (const char *str, int len)
{
  uint32_t state = UTF8_VALID;

  if (verify_utf8 (&state, str, len) == UTF8_INVAL) {
    ULOG_NOTE ("Invalid UTF8 data!\n");
    return 1;
  }
  if (state != UTF8_VALID) {
    ULOG_NOTE ("Invalid UTF8 data!\n");
    return 1;
  }

  return 0;
}

/* It handles a text or binary message frame from the client. */
static void
ws_handle_text_bin (WSServer * server, WSClient * client)
{
  WSFrame **frm = &client->frame;
  WSMessage **msg = &client->message;
  int offset = (*msg)->mask_offset;

  /* All data frames after the initial data frame must have opcode 0 */
  if ((*msg)->fragmented && (*frm)->opcode != WS_OPCODE_CONTINUATION) {
    client->status = WS_ERR | WS_CLOSE;
    return;
  }

  /* RFC states that there is a new masking key per frame, therefore,
   * time to unmask... */
  ws_unmask_payload ((*msg)->payload, (*msg)->payloadsz, offset, (*frm)->mask);
  /* Done with the current frame's payload */
  (*msg)->buflen = 0;
  /* Reading a fragmented frame */
  (*msg)->fragmented = 1;

  if (!(*frm)->fin)
    return;

  /* validate text data encoded as UTF-8 */
  if ((*msg)->opcode == WS_OPCODE_TEXT) {
    if (ws_validate_string ((*msg)->payload, (*msg)->payloadsz) != 0) {
      ws_handle_err (server, client, WS_CLOSE_INVALID_UTF8, WS_ERR | WS_CLOSE, NULL);
      return;
    }
  }

  if ((*msg)->opcode != WS_OPCODE_CONTINUATION && server->on_packet) {
    server->on_packet (server, (SockClient *)client, (*msg)->payload, (*msg)->payloadsz,
            (client->message->opcode == WS_OPCODE_TEXT) ? PT_TEXT : PT_BINARY);
  }
  ws_free_message (client);
}

/* Depending on the frame opcode, then we take certain decisions. */
static void
ws_manage_payload_opcode (WSServer * server, WSClient * client)
{
  WSFrame **frm = &client->frame;
  WSMessage **msg = &client->message;

  switch ((*frm)->opcode) {
  case WS_OPCODE_CONTINUATION:
    ULOG_NOTE ("CONTINUATION\n");
    /* first frame can't be a continuation frame */
    if (!(*msg)->fragmented) {
      client->status = WS_ERR | WS_CLOSE;
      break;
    }
    ws_handle_text_bin (server, client);
    break;
  case WS_OPCODE_TEXT:
  case WS_OPCODE_BIN:
    ULOG_NOTE ("TEXT\n");
    client->message->opcode = (*frm)->opcode;
    clock_gettime (CLOCK_MONOTONIC, &client->ts);
    ws_handle_text_bin (server, client);
    break;
  case WS_OPCODE_PONG:
    ULOG_NOTE ("PONG\n");
    ws_handle_pong (server, client);
    break;
  case WS_OPCODE_PING:
    ULOG_NOTE ("PING\n");
    ws_handle_ping (server, client);
    break;
  default:
    ULOG_NOTE ("CLOSE\n");
    ws_handle_close (server, client);
  }
}

/* Set the extended payload length into the given pointer. */
static void
ws_set_extended_header_size (const char *buf, int *extended)
{
  uint64_t payloadlen = 0;
  /* determine the payload length, else read more data */
  payloadlen = WS_FRM_PAYLOAD (*(buf + 1));
  switch (payloadlen) {
  case WS_PAYLOAD_EXT16:
    *extended = 2;
    break;
  case WS_PAYLOAD_EXT64:
    *extended = 8;
    break;
  }
}

/* Set the extended payload length into our frame structure. */
static void
ws_set_payloadlen (WSFrame * frm, const char *buf)
{
  uint64_t payloadlen = 0, len64;
  uint16_t len16;

  /* determine the payload length, else read more data */
  payloadlen = WS_FRM_PAYLOAD (*(buf + 1));
  switch (payloadlen) {
  case WS_PAYLOAD_EXT16:
    memcpy (&len16, (buf + 2), sizeof (uint16_t));
    frm->payloadlen = ntohs (len16);
    break;
  case WS_PAYLOAD_EXT64:
    memcpy (&len64, (buf + 2), sizeof (uint64_t));
    frm->payloadlen = be64toh (len64);
    break;
  default:
    frm->payloadlen = payloadlen;
  }
}

/* Set the masking key into our frame structure. */
static void
ws_set_masking_key (WSFrame * frm, const char *buf)
{
  uint64_t payloadlen = 0;

  /* determine the payload length, else read more data */
  payloadlen = WS_FRM_PAYLOAD (*(buf + 1));
  switch (payloadlen) {
  case WS_PAYLOAD_EXT16:
    memcpy (&frm->mask, buf + 4, sizeof (frm->mask));
    break;
  case WS_PAYLOAD_EXT64:
    memcpy (&frm->mask, buf + 10, sizeof (frm->mask));
    break;
  default:
    memcpy (&frm->mask, buf + 2, sizeof (frm->mask));
  }
}

/* Attempt to read the frame's header and set the relavant data into
 * our frame structure.
 *
 * On error, or if no data available to read, the number of bytes is
 * returned and the appropriate connection status is set.
 * On success, the number of bytes is returned. */
static int
ws_get_frm_header (WSServer * server, WSClient *client)
{
  WSFrame **frm = NULL;
  int bytes = 0, readh = 0, need = 0, offset = 0, extended = 0;

  if (client->frame == NULL)
    client->frame = new_wsframe ();

  frm = &client->frame;

  /* Read the first 2 bytes for basic frame info */
  readh = (*frm)->buflen;       /* read from header so far */
  need = 2 - readh;     /* need to read */
  if (need > 0) {
    if ((bytes = ws_read_header (server, client, (*frm), readh, need)) < 1)
      return bytes;
    if (bytes != need)
      return ws_set_status (client, WS_READING, bytes);
  }
  offset += 2;

  if (ws_set_front_header_fields (client) != 0)
    return bytes;

  ws_set_extended_header_size ((*frm)->buf, &extended);
  /* read the extended header */
  readh = (*frm)->buflen;       /* read from header so far */
  need = (extended + offset) - readh;   /* read from header field so far */
  if (need > 0) {
    if ((bytes = ws_read_header (server, client, (*frm), readh, need)) < 1)
      return bytes;
    if (bytes != need)
      return ws_set_status (client, WS_READING, bytes);
  }
  offset += extended;

  /* read the masking key */
  readh = (*frm)->buflen;       /* read from header so far */
  need = (4 + offset) - readh;
  if (need > 0) {
    if ((bytes = ws_read_header (server, client, (*frm), readh, need)) < 1)
      return bytes;
    if (bytes != need)
      return ws_set_status (client, WS_READING, bytes);
  }
  offset += 4;

  ws_set_payloadlen ((*frm), (*frm)->buf);
  ws_set_masking_key ((*frm), (*frm)->buf);

  if ((*frm)->payloadlen > server->config->max_frm_size) {
    ws_error (server, client, WS_CLOSE_TOO_LARGE, "Frame is too big");
    return ws_set_status (client, WS_ERR | WS_CLOSE, bytes);
  }

  (*frm)->buflen = 0;
  (*frm)->reading = 0;
  (*frm)->payload_offset = offset;

  return ws_set_status (client, WS_OK, bytes);
}

/* Attempt to realloc the message payload.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
ws_realloc_frm_payload (WSClient * client, WSFrame * frm, WSMessage * msg)
{
  char *tmp = NULL;
  uint64_t newlen = 0;

  newlen = msg->payloadsz + frm->payloadlen;
  /* check the maximal size of the message body here. */
  if (newlen >= PCRDR_MAX_INMEM_PAYLOAD_SIZE) {
    free (msg->payload);
    msg->payload = NULL;
    goto failed;
  }

  tmp = realloc (msg->payload, newlen);
  if (tmp == NULL && newlen > 0) {
    free (msg->payload);
    msg->payload = NULL;
    goto failed;
  }

  msg->payload = tmp;
  update_upper_entity_stats (client->entity,
          client->sockqueue ? client->sockqueue->qlen : 0, newlen);
  return 0;

failed:
    update_upper_entity_stats (client->entity,
          client->sockqueue ? client->sockqueue->qlen : 0, 0);
    return 1;
}

/* Attempt to read the frame's payload and set the relavant data into
 * our message structure.
 *
 * On error, or if no data available to read, the number of bytes is
 * returned and the appropriate connection status is set.
 * On success, the number of bytes is returned. */
static int
ws_get_frm_payload (WSServer * server, WSClient * client)
{
  WSFrame **frm = NULL;
  WSMessage **msg = NULL;
  int bytes = 0, readh = 0, need = 0;

  if (client->message == NULL)
    client->message = new_wsmessage ();

  frm = &client->frame;
  msg = &client->message;

  /* message within the same frame */
  if ((*msg)->payload == NULL && (*frm)->payloadlen) {
    size_t sz = (*frm)->payloadlen;
    (*msg)->payload = calloc (sz, sizeof (char));

    update_upper_entity_stats (client->entity,
              client->sockqueue ? client->sockqueue->qlen : 0, sz);
  }
  /* handle a new frame */
  else if ((*msg)->buflen == 0 && (*frm)->payloadlen) {
    if (ws_realloc_frm_payload (client, (*frm), (*msg)) == 1)
      return ws_set_status (client, WS_ERR | WS_CLOSE, 0);
  }

  readh = (*msg)->buflen;       /* read from so far */
  need = (*frm)->payloadlen - readh;    /* need to read */
  if (need > 0) {
    if ((bytes = ws_read_payload (server, client, (*msg), (*msg)->payloadsz, need)) < 0)
      return bytes;
    if (bytes != need)
      return ws_set_status (client, WS_READING, bytes);
  }

  (*msg)->mask_offset = (*msg)->payloadsz - (*msg)->buflen;

  ws_manage_payload_opcode (server, client);
  ws_free_frame (client);

  return bytes;
}

/* Determine if we need to read a frame's header or its payload.
 *
 * On success, the number of bytes is returned. */
static int
ws_get_message (WSServer * server, WSClient * client)
{
  int bytes = 0;
  if ((client->frame == NULL) || (client->frame->reading))
    if ((bytes = ws_get_frm_header (server, client)) < 1 || client->frame->reading)
      return bytes;
  return ws_get_frm_payload (server, client);
}

/* Determine if we need to read an HTTP request or a websocket frame.
 *
 * On success, the number of bytes is returned. */
static int
read_client_data (WSServer * server, WSClient * client)
{
  int bytes = 0;

  /* Handshake */
  if ((!(client->headers) || (client->headers->reading))) {

      bytes = ws_get_handshake (server, client);
      if (!(client->status & WS_CLOSE) && server->on_accepted) {
          int ret_code;
          ret_code = server->on_accepted (server, (SockClient *)client);
          if (ret_code != PCRDR_SC_OK) {
              ULOG_WARN ("Internal error after accepted this WebSocket client (%d): %d\n",
                      client->fd, ret_code);

              server->on_error (server, (SockClient *)client, ret_code);
              ws_set_status (client, WS_READING, bytes);
          }

          ULOG_NOTE ("Accepted after handshake: %d %s\n", client->fd, client->remote_ip);
      }
  }
  /* Message */
  else
    bytes = ws_get_message (server, client);

  return bytes;
}

/* Handle a tcp close connection. */
void
ws_cleanup_client (WSServer * server, WSClient * client)
{
#if HAVE(LIBSSL)
  if (client->ssl)
    shutdown_ssl (client);
#endif

  shutdown (client->fd, SHUT_RDWR);
  /* upon close, call on_close() callback */
  if (server->on_close)
    (*server->on_close) (server, (SockClient *)client);

  /* do access logging */
  gettimeofday (&client->end_proc, NULL);
  if (server->config->accesslog)
    access_log (client, 200);

  /* errored out while parsing a frame or a message */
  if (client->status & WS_ERR) {
    ws_clear_queue (client);
    ws_free_frame (client);
    ws_free_message (client);
  }

  server->closing = 0;
  ws_close (client);

#if HAVE(LIBSSL)
  if (client->ssl)
    SSL_free (client->ssl);
  client->ssl = NULL;
#endif

  server->nr_clients--;
  ULOG_NOTE ("Active: %d\n", server->nr_clients);
}

/* Handle a tcp read close connection. */
static void
handle_ws_read_close (WSServer * server, WSClient * client)
{
  if (client->status & WS_SENDING) {
    server->closing = 1;
    return;
  }
  ws_cleanup_client (server, client);
}

/* Handle a new socket connection. */
WSClient*
ws_handle_accept (WSServer * server, int listener)
{
  WSClient *client;

  client = accept_client (listener/*, &server->colist*/);
  if (client == NULL)
    return NULL;

  server->nr_clients++;
  if (server->nr_clients > MAX_CLIENTS_EACH) {
    ULOG_WARN ("Too busy: %d %s.\n", client->fd, client->remote_ip);

    http_error (server, client, WS_TOO_BUSY_STR);
    handle_ws_read_close (server, client);
    return NULL;
  }

#if HAVE(LIBSSL)
  /* set flag to do TLS handshake */
  if (server->config->use_ssl)
    client->sslstatus |= WS_TLS_ACCEPTING;
#endif

  return client;
}

/* Handle a tcp read:
  0: ok;
  <0: socket closed
  >0: socket other error 
*/
int
ws_handle_reads (WSServer * server, WSClient * client)
{
#if HAVE(LIBSSL)
  if (handle_ssl_pending_rw (server, client) == 0)
    return 1;
#endif

  /* *INDENT-OFF* */
  client->start_proc = client->end_proc = (struct timeval) {0};
  /* *INDENT-ON* */
  gettimeofday (&client->start_proc, NULL);
  read_client_data (server, client);
  /* An error ocurred while reading data or connection closed */
  if ((client->status & WS_CLOSE)) {
    handle_ws_read_close (server, client);
    return -1;
  }

  return 0;
}

/* Handle a tcp write close connection. */
static void
handle_write_close (WSServer * server, WSClient * client)
{
  ws_cleanup_client (server, client);
}

/* Handle a tcp write.
  0: ok;
  <0: socket closed
  >0: socket other error 
*/
int
ws_handle_writes (WSServer * server, WSClient * client)
{
#if HAVE(LIBSSL)
  if (handle_ssl_pending_rw (server, client) == 0)
    return 1;
#endif

  ws_respond (server, client, NULL, 0); /* buffered data */
  /* done sending data */
  if (client->sockqueue == NULL)
    client->status &= ~WS_SENDING;

  /* An error ocurred while sending data or while reading data but still
   * waiting from the last send() from the server to the client.  e.g.,
   * sending status code */
  if ((client->status & WS_CLOSE) && !(client->status & WS_SENDING)) {
    handle_write_close (server, client);
    return -1;
  }

  return 0;
}

/* Pack the given value into a network byte order.
 *
 * On success, the number size of uint32_t is returned. */
size_t
pack_uint32 (void *buf, uint32_t val, int convert)
{
  uint32_t v32;

  if (convert)
    v32 = htonl (val);
  else
    v32 = val;
  memcpy (buf, &v32, sizeof (uint32_t));

  return sizeof (uint32_t);
}

/* Unpack the given value into a host byte order.
 *
 * On success, the number size of uint32_t is returned. */
size_t
unpack_uint32 (const void *buf, uint32_t * val, int convert)
{
  uint32_t v32 = 0;
  memcpy (&v32, buf, sizeof (uint32_t));
  if (convert)
    *val = ntohl (v32);
  else
    *val = v32;

  return sizeof (uint32_t);
}

/* Creates an endpoint for communication and start listening for
 * connections on a socket */
int
ws_listen (WSServer* server)
{
  int listener = -1, ov = 1;
  struct addrinfo hints, *ai;

  /* get a socket and bind it */
  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  /*hints.ai_flags = AI_PASSIVE; */
  if (getaddrinfo (server->config->addr, server->config->port, &hints, &ai) != 0)
    ULOG_ERR ("Unable to set server: %s.", gai_strerror (errno));

  /* Create a TCP socket.  */
  listener = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (listener < 0) {
    ULOG_ERR ("Unable to create socket: %s.", strerror (errno));
    goto error;
  }

  fcntl (listener, F_SETFD, FD_CLOEXEC);

  /* Options */
  if (setsockopt (listener, SOL_SOCKET, SO_REUSEADDR, &ov, sizeof (ov)) == -1) {
    ULOG_ERR ("Unable to set setsockopt: %s.", strerror (errno));
    goto close_error;
  }

  /* Bind the socket to the address. */
  if (bind (listener, ai->ai_addr, ai->ai_addrlen) != 0) {
    ULOG_ERR ("Unable to set bind: %s.", strerror (errno));
    goto close_error;
  }
  freeaddrinfo (ai);

  /* Tell the socket to accept connections. */
  if (listen (listener, SOMAXCONN) == -1) {
    ULOG_ERR ("Unable to listen: %s.", strerror (errno));
    goto close_error;
  }

  return listener;

close_error:
  close (listener);

error:
  return -1;
}

/* Create a new websocket server context. */
WSServer *
ws_init (ServerConfig *config)
{
  WSServer *server = calloc (1, sizeof (WSServer));

  server->config = config;

  return server;
}

