/**
 *
 * /brief getdns support functions for DNS Resource Records
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

#ifndef RR_DICT_H_
#define RR_DICT_H_

#include "config.h"
#include "getdns/getdns.h"
#include "gldns/gbuffer.h"

/* rdf_end returns a pointer to the end of this rdf's data,
 * i.e. where the next rdata field will start.
 */
typedef const uint8_t *(*_getdns_rdf_end_t)(
    const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf);
/* Limit checks are already done with _getdns_rdf_end_t */
typedef getdns_return_t (*_getdns_rdf_wire2dict_t)(
    getdns_dict *dict, const uint8_t *rdf);
typedef getdns_return_t (*_getdns_rdf_wire2list_t)(
    getdns_list *list, const uint8_t *rdf);
typedef getdns_return_t (*_getdns_rdf_dict2wire_t)(
    const getdns_dict *dict, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len);
typedef getdns_return_t (*_getdns_rdf_list2wire_t)(
    const getdns_list *list, size_t index,
    uint8_t *rdata, uint8_t *rdf, size_t *rdf_len);

typedef struct _getdns_rdf_special {
	_getdns_rdf_end_t       rdf_end;
	_getdns_rdf_wire2dict_t wire2dict;
	_getdns_rdf_wire2list_t wire2list;
	_getdns_rdf_dict2wire_t dict2wire;
	_getdns_rdf_list2wire_t list2wire;
} _getdns_rdf_special;

/* draft-levine-dnsextlang'ish type rr and rdata definitions */

#define GETDNS_RDF_INTEGER    0x010000
#define GETDNS_RDF_BINDATA    0x020000
#define GETDNS_RDF_DNAME      0x040000
#define GETDNS_RDF_COMPRESSED 0x080000
#define GETDNS_RDF_REPEAT     0x100000

#define GETDNS_RDF_FIXEDSZ    0x0000FF
#define GETDNS_RDF_LEN_VAL    0x00FF00

typedef enum _getdns_rdf_wf_type {
	GETDNS_RDF_N        = 0x060000,     /* N      */
	GETDNS_RDF_N_A      = 0x060000,     /* N[A]   */
	GETDNS_RDF_N_C      = 0x0E0000,     /* N[C]   */
	GETDNS_RDF_N_A_C    = 0x0E0000,     /* N[A,C] */
	GETDNS_RDF_N_M      = 0x160000,     /* N[M]   */

	GETDNS_RDF_I1       = 0x010001,     /* I1     */
	GETDNS_RDF_I2       = 0x010002,     /* I2     */
	GETDNS_RDF_I4       = 0x010004,     /* I4     */

	GETDNS_RDF_T        = 0x010004,     /* T      */
	                               /* Time values using ring arithmetics
	                                * (rfc1982) for TKEY['inception'],
	                                * TKEY['expiration'],
	                                * RRSIG['inception'] and
	                                * RRSIG['expiration']
	                                */
	GETDNS_RDF_T6       = 0x020006,     /* T6     */
	                               /* Absolute time values (since epoch)
					* for TSIG['time_signed']
					*/

	GETDNS_RDF_A        = 0x020004,     /* A      */
	GETDNS_RDF_AA       = 0x020008,     /* AA     */
	GETDNS_RDF_AAAA     = 0x020010,     /* AAAA   */

	GETDNS_RDF_S        = 0x020100,     /* S      */
	GETDNS_RDF_S_L      = 0x020000,     /* S[L]   */
	GETDNS_RDF_S_M      = 0x120100,     /* S[M]   */

	GETDNS_RDF_B        = 0x020000,     /* B      */
	GETDNS_RDF_B_C      = 0x020100,     /* B[C]   */

	GETDNS_RDF_B32_C    = 0x020100,     /* B32[C] */

	GETDNS_RDF_X        = 0x020000,     /* X      */
	GETDNS_RDF_X_C      = 0x020100,     /* X[C]   */
	                               /* for NSEC3['salt'] and
	                                * NSEC3PARAM['salt'].
	                                */
	GETDNS_RDF_X_S      = 0x020200,     /* X[S]   */
	                               /* for OPT['option_data'],
	                                *    TKEY['key_data'],
	                                *    TKEY['other_data'],
	                                *    TSIG['mac'] and
	                                *    TSIG['other_data']
					* Although those do not have an
					* official presentation format.
	                                */
	GETDNS_RDF_X6       = 0x020006,
	GETDNS_RDF_X8       = 0x020008,

	GETDNS_RDF_R        = 0x100000, /* Repeat */

	GETDNS_RDF_SPECIAL  = 0x800000,
} _getdns_rdf_type;

typedef struct _getdns_rdata_def {
	const char          *name;
	_getdns_rdf_type     type;
	_getdns_rdf_special *special;
} _getdns_rdata_def;

typedef struct _getdns_rr_def {
	const char              *name;
	const _getdns_rdata_def *rdata;
	size_t                   n_rdata_fields;
} _getdns_rr_def;

const _getdns_rr_def *_getdns_rr_def_lookup(uint16_t rr_type);

getdns_return_t _getdns_rr_dict2wire(
    const getdns_dict *rr_dict, gldns_buffer *buf);

const char *_getdns_rr_type_name(int rr_type);

#endif

/* rrs.h */
