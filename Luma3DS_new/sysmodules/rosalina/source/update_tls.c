/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <stdbool.h>
#include <string.h>
#include "bearssl.h"
#include "update_tls.h"

// サーバーの証明書は確かめず、鍵だけを取り出す
typedef struct UpdateTlsX509
{
    const br_x509_class     *vtable;
    br_x509_decoder_context dec;
    unsigned                depth;
    unsigned                err;
} UpdateTlsX509;

typedef struct UpdateTlsMem
{
    br_ssl_client_context   sc;
    UpdateTlsX509           x509;
    br_sslio_context        ioc;
    unsigned char           iobuf[BR_SSL_BUFSIZE_MONO];
} UpdateTlsMem;

_Static_assert(sizeof(UpdateTlsMem) <= UPDATE_TLS_MEM_SIZE, "UPDATE_TLS_MEM_SIZE が小さい");

// GitHub が使う組だけを入れる（コードを小さくするため）。証明書は ECDSA と RSA のどちらでも通る
static const uint16_t g_suites[] =
{
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
};

// ---------------------------------------------------------------- 証明書（鍵だけ取り出す）

static void UpdateTlsX509_StartChain(const br_x509_class **ctx, const char *serverName)
{
    UpdateTlsX509 *x = (UpdateTlsX509 *)ctx;

    (void)serverName;
    x->depth = 0;
    x->err = BR_ERR_X509_EMPTY_CHAIN;
}

static void UpdateTlsX509_StartCert(const br_x509_class **ctx, uint32_t length)
{
    UpdateTlsX509 *x = (UpdateTlsX509 *)ctx;

    (void)length;
    if (x->depth == 0)
        br_x509_decoder_init(&x->dec, NULL, NULL);
}

static void UpdateTlsX509_Append(const br_x509_class **ctx, const unsigned char *buf, size_t len)
{
    UpdateTlsX509 *x = (UpdateTlsX509 *)ctx;

    if (x->depth == 0)
        br_x509_decoder_push(&x->dec, buf, len);
}

static void UpdateTlsX509_EndCert(const br_x509_class **ctx)
{
    UpdateTlsX509 *x = (UpdateTlsX509 *)ctx;

    if (x->depth == 0)
        x->err = br_x509_decoder_last_error(&x->dec);
    ++x->depth;
}

static unsigned UpdateTlsX509_EndChain(const br_x509_class **ctx)
{
    return ((UpdateTlsX509 *)ctx)->err;
}

static const br_x509_pkey *UpdateTlsX509_GetPkey(const br_x509_class *const *ctx, unsigned *usages)
{
    UpdateTlsX509 *x = (UpdateTlsX509 *)ctx;

    if (usages != NULL)
        *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
    return br_x509_decoder_get_pkey(&x->dec);
}

static const br_x509_class g_updateTlsX509Vtable =
{
    sizeof(UpdateTlsX509),
    UpdateTlsX509_StartChain,
    UpdateTlsX509_StartCert,
    UpdateTlsX509_Append,
    UpdateTlsX509_EndCert,
    UpdateTlsX509_EndChain,
    UpdateTlsX509_GetPkey,
};

// ---------------------------------------------------------------- HTTP

static int UpdateTls_LowRead(void *ctx, unsigned char *buf, size_t len)
{
    const UpdateTlsIo *io = (const UpdateTlsIo *)ctx;
    return io->read(io->ctx, buf, len);
}

static int UpdateTls_LowWrite(void *ctx, const unsigned char *buf, size_t len)
{
    const UpdateTlsIo *io = (const UpdateTlsIo *)ctx;
    return io->write(io->ctx, buf, len);
}

// s が prefix で始まるか（英字の大文字・小文字は区別しない）
static bool UpdateTls_StartsWithNoCase(const char *s, const char *prefix)
{
    for (; *prefix != 0; ++s, ++prefix)
    {
        char a = *s, b = *prefix;
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (a != b)
            return false;
    }
    return true;
}

static size_t UpdateTls_Append(char *dst, size_t pos, size_t size, const char *s)
{
    while (*s != 0 && pos + 1 < size)
        dst[pos++] = *s++;
    dst[pos] = 0;
    return pos;
}

// 応答のヘッダーを最後まで読み、状態番号と Location を取る。GitHub のヘッダーには長い行
// （Content-Security-Policy など）があるので、収まらない行は読み捨てる。失敗は false
static bool UpdateTls_ReadHeaders(br_sslio_context *ioc, int *status, char *location, size_t locationSize)
{
    char            line[256];
    unsigned char   buf[128];
    size_t          n = 0;
    bool            overflow = false, first = true;

    *status = 0;
    location[0] = 0;

    for (;;)
    {
        int got = br_sslio_read(ioc, buf, sizeof(buf));
        if (got <= 0)
            return false;

        for (int i = 0; i < got; ++i)
        {
            if (buf[i] != '\n')
            {
                if (n + 1 < sizeof(line))
                    line[n++] = (char)buf[i];
                else
                    overflow = true;
                continue;
            }

            if (n > 0 && line[n - 1] == '\r')
                --n;
            line[n] = 0;

            if (first)
            {
                // HTTP/1.1 302 Found
                const char *p = strchr(line, ' ');
                if (!UpdateTls_StartsWithNoCase(line, "http/") || p == NULL)
                    return false;
                for (++p; *p >= '0' && *p <= '9'; ++p)
                    *status = *status * 10 + (*p - '0');
            }
            else if (n == 0 && !overflow)
                return true;
            else if (!overflow && UpdateTls_StartsWithNoCase(line, "location:"))
            {
                const char *p = line + 9;
                while (*p == ' ' || *p == '\t')
                    ++p;
                UpdateTls_Append(location, 0, locationSize, p);
            }

            first = false;
            overflow = false;
            n = 0;
        }
    }
}

// 転送先の .../releases/tag/<版> から版の名前を取る。使える字は英数字と . _ - + だけ
static bool UpdateTls_TagFromLocation(const char *location, char *tag)
{
    static const char marker[] = "/releases/tag/";
    const char *p = strstr(location, marker);
    size_t n = 0;

    if (p == NULL)
        return false;

    for (p += sizeof(marker) - 1; *p != 0 && *p != '?' && *p != '#'; ++p)
    {
        char c = *p;
        bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  c == '.' || c == '_' || c == '-' || c == '+';
        if (!ok || n + 1 >= UPDATE_TAG_SIZE)
        {
            tag[0] = 0;
            return false;
        }
        tag[n++] = c;
    }

    tag[n] = 0;
    return n != 0;
}

int UpdateTls_GetLatestTag(void *memory, const UpdateTlsIo *io, const char *host, const char *repo,
                           const char *userAgent, const void *seed, size_t seedLen, char *tag, int *detail)
{
    UpdateTlsMem *mem = (UpdateTlsMem *)memory;
    br_ssl_engine_context *eng = &mem->sc.eng;
    char request[256], location[192];
    size_t n = 0;
    int status;

    *detail = 0;
    tag[0] = 0;

    br_ssl_client_zero(&mem->sc);
    br_ssl_engine_set_versions(eng, BR_TLS12, BR_TLS12);
    br_ssl_engine_set_suites(eng, g_suites, sizeof(g_suites) / sizeof(g_suites[0]));
    br_ssl_engine_set_hash(eng, br_sha256_ID, &br_sha256_vtable);
    br_ssl_engine_set_prf_sha256(eng, &br_tls12_sha256_prf);
    br_ssl_engine_set_default_aes_gcm(eng);
    br_ssl_engine_set_ec(eng, &br_ec_all_m31);
    br_ssl_engine_set_ecdsa(eng, &br_ecdsa_i31_vrfy_asn1);
    br_ssl_engine_set_rsavrfy(eng, &br_rsa_i31_pkcs1_vrfy);
    mem->x509.vtable = &g_updateTlsX509Vtable;
    br_ssl_engine_set_x509(eng, &mem->x509.vtable);
    br_ssl_engine_set_buffer(eng, mem->iobuf, sizeof(mem->iobuf), 0);
    br_ssl_engine_inject_entropy(eng, seed, seedLen);

    if (!br_ssl_client_reset(&mem->sc, host, 0))
    {
        *detail = br_ssl_engine_last_error(eng);
        return UPDATE_TLS_ERR_TLS;
    }

    br_sslio_init(&mem->ioc, eng, UpdateTls_LowRead, (void *)io, UpdateTls_LowWrite, (void *)io);

    n = UpdateTls_Append(request, n, sizeof(request), "HEAD /");
    n = UpdateTls_Append(request, n, sizeof(request), repo);
    n = UpdateTls_Append(request, n, sizeof(request), "/releases/latest HTTP/1.1\r\nHost: ");
    n = UpdateTls_Append(request, n, sizeof(request), host);
    n = UpdateTls_Append(request, n, sizeof(request), "\r\nUser-Agent: ");
    n = UpdateTls_Append(request, n, sizeof(request), userAgent);
    n = UpdateTls_Append(request, n, sizeof(request), "\r\nAccept: */*\r\nConnection: close\r\n\r\n");

    if (br_sslio_write_all(&mem->ioc, request, n) != 0 || br_sslio_flush(&mem->ioc) != 0 ||
        !UpdateTls_ReadHeaders(&mem->ioc, &status, location, sizeof(location)))
    {
        *detail = br_ssl_engine_last_error(eng);
        return UPDATE_TLS_ERR_TLS;
    }

    if (status < 300 || status > 399 || location[0] == 0)
    {
        *detail = status;
        return UPDATE_TLS_ERR_HTTP;
    }

    return UpdateTls_TagFromLocation(location, tag) ? UPDATE_TLS_OK : UPDATE_TLS_ERR_NOTAG;
}

// ---------------------------------------------------------------- 版の比較

// v1.4 → { 1, 4, 0, 0 }。数字で始まらなければ false
static bool UpdateVersion_Parse(const char *s, unsigned parts[4])
{
    memset(parts, 0, 4 * sizeof(unsigned));

    if (*s == 'v' || *s == 'V')
        ++s;
    if (*s < '0' || *s > '9')
        return false;

    for (unsigned i = 0; i < 4; ++i)
    {
        while (*s >= '0' && *s <= '9')
            parts[i] = parts[i] * 10 + (unsigned)(*s++ - '0');
        if (*s != '.' || s[1] < '0' || s[1] > '9')
            break;
        ++s;
    }
    return true;
}

int UpdateVersion_Compare(const char *a, const char *b)
{
    unsigned pa[4], pb[4];
    bool oka = UpdateVersion_Parse(a, pa), okb = UpdateVersion_Parse(b, pb);

    if (!oka || !okb)
        return (int)oka - (int)okb;

    for (unsigned i = 0; i < 4; ++i)
        if (pa[i] != pb[i])
            return pa[i] > pb[i] ? 1 : -1;
    return 0;
}
