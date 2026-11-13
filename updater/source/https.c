/*
*   This file is part of a modified version of Luma3DS (boot.firm_Customize-Model updater)
*   Copyright (C) 2026 ELVES
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "bearssl.h"
#include "https.h"
#include "trust_anchors.h"

#ifdef __3DS__
#include <malloc.h>
#include <3ds.h>
#else
#include <fcntl.h>
#include <time.h>
#endif

#define HTTPS_MAX_REDIRECTS 5
#define HTTPS_TIMEOUT_MS    20000
#define HTTPS_HOST_SIZE     128
#define HTTPS_PATH_SIZE     1024
#define HTTPS_LINE_SIZE     1024    // これより長いヘッダーの行は読み捨てる（Location はこれに収まる）
#define HTTPS_USER_AGENT    "boot.firm_Customize-Model-Updater/" APP_VERSION

#ifndef APP_VERSION
#define APP_VERSION "test"
#endif

typedef struct HttpsConn
{
    int                     fd;
    br_ssl_client_context   sc;
    br_x509_minimal_context xc;
    br_sslio_context        ioc;
    unsigned char           iobuf[BR_SSL_BUFSIZE_BIDI];
    unsigned char           rbuf[2048];     // 応答を 1 行ずつ読むための受け皿
    size_t                  rpos, rlen;
} HttpsConn;

typedef struct HttpsUrl
{
    char    host[HTTPS_HOST_SIZE];
    char    path[HTTPS_PATH_SIZE];
    int     port;
} HttpsUrl;

// ---------------------------------------------------------------- 3DS と PC で違うところ

#ifdef __3DS__
#define SOC_BUFFER_SIZE 0x100000
static u32 *g_socBuffer;

bool Https_Init(void)
{
    g_socBuffer = (u32 *)memalign(0x1000, SOC_BUFFER_SIZE);
    if (g_socBuffer == NULL)
        return false;
    if (R_FAILED(socInit(g_socBuffer, SOC_BUFFER_SIZE)))
    {
        free(g_socBuffer);
        g_socBuffer = NULL;
        return false;
    }
    return true;
}

void Https_Exit(void)
{
    if (g_socBuffer == NULL)
        return;
    socExit();
    free(g_socBuffer);
    g_socBuffer = NULL;
}

// TLS の乱数の種（ps:ps の乱数に時刻を混ぜる）
static void Https_Seed(unsigned char *seed, size_t size)
{
    u64 tick = svcGetSystemTick();

    memset(seed, 0, size);
    memcpy(seed, &tick, sizeof(tick));
    if (R_SUCCEEDED(psInit()))
    {
        PS_GenerateRandomBytes(seed + sizeof(tick), size - sizeof(tick));
        psExit();
    }
}

// 証明書の期限を確かめるための今の日時（0 年 1 月 1 日からの日数と、その日の秒数）。
// osGetTime は 1900 年からのミリ秒（本体の時計）
static void Https_Now(uint32_t *days, uint32_t *seconds)
{
    u64 ms = osGetTime();

    *days = (uint32_t)(ms / 86400000ULL) + 693961;
    *seconds = (uint32_t)(ms / 1000ULL % 86400ULL);
}
#else
bool Https_Init(void) { return true; }
void Https_Exit(void) { }

static void Https_Seed(unsigned char *seed, size_t size)
{
    int fd = open("/dev/urandom", O_RDONLY);

    memset(seed, 0, size);
    if (fd >= 0)
    {
        if (read(fd, seed, size) < 0)
            seed[0] ^= 1;
        close(fd);
    }
}

static void Https_Now(uint32_t *days, uint32_t *seconds)
{
    time_t t = time(NULL);

    *days = (uint32_t)(t / 86400) + 719528;
    *seconds = (uint32_t)(t % 86400);
}
#endif

// ---------------------------------------------------------------- 接続

static bool Https_ParseUrl(const char *url, HttpsUrl *out)
{
    static const char scheme[] = "https://";
    const char *p = url + sizeof(scheme) - 1, *slash, *colon;
    size_t hostLen;

    if (strncmp(url, scheme, sizeof(scheme) - 1) != 0)
        return false;

    slash = strchr(p, '/');
    if (slash == NULL)
        slash = p + strlen(p);
    colon = memchr(p, ':', (size_t)(slash - p));

    hostLen = (size_t)((colon != NULL ? colon : slash) - p);
    if (hostLen == 0 || hostLen >= sizeof(out->host))
        return false;
    memcpy(out->host, p, hostLen);
    out->host[hostLen] = 0;

    out->port = colon != NULL ? atoi(colon + 1) : 443;
    if (out->port <= 0 || out->port > 65535)
        return false;

    if (*slash == 0)
        strcpy(out->path, "/");
    else if (strlen(slash) < sizeof(out->path))
        strcpy(out->path, slash);
    else
        return false;
    return true;
}

static int Https_LowRead(void *ctx, unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    struct pollfd pfd = { fd, POLLIN, 0 };
    ssize_t n;

    if (poll(&pfd, 1, HTTPS_TIMEOUT_MS) <= 0)
        return -1;
    n = recv(fd, buf, len, 0);
    return n > 0 ? (int)n : -1;
}

static int Https_LowWrite(void *ctx, const unsigned char *buf, size_t len)
{
    ssize_t n = send(*(int *)ctx, buf, len, 0);
    return n > 0 ? (int)n : -1;
}

static HttpsResult Https_Connect(HttpsConn *c, const HttpsUrl *u, int *detail)
{
    struct hostent      *he;
    struct sockaddr_in  addr;
    unsigned char       seed[48];
    uint32_t            days, seconds;

    he = gethostbyname(u->host);
    if (he == NULL || he->h_addr_list[0] == NULL)
        return HTTPS_ERR_DNS;

    c->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->fd < 0)
        return HTTPS_ERR_CONNECT;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)u->port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
    if (connect(c->fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return HTTPS_ERR_CONNECT;

    br_ssl_client_init_full(&c->sc, &c->xc, g_trustAnchors, g_trustAnchorCount);
    Https_Now(&days, &seconds);
    br_x509_minimal_set_time(&c->xc, days, seconds);
    br_ssl_engine_set_buffer(&c->sc.eng, c->iobuf, sizeof(c->iobuf), 1);
    Https_Seed(seed, sizeof(seed));
    br_ssl_engine_inject_entropy(&c->sc.eng, seed, sizeof(seed));

    if (!br_ssl_client_reset(&c->sc, u->host, 0))
    {
        *detail = br_ssl_engine_last_error(&c->sc.eng);
        return HTTPS_ERR_TLS;
    }

    br_sslio_init(&c->ioc, &c->sc.eng, Https_LowRead, &c->fd, Https_LowWrite, &c->fd);
    c->rpos = c->rlen = 0;
    return HTTPS_OK;
}

static void Https_Close(HttpsConn *c)
{
    if (c->fd >= 0)
        close(c->fd);
    c->fd = -1;
}

// TLS の失敗か、相手が途中で切ったか
static HttpsResult Https_ReadError(HttpsConn *c, int *detail)
{
    int err = br_ssl_engine_last_error(&c->sc.eng);

    if (err == BR_ERR_OK || err == BR_ERR_IO)
        return HTTPS_ERR_PROTOCOL;
    *detail = err;
    return HTTPS_ERR_TLS;
}

// ---------------------------------------------------------------- 応答を読む

// 受け皿から最大 len バイトを取る。読めなければ -1
static int Https_Read(HttpsConn *c, void *dst, size_t len)
{
    if (c->rpos == c->rlen)
    {
        int n = br_sslio_read(&c->ioc, c->rbuf, sizeof(c->rbuf));
        if (n <= 0)
            return -1;
        c->rpos = 0;
        c->rlen = (size_t)n;
    }

    if (len > c->rlen - c->rpos)
        len = c->rlen - c->rpos;
    memcpy(dst, c->rbuf + c->rpos, len);
    c->rpos += len;
    return (int)len;
}

// 1 行（CRLF を除く）を line に読む。収まらない行は *overflow を立てて残りを捨てる
static bool Https_ReadLine(HttpsConn *c, char *line, size_t size, bool *overflow)
{
    size_t n = 0;
    char ch;

    *overflow = false;
    for (;;)
    {
        if (Https_Read(c, &ch, 1) != 1)
            return false;
        if (ch == '\n')
            break;
        if (n + 1 < size)
            line[n++] = ch;
        else
            *overflow = true;
    }

    if (n > 0 && line[n - 1] == '\r')
        --n;
    line[n] = 0;
    return true;
}

static bool Https_HeaderIs(const char *line, const char *name)
{
    size_t i;

    for (i = 0; name[i] != 0; ++i)
    {
        char a = line[i];
        if (a >= 'A' && a <= 'Z')
            a += 'a' - 'A';
        if (a != name[i])
            return false;
    }
    return line[i] == ':';
}

static const char *Https_HeaderValue(const char *line)
{
    const char *p = strchr(line, ':') + 1;
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

// 本文を len バイト（len < 0 なら切れるまで）sink に渡す
static HttpsResult Https_ReadBody(HttpsConn *c, int64_t len, const HttpsSink *sink, int *detail)
{
    unsigned char buf[2048];

    while (len != 0)
    {
        size_t want = len < 0 || len > (int64_t)sizeof(buf) ? sizeof(buf) : (size_t)len;
        int n = Https_Read(c, buf, want);

        if (n <= 0)
        {
            // 長さの分からない本文は、TLS の正しい終わり（close_notify）で終わったときだけ受け入れる
            if (len < 0 && br_ssl_engine_last_error(&c->sc.eng) == BR_ERR_OK)
                return HTTPS_OK;
            return Https_ReadError(c, detail);
        }
        if (!sink->write(sink->ctx, buf, (size_t)n))
            return HTTPS_ERR_ABORTED;
        if (len > 0)
            len -= n;
    }
    return HTTPS_OK;
}

static HttpsResult Https_ReadChunked(HttpsConn *c, const HttpsSink *sink, int *detail)
{
    char line[64];
    bool overflow;

    for (;;)
    {
        char *end;
        long long size;

        if (!Https_ReadLine(c, line, sizeof(line), &overflow))
            return Https_ReadError(c, detail);
        size = strtoll(line, &end, 16);
        if (end == line || size < 0)
            return HTTPS_ERR_PROTOCOL;

        if (size == 0)
        {
            // 後ろに付くヘッダー（あれば）を空の行まで読み捨てる
            do
            {
                if (!Https_ReadLine(c, line, sizeof(line), &overflow))
                    return Https_ReadError(c, detail);
            }
            while (line[0] != 0 || overflow);
            return HTTPS_OK;
        }

        HttpsResult res = Https_ReadBody(c, size, sink, detail);
        if (res != HTTPS_OK)
            return res;
        if (!Https_ReadLine(c, line, sizeof(line), &overflow) || line[0] != 0)
            return HTTPS_ERR_PROTOCOL;
    }
}

// 1 回分の GET。転送のときは *location に行き先を入れて HTTPS_OK を返す
static HttpsResult Https_GetOnce(HttpsConn *c, const HttpsUrl *u, const char *accept, const HttpsSink *sink,
                                 char *location, size_t locationSize, int *detail)
{
    char    line[HTTPS_LINE_SIZE];
    bool    overflow, chunked = false;
    int64_t length = -1;
    int     status = 0, n;
    HttpsResult res;

    location[0] = 0;

    res = Https_Connect(c, u, detail);
    if (res != HTTPS_OK)
        return res;

    n = snprintf(line, sizeof(line), "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " HTTPS_USER_AGENT "\r\n"
                 "Accept: %s\r\nConnection: close\r\n\r\n", u->path, u->host, accept != NULL ? accept : "*/*");
    if (n < 0 || (size_t)n >= sizeof(line))
        return HTTPS_ERR_URL;

    if (br_sslio_write_all(&c->ioc, line, (size_t)n) != 0 || br_sslio_flush(&c->ioc) != 0)
        return Https_ReadError(c, detail);

    // 状態の行（HTTP/1.1 200 OK）
    if (!Https_ReadLine(c, line, sizeof(line), &overflow))
        return Https_ReadError(c, detail);
    if (strncmp(line, "HTTP/1.", 7) != 0 || strchr(line, ' ') == NULL)
        return HTTPS_ERR_PROTOCOL;
    status = atoi(strchr(line, ' ') + 1);

    for (;;)
    {
        if (!Https_ReadLine(c, line, sizeof(line), &overflow))
            return Https_ReadError(c, detail);
        if (overflow)
            continue;
        if (line[0] == 0)
            break;

        if (Https_HeaderIs(line, "content-length"))
            length = strtoll(Https_HeaderValue(line), NULL, 10);
        else if (Https_HeaderIs(line, "transfer-encoding"))
            chunked = strstr(Https_HeaderValue(line), "chunked") != NULL;
        else if (Https_HeaderIs(line, "location"))
        {
            const char *v = Https_HeaderValue(line);
            if (strlen(v) < locationSize)
                strcpy(location, v);
        }
    }

    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
        return location[0] != 0 ? HTTPS_OK : HTTPS_ERR_PROTOCOL;

    location[0] = 0;
    if (status < 200 || status > 299)
    {
        *detail = status;
        return HTTPS_ERR_HTTP;
    }

    if (sink->begin != NULL && !sink->begin(sink->ctx, chunked ? -1 : length))
        return HTTPS_ERR_ABORTED;

    return chunked ? Https_ReadChunked(c, sink, detail) : Https_ReadBody(c, length, sink, detail);
}

HttpsResult Https_Get(const char *url, const char *accept, const HttpsSink *sink, int *detail)
{
    HttpsConn   *c;
    HttpsUrl    u;
    char        location[HTTPS_LINE_SIZE];
    HttpsResult res = HTTPS_ERR_REDIRECTS;

    *detail = 0;
    if (!Https_ParseUrl(url, &u))
        return HTTPS_ERR_URL;

    c = (HttpsConn *)malloc(sizeof(HttpsConn));
    if (c == NULL)
        return HTTPS_ERR_MEMORY;
    c->fd = -1;

    for (int i = 0; i <= HTTPS_MAX_REDIRECTS; ++i)
    {
        res = Https_GetOnce(c, &u, accept, sink, location, sizeof(location), detail);
        Https_Close(c);
        if (res != HTTPS_OK || location[0] == 0)
            break;

        // 行き先は https:// で始まる URL か、同じ相手の / で始まるパス
        if (location[0] == '/')
        {
            if (strlen(location) >= sizeof(u.path))
            {
                res = HTTPS_ERR_URL;
                break;
            }
            strcpy(u.path, location);
        }
        else if (!Https_ParseUrl(location, &u))
        {
            res = HTTPS_ERR_URL;
            break;
        }
        res = HTTPS_ERR_REDIRECTS;
    }

    free(c);
    return res;
}

// ---------------------------------------------------------------- メモリに受ける

static bool Https_BufferWrite(void *ctx, const void *data, size_t len)
{
    HttpsBuffer *b = (HttpsBuffer *)ctx;

    if (b->size + len > b->maxSize)
        return false;

    if (b->size + len + 1 > b->capacity)
    {
        size_t cap = b->capacity != 0 ? b->capacity : 0x4000;
        char *p;

        while (cap < b->size + len + 1)
            cap *= 2;
        p = (char *)realloc(b->data, cap);
        if (p == NULL)
            return false;
        b->data = p;
        b->capacity = cap;
    }

    memcpy(b->data + b->size, data, len);
    b->size += len;
    b->data[b->size] = 0;
    return true;
}

static bool Https_BufferBegin(void *ctx, int64_t length)
{
    HttpsBuffer *b = (HttpsBuffer *)ctx;

    if (length >= 0 && (uint64_t)length > b->maxSize)
        return false;
    // 本文が空でも data を空の文字列にする
    return Https_BufferWrite(ctx, "", 0);
}

void Https_BufferSink(HttpsSink *sink, HttpsBuffer *buf, size_t maxSize)
{
    memset(buf, 0, sizeof(*buf));
    buf->maxSize = maxSize;
    sink->begin = Https_BufferBegin;
    sink->write = Https_BufferWrite;
    sink->ctx = buf;
}

void Https_BufferFree(HttpsBuffer *buf)
{
    free(buf->data);
    memset(buf, 0, sizeof(*buf));
}
