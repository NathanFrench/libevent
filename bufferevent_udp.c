/*
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 * Copyright (c) 2002-2006 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "event2/event-config.h"
#include "evconfig-private.h"

#include <sys/types.h>

#ifdef EVENT__HAVE_SYS_TIME_H
#include <sys/time.h>
#endif #include < errno.h > #include < stdio.h >
#include <stdlib.h>
#include <string.h>
#ifdef EVENT__HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef EVENT__HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef EVENT__HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef EVENT__HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef EVENT__HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif

#include "event2/util.h"
#include "event2/bufferevent.h"
#include "event2/buffer.h"
#include "event2/bufferevent_struct.h"
#include "event2/bufferevent_compat.h"
#include "event2/event.h"
#include "log-internal.h"
#include "mm-internal.h"
#include "bufferevent-internal.h"
#include "util-internal.h"
#ifdef _WIN32
#include "iocp-internal.h"
#endif

#include <sys/queue.h>

struct bufferevent_udp {
    struct bufferevent _bev;
    evutil_socket_t    _sock;
};

struct bufferevent *
bufferevent_udp_socket_new(struct event_base * base, evutil_socket_t fd, int opts)
{
    struct bufferevent_udp * udp_bev;

    udp_bev = (bufferevent_udp *)calloc(1, sizeof(*udp_bev));
    udp_bev->_bev.input   = evbuffer_new();
    udp_bev->_bev.output  = evbuffer_new();
    udp_bev->_bev.ev_base = base;
    udp_bev->_sock        = fd;

    return &udp_bev->_bev;
}

#if 0
struct bufferevent_udp_data {
    struct bufferevent_udp * _bev;
    struct sockaddr_storage  _addr;
    evutil_socket_t          _sock;
    ev_socklen_t             _addrlen;
    struct evbuffer        * _buf_in;
    struct evbuffer        * _buf_out;

    TAILQ_ENTRY(bufferevent_udp_data) _pending_next;
};

struct bufferevent_udp {
    struct bufferevent_private bev;
    evutil_socket_t            _sock;
    struct event               _event;

    TAILQ_HEAD(, bufferevent_udp_data) _pending;
};


static int
be_udp_enable(struct bufferevent * bev, short events)
{
    (void)bev;
    (void)events;
    return 0;
}

static int
be_udp_disable(struct bufferevent * bev, short events)
{
    (void)bev;
    (void)events;
    return 0;
}

static void
be_udp_unlink(struct bufferevent * bev)
{
    (void)bev;
    return;
}

static void
be_udp_destruct(struct bufferevent * bev)
{
    (void)bev;
    return;
}

static int
be_udp_flush(struct bufferevent * bev, short type, enum bufferevent_flush_mode mode)
{
    (void)bev;
    (void)type;
    (void)mode;

    return 0;
}

const struct bufferevent_ops bufferevent_ops_udp = {
    "udp_socket",
    evutil_offsetof(struct bufferevent_udp,    bev.bev),
    be_udp_enable,
    be_udp_disable,
    be_udp_unlink,
    be_udp_destruct,
    bufferevent_generic_adj_existing_timeouts_,
    be_udp_flush,
    NULL                                      /* ctrl */
};

static int
be_udp_read_(struct bufferevent_udp * bev_udp)
{
    struct bufferevent         * bev      = &bev_udp->bev.bev;
    struct bufferevent_private * bev_priv = &bev_udp->bev;
    struct evbuffer            * evbuf    = bev->input;
    ssize_t                      recv_len;
    struct sockaddr_storage      addr;
    static ev_socklen_t          addrlen  = sizeof(addr);
    char                         buf[1500];


    while (1) {
        struct bufferevent_udp_data * dat;

        recv_len = recvfrom(bev_udp->_sock, buf, sizeof(buf), 0,
                            (struct sockaddr *)&addr, &addrlen);

        if (recv_len == -1) {
            return 0;
        }

        /*
         * dat           = (struct bufferevent_udp_data *)calloc(1, sizeof(*dat));
         * dat->_bev     = bev_udp;
         * dat->_sock    = bev_udp->_sock;
         * dat->_addrlen = addrlen;
         * dat->_buf_in  = bev->input;
         * dat->_buf_out = bev->output;
         *
         * memcpy(&dat->_addr, &addr, addrlen);
         */
        memcpy(&bev_priv->conn_address, &addr, addrlen);
        evbuffer_add(bev->input, buf, recv_len);

        bufferevent_trigger_nolock_(bev->input, EV_READ, 0);
    }

    return 0;
}

static void
be_udp_handler_(evutil_socket_t sock, short events, void * arg)
{
    (void)sock;
    struct bufferevent_udp * bev_udp;

    bev_udp = (struct bufferevent_udp *)arg;

    if (events & EV_READ) {
        be_udp_read_(bev_udp);
    }

    if (events & EV_WRITE) {
        be_udp_write_(bev_udp);
    }
}

static void
be_udp_outbuf_cb_(struct evbuffer * buf, const struct evbuffer_cb_info * cbinfo, void * arg)
{
/**
 * @brief
 *
 * @param base
 * @param fd
 * @param options
 *
 * @return
 */
    struct bufferevent *
    bufferevent_udp_socket_new(struct event_base * base,
                               evutil_socket_t     fd,
                               int                 options)
    {
        struct bufferevent_private * bev_priv;
        struct bufferevent_udp     * bev_udp;

        bev_udp        = (struct bufferevent_udp *)calloc(1, sizeof(*bev_udp));

        bev_priv       = &bev_udp->bev;
        bev_udp->_sock = fd;

        bufferevent_init_common_(bev_priv, base, &bufferevent_ops_udp, options);
        event_assign(&bev_udp->_event, base, fd, EV_READ | EV_PERSIST, be_udp_handler_, bev_udp);

        evbuffer_add_cb(bev_udp.bev.bev->output, be_udp_outbuf_cb_, bev_udp);

        return &bev_udp->bev.bev;
    }

#endif
