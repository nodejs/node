/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_TEST_SHIM_PACKETED_BIO_H
#define OSSL_TEST_SHIM_PACKETED_BIO_H

#include <openssl/base.h>
#include <openssl/bio.h>

// PacketedBioCreate creates a filter BIO which implements a reliable in-order
// blocking datagram socket. It internally maintains a clock and honors
// |BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT| based on it.
//
// During a |BIO_read|, the peer may signal the filter BIO to simulate a
// timeout. If |advance_clock| is true, it automatically advances the clock and
// continues reading, subject to the read deadline. Otherwise, it fails
// immediately. The caller must then call |PacketedBioAdvanceClock| before
// retrying |BIO_read|.
bssl::UniquePtr<BIO> PacketedBioCreate(bool advance_clock);

// PacketedBioGetClock returns the current time for |bio|.
timeval PacketedBioGetClock(const BIO *bio);

// PacketedBioAdvanceClock advances |bio|'s internal clock and returns true if
// there is a pending timeout. Otherwise, it returns false.
bool PacketedBioAdvanceClock(BIO *bio);


#endif  // OSSL_TEST_SHIM_PACKETED_BIO_H
