/**
 *
 * /brief functions for Public Key Pinning
 *
 */

/*
 * Copyright (c) 2015, Daniel Kahn Gillmor
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

/**
 * getdns Public Key Pinning
 * 
 * a public key pinset is a list of dicts.  each dict should have a
 * "digest" and a "value".
 * 
 * "digest": a string indicating the type of digest. at the moment, we
 *           only support a "digest" of "sha256".
 * 
 * "value": a binary representation of the digest provided.
 * 
 * given a such a pinset, we should be able to validate a chain
 * properly according to section 2.6 of RFC 7469.
 */
#include "config.h"
#include "debug.h"
#include <getdns/getdns.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <string.h>
#include "context.h"
#include "util-internal.h"

#include "pubkey-pinning-internal.h"

/* we only support sha256 at the moment.  adding support for another
   digest is more complex than just adding another entry here. in
   particular, you'll probably need a match for a particular cert
   against all supported algorithms.  better to wait on doing that
   until it is a better-understood problem (i.e. wait until hpkp is
   updated and follow the guidance in rfc7469bis)
*/

/* b64 turns every 3 octets (or fraction thereof) into 4 octets */
#define B64_ENCODED_SHA256_LENGTH (((SHA256_DIGEST_LENGTH + 2)/3)  * 4)
getdns_return_t _getdns_decode_base64(const char* str, uint8_t* res, size_t res_size)
{
	BIO *bio = NULL;
	char inbuf[B64_ENCODED_SHA256_LENGTH + 1];
	getdns_return_t ret = GETDNS_RETURN_GOOD;

	/* openssl needs a trailing newline to base64 decode */
	memcpy(inbuf, str, B64_ENCODED_SHA256_LENGTH);
	inbuf[B64_ENCODED_SHA256_LENGTH] = '\n';
	
	bio = BIO_push(BIO_new(BIO_f_base64()),
		       BIO_new_mem_buf(inbuf, sizeof(inbuf)));
	if (BIO_read(bio, res, res_size) != (int) res_size)
		ret = GETDNS_RETURN_GENERIC_ERROR;

	BIO_free_all(bio);
	return ret;
}

/* pubkey-pinning.c */
