/*
 * keyraw.c - raw key operations and conversions - OpenSSL version
 *
 * (c) NLnet Labs, 2004-2008
 *
 * See the file LICENSE for the license
 */
/**
 * \file
 * Implementation of raw DNSKEY functions (work on wire rdata).
 */

#include "config.h"
#include "gldns/keyraw.h"
#include "gldns/rrdef.h"
