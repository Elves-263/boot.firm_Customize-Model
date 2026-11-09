/*
*   This file is part of Luma3DS.
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   SPDX-License-Identifier: (MIT OR GPL-2.0-or-later)
*/

#pragma once

#include <sys/socket.h>
#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/srv.h>
#include <3ds/services/soc.h>
#include <poll.h>

#define _REENT_ONLY
#include <errno.h>

#define SYNC_ERROR ENODEV
extern bool miniSocEnabled;

Result miniSocInit(void);

void miniSocLockState(void);
void miniSocUnlockState(bool force);

Result miniSocExitDirect(void);
Result miniSocExit(void);

s32 _net_convert_error(s32 sock_retval);

int socSocket(int domain, int type, int protocol);
int socBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int socListen(int sockfd, int max_connections);
int socAccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int socConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int socPoll(struct pollfd *fds, nfds_t nfds, int timeout);
int socSetsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int socClose(int sockfd);
long socGethostid(void);

// soc:U の gethostbyname が結果を書く大きさ
#define SOC_HOSTENT_BUFFER_SIZE 0x1A88
// name の IPv4 アドレスを 1 つ *addr に入れる（ネットワークのバイト順）。outbuf は SOC_HOSTENT_BUFFER_SIZE
// バイトの作業用（大きいので呼ぶ側が用意する）。成功は 0、失敗は -1
int socGethostbyname(const char *name, u32 *addr, void *outbuf);

ssize_t socRecvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t socSendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

static inline ssize_t socRecv(int sockfd, void *buf, size_t len, int flags)
{
    return socRecvfrom(sockfd, buf, len, flags, NULL, 0);
}

static inline ssize_t socSend(int sockfd, const void *buf, size_t len, int flags)
{
    return socSendto(sockfd, buf, len, flags, NULL, 0);
}
