// Copyright (c) 2022 by Wind River Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_hoofs/platform/socket.hpp"
#include "iceoryx_hoofs/platform/platform_settings.hpp"
#include <unistd.h>
#include <sys/un.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

int iox_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    /* /comp/socket/0x1234 is a hardcoded path in VxWorks, convert basename to the 0x1234 format  */
    int i, sum = 0;
    char *basename = &((struct sockaddr_un*)addr)->sun_path[strlen(iox::platform::IOX_UDS_SOCKET_PATH_PREFIX)];

    /* calculate a sum from the name to make it consistent for unlinking */
    for (i = 0; i < (int)strlen(basename); i++)
        sum += basename[i];

    sprintf(&((struct sockaddr_un*)addr)->sun_path[0], "%s0x%04x", iox::platform::IOX_UDS_SOCKET_PATH_PREFIX, sum % 0xffff);
    unlink(&(((struct sockaddr_un*)addr)->sun_path[0]));

    return bind(sockfd, addr, addrlen);
}

int iox_socket(int domain, int type, int protocol)
{
    /* there is no SOCK_DGRAM in VxWorks */
    type = SOCK_SEQPACKET;
    return socket(domain, type, protocol);
}

int iox_setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen)
{
    return setsockopt(sockfd, level, optname, optval, optlen);
}

ssize_t
iox_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen)
{
    return sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t iox_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen)
{
    return recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}

int iox_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    return connect(sockfd, addr, addrlen);
}

int iox_closesocket(int sockfd)
{
    return close(sockfd);
}
