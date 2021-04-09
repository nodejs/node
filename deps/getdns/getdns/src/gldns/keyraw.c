/*
 * keyraw.c - raw key operations and conversions
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

size_t
gldns_rr_dnskey_key_size_raw(const unsigned char* keydata,
	const size_t len, int alg)
{
	/* for DSA keys */
	uint8_t t;
	
	/* for RSA keys */
	uint16_t exp;
	uint16_t int16;
	
	switch ((gldns_algorithm)alg) {
	case GLDNS_DSA:
	case GLDNS_DSA_NSEC3:
		if (len > 0) {
			t = keydata[0];
			return (64 + t*8)*8;
		} else {
			return 0;
		}
		break;
	case GLDNS_RSAMD5:
	case GLDNS_RSASHA1:
	case GLDNS_RSASHA1_NSEC3:
#ifdef USE_SHA2
	case GLDNS_RSASHA256:
	case GLDNS_RSASHA512:
#endif
		if (len > 0) {
			if (keydata[0] == 0) {
				/* big exponent */
				if (len > 3) {
					memmove(&int16, keydata + 1, 2);
					exp = ntohs(int16);
					return (len - exp - 3)*8;
				} else {
					return 0;
				}
			} else {
				exp = keydata[0];
				return (len-exp-1)*8;
			}
		} else {
			return 0;
		}
		break;
#ifdef USE_GOST
	case GLDNS_ECC_GOST:
		return 512;
#endif
#ifdef USE_ECDSA
        case GLDNS_ECDSAP256SHA256:
                return 256;
        case GLDNS_ECDSAP384SHA384:
                return 384;
#endif
#ifdef USE_ED25519
	case GLDNS_ED25519:
		return 256;
#endif
#ifdef USE_ED448
	case GLDNS_ED448:
		return 456;
#endif
	default:
		return 0;
	}
}

uint16_t gldns_calc_keytag_raw(const uint8_t* key, size_t keysize)
{
	if(keysize < 4) {
		return 0;
	}
	/* look at the algorithm field, copied from 2535bis */
	if (key[3] == GLDNS_RSAMD5) {
		uint16_t ac16 = 0;
		if (keysize > 4) {
			memmove(&ac16, key + keysize - 3, 2);
		}
		ac16 = ntohs(ac16);
		return (uint16_t) ac16;
	} else {
		size_t i;
		uint32_t ac32 = 0;
		for (i = 0; i < keysize; ++i) {
			ac32 += (i & 1) ? key[i] : key[i] << 8;
		}
		ac32 += (ac32 >> 16) & 0xFFFF;
		return (uint16_t) (ac32 & 0xFFFF);
	}
}
