/**
 *
 * /brief dummy prototypes for logging a la unbound
 *
 */

/*
 * Copyright (c) 2013, NLnet Labs, Verisign, Inc.
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

#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <stdlib.h>

#include "config.h"
#include "debug.h"

#ifdef DEBUGGING
#define verbose(x, ...) DEBUG_NL(__VA_ARGS__)
#define log_err(...)	DEBUG_NL(__VA_ARGS__)
#define log_info(...)	DEBUG_NL(__VA_ARGS__)
#define fatal_exit(...) do { DEBUG_NL(__VA_ARGS__); exit(EXIT_FAILURE); } while(0)
#define log_assert(x)	do { if(!(x)) fatal_exit("%s:%d: %s: assertion %s failed", \
                                                __FILE__, __LINE__, __FUNC__, #x); \
                        } while(0)
#else
#define verbose(...)	((void)0)
#define log_err(...)	((void)0)
#define log_info(...)	((void)0)
#define fatal_exit(...)	((void)0)
#define log_assert(x)	((void)0)
#endif


#endif /* UTIL_LOG_H */

