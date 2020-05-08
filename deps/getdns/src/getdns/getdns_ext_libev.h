/**
 * \file
 * \brief Public interfaces to getdns, include in your application to use getdns API.
 *
 * This source was taken from the original pseudo-implementation by
 * Paul Hoffman.
 */

/*
 * Copyright (c) 2013, NLNet Labs, Verisign, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the names of the copyright holders nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GETDNS_EXT_LIBEV_H
#define GETDNS_EXT_LIBEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <getdns/getdns.h>
#include <getdns/getdns_extra.h>
struct ev_loop;


/**
 *  \ingroup eventloops
 */
/**
 * Associate the libev ev_loop with the context, so that all
 * asynchronous requests will schedule Input/Output with it.
 * Synchronous requests will still use a default eventloop based on `poll()`.
 * Applications need to @code #include <getdns/getdns_ext_libev.h> @endcode
 * and link with libgetdns_ext_ev to use this function.
 * getdns needs to have been configured with --with-libev for this 
 * extension to be available.
 * @param context The context to configure
 * @param ev_loop The libev event loop to associate with this context.
 * @return GETDNS_RETURN_GOOD when successful
 * @return GETDNS_RETURN_BAD_CONTEXT when context is NULL
 * @return GETDNS_RETURN_INVALID_PARAMETER when ev_loop is NULL
 * @return GETDNS_RETURN_MEMORY_ERROR when memory could not be allocated
 */
getdns_return_t
getdns_extension_set_libev_loop(struct getdns_context *context,
    struct ev_loop *ev_loop);

#ifdef __cplusplus
}
#endif
#endif
