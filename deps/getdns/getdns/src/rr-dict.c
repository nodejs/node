/**
 *
 * /brief getdns support functions for DNS Resource Records
 *
 * This file contains the tables with the information needed by getdns about
 * individual RRs, such as their name and rdata fields and types.
 * This information is provided via the response dict.
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

#include "rr-dict.h"
#include "gldns/gbuffer.h"
#include "util-internal.h"
#include "types-internal.h"
#include "context.h"
#include "dict.h"

#define ALEN(a) (sizeof(a)/sizeof(a[0]))
#define UNKNOWN_RDATA NULL

static const uint8_t *
apl_n_rdf_end(const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	(void)pkt;
	return rdf < pkt_end ? rdf + 1 : NULL;
}
static getdns_return_t
apl_n_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	return getdns_dict_set_int(dict, "n", (*rdf  >> 7));
}
static getdns_return_t
apl_n_2wire(uint32_t value, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	(void)rdata; /* unused parameter */

	if (*rdf_len < 1) {
		*rdf_len = 1;
		return GETDNS_RETURN_NEED_MORE_SPACE;
	}
	*rdf_len = 1;
	*rdf = value ? 0x80 : 0x00;
	return GETDNS_RETURN_GOOD;
}
static getdns_return_t
apl_n_dict2wire(const getdns_dict *dict,
    uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	uint32_t        value;

	if ((r = getdns_dict_get_int(dict, "n", &value)))
		return r;
	else
		return apl_n_2wire(value, rdata, rdf, rdf_len);
}
static _getdns_rdf_special apl_n = {
    apl_n_rdf_end,
    apl_n_wire2dict, NULL,
    apl_n_dict2wire, NULL 
};

static const uint8_t *
apl_afdpart_rdf_end(
    const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	const uint8_t *end = rdf + (rdf[-1] & 0x7F);
	(void)(pkt);
	return end <= pkt_end ? end : NULL;
}
static getdns_return_t
apl_afdpart_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	return _getdns_dict_set_const_bindata(
	    dict, "afdpart", (rdf[-1] & 0x7F), rdf);
}
static getdns_return_t
apl_afdpart_2wire(
    const getdns_bindata *value, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	if (value->size > 0x7F)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (rdf - 1 < rdata)
		return GETDNS_RETURN_GENERIC_ERROR;

	if (*rdf_len < value->size) {
		*rdf_len = value->size;
		return GETDNS_RETURN_NEED_MORE_SPACE;
	}
	*rdf_len = value->size;

	/* Keeping first bit is safe because value->size <= 0x7F */
	rdf[-1] |= value->size;

	(void) memcpy(rdf, value->data, value->size);
	return GETDNS_RETURN_GOOD;
}
static getdns_return_t
apl_afdpart_dict2wire(
    const getdns_dict *dict, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	getdns_bindata *value;

	if ((r = getdns_dict_get_bindata(dict, "afdpart", &value)))
		return r;
	else
		return apl_afdpart_2wire(value, rdata, rdf, rdf_len);
}
static _getdns_rdf_special apl_afdpart = {
    apl_afdpart_rdf_end,
    apl_afdpart_wire2dict, NULL,
    apl_afdpart_dict2wire, NULL 
};

static const uint8_t *
ipseckey_gateway_rdf_end(
    const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	const uint8_t *end;

	if (rdf - 5 < pkt)
		return NULL;
	switch (rdf[-2]) {
	case 0:	end = rdf;
		break;
	case 1: end = rdf + 4;
		break;
	case 2: end = rdf + 16;
		break;
	case 3: for (end = rdf; end < pkt_end; end += *end + 1)
			if ((*end & 0xC0) == 0xC0)
				end  += 2;
			else if (*end & 0xC0)
				return NULL;
			else if (!*end) {
				end += 1;
				break;
			}
		break;
	default:
		return NULL;
	}
	return end <= pkt_end ? end : NULL;
}
static getdns_return_t
ipseckey_gateway_equip_const_bindata(
    const uint8_t *rdf, size_t *size, const uint8_t **data)
{
	*data = rdf;
	switch (rdf[-2]) {
	case 0:	*size = 0;
		break;
	case 1: *size = 4;
		break;
	case 2: *size = 16;
		break;
	case 3: while (*rdf)
			if ((*rdf & 0xC0) == 0xC0)
				rdf += 2;
			else if (*rdf & 0xC0)
				return GETDNS_RETURN_GENERIC_ERROR;
			else
				rdf += *rdf + 1;
		*size = rdf + 1 - *data;
		break;
	default:
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	return GETDNS_RETURN_GOOD;
}

static getdns_return_t
ipseckey_gateway_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	size_t size;
	const uint8_t *data;

	if (ipseckey_gateway_equip_const_bindata(rdf, &size, &data))
		return GETDNS_RETURN_GENERIC_ERROR;

	else if (! size)
		return GETDNS_RETURN_GOOD;
	else
		return _getdns_dict_set_const_bindata(dict, "gateway", size, data);
}
static getdns_return_t
ipseckey_gateway_2wire(
    const getdns_bindata *value, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	assert(rdf - 2 >= rdata && rdf[-2] > 0);

	switch (rdf[-2]) {
	case 1: if (!value || value->size != 4)
			return GETDNS_RETURN_INVALID_PARAMETER;
		if (*rdf_len < 4) {
			*rdf_len = 4;
			return GETDNS_RETURN_NEED_MORE_SPACE;
		}
		*rdf_len = 4;
		(void)memcpy(rdf, value->data, 4);
		return GETDNS_RETURN_GOOD;
	case 2: if (!value || value->size != 16)
			return GETDNS_RETURN_INVALID_PARAMETER;
		if (*rdf_len < 16) {
			*rdf_len = 16;
			return GETDNS_RETURN_NEED_MORE_SPACE;
		}
		*rdf_len = 16;
		(void)memcpy(rdf, value->data, 16);
		return GETDNS_RETURN_GOOD;
	case 3: if (!value || value->size == 0)
			return GETDNS_RETURN_INVALID_PARAMETER;
		/* Assume bindata is a valid dname; garbage in, garbage out */
		if (*rdf_len < value->size) {
			*rdf_len = value->size;
			return GETDNS_RETURN_NEED_MORE_SPACE;
		}
		*rdf_len = value->size;
		(void)memcpy(rdf, value->data, value->size);
		return GETDNS_RETURN_GOOD;
	default:
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	return GETDNS_RETURN_GOOD;
}
static getdns_return_t
ipseckey_gateway_dict2wire(
    const getdns_dict *dict, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	getdns_bindata *value;

	if (rdf - 2 < rdata)
		return GETDNS_RETURN_GENERIC_ERROR;

	else if (rdf[-2] == 0) {
		*rdf_len = 0;
		return GETDNS_RETURN_GOOD;
	}
	else if ((r = getdns_dict_get_bindata(dict, "gateway", &value)))
		return r;
	else
		return ipseckey_gateway_2wire(value, rdata, rdf, rdf_len);
}
static _getdns_rdf_special ipseckey_gateway = {
    ipseckey_gateway_rdf_end,
    ipseckey_gateway_wire2dict, NULL,
    ipseckey_gateway_dict2wire, NULL 
};

static const uint8_t *
hip_pk_algorithm_rdf_end(
    const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	(void)(pkt);
	return rdf + 4 > pkt_end ? NULL
	     : rdf + 4 + *rdf + gldns_read_uint16(rdf + 2) > pkt_end ? NULL
	     : rdf + 1;
}
static getdns_return_t
hip_pk_algorithm_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	return getdns_dict_set_int(dict, "pk_algorithm", rdf[1]);
}
static getdns_return_t
hip_pk_algorithm_2wire(uint32_t value, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	if (rdata != rdf)
		return GETDNS_RETURN_GENERIC_ERROR;
	if (value > 0xFF)
		return GETDNS_RETURN_INVALID_PARAMETER;
	if (*rdf_len < 4) {
		*rdf_len = 4;
		return GETDNS_RETURN_NEED_MORE_SPACE;
	}
	*rdf_len = 4;
	rdata[1] = value;
	return GETDNS_RETURN_GOOD;
}
static getdns_return_t
hip_pk_algorithm_dict2wire(
    const getdns_dict *dict,uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	uint32_t        value;

	if ((r = getdns_dict_get_int(dict, "pk_algorithm", &value)))
		return r;
	else
		return hip_pk_algorithm_2wire(value, rdata, rdf, rdf_len);
}
static _getdns_rdf_special hip_pk_algorithm = {
    hip_pk_algorithm_rdf_end,
    hip_pk_algorithm_wire2dict, NULL,
    hip_pk_algorithm_dict2wire, NULL
};

static const uint8_t *
hip_hit_rdf_end(const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	(void)(pkt);
	return rdf + 3 > pkt_end ? NULL
	     : rdf + 3 + rdf[-1] + gldns_read_uint16(rdf + 1) > pkt_end ? NULL
	     : rdf + 1;
}
static getdns_return_t
hip_hit_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	return _getdns_dict_set_const_bindata(dict, "hit", rdf[-1], rdf + 3);
}
static getdns_return_t
hip_hit_2wire(
    const getdns_bindata *value, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	if (rdata != rdf - 4)
		return GETDNS_RETURN_GENERIC_ERROR;
	if (value && value->size > 0xFF)
		return GETDNS_RETURN_INVALID_PARAMETER;
	if (!value || value->size == 0) {
		rdata[0] = 0;
		*rdf_len = 0;
		return GETDNS_RETURN_GOOD;
	}
	if (value->size > *rdf_len) {
		*rdf_len = value->size;
		return GETDNS_RETURN_NEED_MORE_SPACE;
	}
	*rdf_len = value->size;
	rdata[0] = (uint8_t) value->size;
	(void)memcpy(rdf, value->data, value->size);
	return GETDNS_RETURN_GOOD;
}
static getdns_return_t
hip_hit_dict2wire(
    const getdns_dict *dict, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	getdns_bindata *value;

	if ((r = getdns_dict_get_bindata(dict, "hit", &value)))
		return r;
	else
		return hip_hit_2wire(value, rdata, rdf, rdf_len);
}
static _getdns_rdf_special hip_hit = {
    hip_hit_rdf_end,
    hip_hit_wire2dict, NULL,
    hip_hit_dict2wire, NULL
};

static const uint8_t *
hip_public_key_rdf_end(
    const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	(void)(pkt);
	return rdf + 2 > pkt_end ? NULL
	     : rdf + 2 + rdf[-2] + gldns_read_uint16(rdf) > pkt_end ? NULL
	     : rdf + 2 + rdf[-2] + gldns_read_uint16(rdf);
}
static getdns_return_t
hip_public_key_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	return _getdns_dict_set_const_bindata(
	    dict, "public_key", gldns_read_uint16(rdf), rdf + 2 + rdf[-2]);
}
static getdns_return_t
hip_public_key_2wire(
    const getdns_bindata *value, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	if (rdata > rdf - 4 || rdata + 4 + rdata[0] != rdf)
		return GETDNS_RETURN_GENERIC_ERROR;
	if (value && value->size > 0xFFFF)
		return GETDNS_RETURN_INVALID_PARAMETER;
	if (!value || value->size == 0) {
		rdata[2] = rdata[3] = 0;
		*rdf_len = 0;
		return GETDNS_RETURN_GOOD;
	}
	if (value->size > *rdf_len) {
		*rdf_len = value->size;
		return GETDNS_RETURN_NEED_MORE_SPACE;
	}
	*rdf_len = value->size;
	gldns_write_uint16(rdata + 2, (uint16_t) value->size);
	(void)memcpy(rdf, value->data, value->size);
	return GETDNS_RETURN_GOOD;
}
static getdns_return_t
hip_public_key_dict2wire(
    const getdns_dict *dict, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	getdns_bindata *value;

	if ((r = getdns_dict_get_bindata(dict, "public_key", &value)))
		return r;
	else
		return hip_public_key_2wire(value, rdata, rdf, rdf_len);
}
static _getdns_rdf_special hip_public_key = {
    hip_public_key_rdf_end,
    hip_public_key_wire2dict, NULL,
    hip_public_key_dict2wire, NULL
};

static const uint8_t *
amtrelay_D_rdf_end(const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	(void)pkt;
	return rdf < pkt_end ? rdf + 1 : NULL;
}
static getdns_return_t
amtrelay_D_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	return getdns_dict_set_int(dict, "discovery_optional", (*rdf  >> 7));
}
static getdns_return_t
amtrelay_D_dict2wire(const getdns_dict *dict,
    uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	uint32_t        value;
	(void)rdata; /* unused parameter */

	if ((r = getdns_dict_get_int(dict, "discovery_optional", &value)))
		return r;

	*rdf_len = 1;
	if (*rdf_len < 1)
		return GETDNS_RETURN_NEED_MORE_SPACE;

	*rdf_len = 1;
	*rdf = value ? 0x80 : 0x00;
	return GETDNS_RETURN_GOOD;
}
static _getdns_rdf_special amtrelay_D = {
    amtrelay_D_rdf_end,
    amtrelay_D_wire2dict, NULL,
    amtrelay_D_dict2wire, NULL 
};

static const uint8_t *
amtrelay_rtype_rdf_end(
    const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	(void)pkt; (void)pkt_end;
	return rdf;
}
static getdns_return_t
amtrelay_rtype_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	return getdns_dict_set_int(
	    dict, "replay_type", (rdf[-1] & 0x7F));
}
static getdns_return_t
amtrelay_rtype_dict2wire(
    const getdns_dict *dict, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	uint32_t value;

	if ((r = getdns_dict_get_int(dict, "relay_type", &value)))
		return r;

	if (rdf - 1 < rdata)
		return GETDNS_RETURN_GENERIC_ERROR;

	*rdf_len = 0;
	rdf[-1] |= (value & 0x7F);

	return GETDNS_RETURN_GOOD;
}
static _getdns_rdf_special amtrelay_rtype = {
    amtrelay_rtype_rdf_end,
    amtrelay_rtype_wire2dict, NULL,
    amtrelay_rtype_dict2wire, NULL 
};

static const uint8_t *
amtrelay_relay_rdf_end(
    const uint8_t *pkt, const uint8_t *pkt_end, const uint8_t *rdf)
{
	const uint8_t *end;

	if (rdf - 4 < pkt)
		return NULL;
	switch (rdf[-1] & 0x7F) {
	case 0:	end = rdf;
		break;
	case 1: end = rdf + 4;
		break;
	case 2: end = rdf + 16;
		break;
	case 3: for (end = rdf; end < pkt_end; end += *end + 1)
			if ((*end & 0xC0) == 0xC0)
				end  += 2;
			else if (*end & 0xC0)
				return NULL;
			else if (!*end) {
				end += 1;
				break;
			}
		break;
	default:
		return NULL;
	}
	return end <= pkt_end ? end : NULL;
}
static getdns_return_t
amtrelay_relay_equip_const_bindata(
    const uint8_t *rdf, size_t *size, const uint8_t **data)
{
	*data = rdf;
	switch (rdf[-1] & 0x7F) {
	case 0:	*size = 0;
		break;
	case 1: *size = 4;
		break;
	case 2: *size = 16;
		break;
	case 3: while (*rdf)
			if ((*rdf & 0xC0) == 0xC0)
				rdf += 2;
			else if (*rdf & 0xC0)
				return GETDNS_RETURN_GENERIC_ERROR;
			else
				rdf += *rdf + 1;
		*size = rdf + 1 - *data;
		break;
	default:
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	return GETDNS_RETURN_GOOD;
}

static getdns_return_t
amtrelay_relay_wire2dict(getdns_dict *dict, const uint8_t *rdf)
{
	size_t size;
	const uint8_t *data;

	if (amtrelay_relay_equip_const_bindata(rdf, &size, &data))
		return GETDNS_RETURN_GENERIC_ERROR;

	else if (! size)
		return GETDNS_RETURN_GOOD;
	else
		return _getdns_dict_set_const_bindata(dict, "relay", size, data);
}
static getdns_return_t
amtrelay_relay_2wire(
    const getdns_bindata *value, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	assert(rdf - 1 >= rdata && (rdf[-1] & 0x7F) > 0);

	switch (rdf[-1] & 0x7F) {
	case 1: if (!value || value->size != 4)
			return GETDNS_RETURN_INVALID_PARAMETER;
		if (*rdf_len < 4) {
			*rdf_len = 4;
			return GETDNS_RETURN_NEED_MORE_SPACE;
		}
		*rdf_len = 4;
		(void)memcpy(rdf, value->data, 4);
		return GETDNS_RETURN_GOOD;
	case 2: if (!value || value->size != 16)
			return GETDNS_RETURN_INVALID_PARAMETER;
		if (*rdf_len < 16) {
			*rdf_len = 16;
			return GETDNS_RETURN_NEED_MORE_SPACE;
		}
		*rdf_len = 16;
		(void)memcpy(rdf, value->data, 16);
		return GETDNS_RETURN_GOOD;
	case 3: if (!value || value->size == 0)
			return GETDNS_RETURN_INVALID_PARAMETER;
		/* Assume bindata is a valid dname; garbage in, garbage out */
		if (*rdf_len < value->size) {
			*rdf_len = value->size;
			return GETDNS_RETURN_NEED_MORE_SPACE;
		}
		*rdf_len = value->size;
		(void)memcpy(rdf, value->data, value->size);
		return GETDNS_RETURN_GOOD;
	default:
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	return GETDNS_RETURN_GOOD;
}
static getdns_return_t
amtrelay_relay_dict2wire(
    const getdns_dict *dict, uint8_t *rdata, uint8_t *rdf, size_t *rdf_len)
{
	getdns_return_t r;
	getdns_bindata *value;

	if (rdf - 1 < rdata)
		return GETDNS_RETURN_GENERIC_ERROR;

	else if ((rdf[-1] & 0x7F) == 0) {
		*rdf_len = 0;
		return GETDNS_RETURN_GOOD;
	}
	else if ((r = getdns_dict_get_bindata(dict, "relay", &value)))
		return r;
	else
		return amtrelay_relay_2wire(value, rdata, rdf, rdf_len);
}
static _getdns_rdf_special amtrelay_relay = {
    amtrelay_relay_rdf_end,
    amtrelay_relay_wire2dict, NULL,
    amtrelay_relay_dict2wire, NULL 
};


static _getdns_rdata_def          a_rdata[] = {
	{ "ipv4_address"                , GETDNS_RDF_A      , NULL }};
static _getdns_rdata_def         ns_rdata[] = {
	{ "nsdname"                     , GETDNS_RDF_N_C    , NULL }};
static _getdns_rdata_def         md_rdata[] = {
	{ "madname"                     , GETDNS_RDF_N_C    , NULL }};
static _getdns_rdata_def      cname_rdata[] = {
	{ "cname"                       , GETDNS_RDF_N_C    , NULL }};
static _getdns_rdata_def        soa_rdata[] = {
	{ "mname"                       , GETDNS_RDF_N_C    , NULL },
	{ "rname"                       , GETDNS_RDF_N_C    , NULL },
	{ "serial"                      , GETDNS_RDF_I4     , NULL },
	{ "refresh"                     , GETDNS_RDF_I4     , NULL },
	{ "retry"                       , GETDNS_RDF_I4     , NULL },
	{ "expire"                      , GETDNS_RDF_I4     , NULL },
	{ "minimum"                     , GETDNS_RDF_I4     , NULL }};
static _getdns_rdata_def         mg_rdata[] = {
	{ "mgmname"                     , GETDNS_RDF_N_C    , NULL }};
static _getdns_rdata_def         mr_rdata[] = {
	{ "newname"                     , GETDNS_RDF_N_C    , NULL }};
static _getdns_rdata_def       null_rdata[] = {
	{ "anything"                    , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        wks_rdata[] = {
	{ "address"                     , GETDNS_RDF_A      , NULL },
	{ "protocol"                    , GETDNS_RDF_I1     , NULL },
	{ "bitmap"                      , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        ptr_rdata[] = {
	{ "ptrdname"                    , GETDNS_RDF_N_C    , NULL }};
static _getdns_rdata_def      hinfo_rdata[] = {
	{ "cpu"                         , GETDNS_RDF_S      , NULL },
	{ "os"                          , GETDNS_RDF_S      , NULL }};
static _getdns_rdata_def      minfo_rdata[] = {
	{ "rmailbx"                     , GETDNS_RDF_N_C    , NULL },
	{ "emailbx"                     , GETDNS_RDF_N_C    , NULL }};
static _getdns_rdata_def         mx_rdata[] = {
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "exchange"                    , GETDNS_RDF_N_C    , NULL }};
static _getdns_rdata_def        txt_rdata[] = {
	{ "txt_strings"                 , GETDNS_RDF_S_M    , NULL }};
static _getdns_rdata_def         rp_rdata[] = {
	{ "mbox_dname"                  , GETDNS_RDF_N      , NULL },
	{ "txt_dname"                   , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def      afsdb_rdata[] = {
	{ "subtype"                     , GETDNS_RDF_I2     , NULL },
	{ "hostname"                    , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def        x25_rdata[] = {
	{ "psdn_address"                , GETDNS_RDF_S      , NULL }};
static _getdns_rdata_def       isdn_rdata[] = {
	{ "isdn_address"                , GETDNS_RDF_S      , NULL },
	{ "sa"                          , GETDNS_RDF_S      , NULL }};
static _getdns_rdata_def         rt_rdata[] = {
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "intermediate_host"           , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def       nsap_rdata[] = {
	{ "nsap"                        , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def   nsap_ptr_rdata[] = {
	{ "owner"                       , GETDNS_RDF_S      , NULL }};
static _getdns_rdata_def        sig_rdata[] = {
	{ "sig_obsolete"                , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        key_rdata[] = {
	{ "key_obsolete"                , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def         px_rdata[] = {
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "map822"                      , GETDNS_RDF_N      , NULL },
	{ "mapx400"                     , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def       gpos_rdata[] = {
	{ "longitude"                   , GETDNS_RDF_S      , NULL },
	{ "latitude"                    , GETDNS_RDF_S      , NULL },
	{ "altitude"                    , GETDNS_RDF_S      , NULL }};
static _getdns_rdata_def       aaaa_rdata[] = {
	{ "ipv6_address"                , GETDNS_RDF_AAAA   , NULL }};
static _getdns_rdata_def        loc_rdata[] = {
	{ "loc_obsolete"                , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        nxt_rdata[] = {
	{ "nxt_obsolete"                , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        eid_rdata[] = {
	{ "endpoint_identifier"         , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def     nimloc_rdata[] = {
	{ "nimrod_locator"              , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        srv_rdata[] = {
	{ "priority"                    , GETDNS_RDF_I2     , NULL },
	{ "weight"                      , GETDNS_RDF_I2     , NULL },
	{ "port"                        , GETDNS_RDF_I2     , NULL },
	{ "target"                      , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def       atma_rdata[] = {
	{ "format"                      , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def      naptr_rdata[] = {
	{ "order"                       , GETDNS_RDF_I2     , NULL },
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "flags"                       , GETDNS_RDF_S      , NULL },
	{ "service"                     , GETDNS_RDF_S      , NULL },
	{ "regexp"                      , GETDNS_RDF_S      , NULL },
	{ "replacement"                 , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def         kx_rdata[] = {
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "exchanger"                   , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def       cert_rdata[] = {
	{ "type"                        , GETDNS_RDF_I2     , NULL },
	{ "key_tag"                     , GETDNS_RDF_I2     , NULL },
	{ "algorithm"                   , GETDNS_RDF_I1     , NULL },
	{ "certificate_or_crl"          , GETDNS_RDF_B      , NULL }};
static _getdns_rdata_def         a6_rdata[] = {
	{ "a6_obsolete"                 , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def      dname_rdata[] = {
	{ "target"                      , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def        opt_rdata[] = {
	{ "options"                     , GETDNS_RDF_R      , NULL },
	{ "option_code"                 , GETDNS_RDF_I2     , NULL },
	{ "option_data"                 , GETDNS_RDF_X_S    , NULL }};
static _getdns_rdata_def        apl_rdata[] = {
	{ "apitems"                     , GETDNS_RDF_R      , NULL },
	{ "address_family"              , GETDNS_RDF_I2     , NULL },
	{ "prefix"                      , GETDNS_RDF_I1     , NULL },
	{ "n"                           , GETDNS_RDF_SPECIAL, &apl_n },
	{ "afdpart"                     , GETDNS_RDF_SPECIAL, &apl_afdpart }};
static _getdns_rdata_def         ds_rdata[] = {
	{ "key_tag"                     , GETDNS_RDF_I2     , NULL },
	{ "algorithm"                   , GETDNS_RDF_I1     , NULL },
	{ "digest_type"                 , GETDNS_RDF_I1     , NULL },
	{ "digest"                      , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def      sshfp_rdata[] = {
	{ "algorithm"                   , GETDNS_RDF_I1     , NULL },
	{ "fp_type"                     , GETDNS_RDF_I1     , NULL },
	{ "fingerprint"                 , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def   ipseckey_rdata[] = {
	{ "algorithm"                   , GETDNS_RDF_I1     , NULL },
	{ "gateway_type"                , GETDNS_RDF_I1     , NULL },
	{ "precedence"                  , GETDNS_RDF_I1     , NULL },
	{ "gateway"                     , GETDNS_RDF_SPECIAL, &ipseckey_gateway },
	{ "public_key"                  , GETDNS_RDF_B      , NULL }};
static _getdns_rdata_def      rrsig_rdata[] = {
	{ "type_covered"                , GETDNS_RDF_I2     , NULL },
	{ "algorithm"                   , GETDNS_RDF_I1     , NULL },
	{ "labels"                      , GETDNS_RDF_I1     , NULL },
	{ "original_ttl"                , GETDNS_RDF_I4     , NULL },
	{ "signature_expiration"        , GETDNS_RDF_T      , NULL },
	{ "signature_inception"         , GETDNS_RDF_T      , NULL },
	{ "key_tag"                     , GETDNS_RDF_I2     , NULL },
	{ "signers_name"                , GETDNS_RDF_N      , NULL },
	{ "signature"                   , GETDNS_RDF_B      , NULL }};
static _getdns_rdata_def       nsec_rdata[] = {
	{ "next_domain_name"            , GETDNS_RDF_N      , NULL },
	{ "type_bit_maps"               , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def     dnskey_rdata[] = {
	{ "flags"                       , GETDNS_RDF_I2     , NULL },
	{ "protocol"                    , GETDNS_RDF_I1     , NULL },
	{ "algorithm"                   , GETDNS_RDF_I1     , NULL },
	{ "public_key"                  , GETDNS_RDF_B      , NULL }};
static _getdns_rdata_def      dhcid_rdata[] = {
	{ "dhcid_opaque"                , GETDNS_RDF_B      , NULL }};
static _getdns_rdata_def      nsec3_rdata[] = {
	{ "hash_algorithm"              , GETDNS_RDF_I1     , NULL },
	{ "flags"                       , GETDNS_RDF_I1     , NULL },
	{ "iterations"                  , GETDNS_RDF_I2     , NULL },
	{ "salt"                        , GETDNS_RDF_X_C    , NULL },
	{ "next_hashed_owner_name"      , GETDNS_RDF_B32_C  , NULL },
	{ "type_bit_maps"               , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def nsec3param_rdata[] = {
	{ "hash_algorithm"              , GETDNS_RDF_I1     , NULL },
	{ "flags"                       , GETDNS_RDF_I1     , NULL },
	{ "iterations"                  , GETDNS_RDF_I2     , NULL },
	{ "salt"                        , GETDNS_RDF_X_C    , NULL }};
static _getdns_rdata_def       tlsa_rdata[] = {
	{ "certificate_usage"           , GETDNS_RDF_I1     , NULL },
	{ "selector"                    , GETDNS_RDF_I1     , NULL },
	{ "matching_type"               , GETDNS_RDF_I1     , NULL },
	{ "certificate_association_data", GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        hip_rdata[] = {
	{ "pk_algorithm"                , GETDNS_RDF_SPECIAL, &hip_pk_algorithm },
	{ "hit"                         , GETDNS_RDF_SPECIAL, &hip_hit },
	{ "public_key"                  , GETDNS_RDF_SPECIAL, &hip_public_key },
	{ "rendezvous_servers"          , GETDNS_RDF_N_M    , NULL }};
static _getdns_rdata_def     talink_rdata[] = {
	{ "previous"                    , GETDNS_RDF_N      , NULL },
	{ "next"                        , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def   openpgpkey_rdata[] = {
	{ "transferable_public_key"     , GETDNS_RDF_B      , NULL }};
static _getdns_rdata_def        csync_rdata[] = {
	{ "serial"                      , GETDNS_RDF_I4     , NULL },
	{ "flags"                       , GETDNS_RDF_I2     , NULL },
	{ "type_bit_maps"               , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def     zonemd_rdata[] = {
	{ "serial"                      , GETDNS_RDF_I4     , NULL },
	{ "digest_type"                 , GETDNS_RDF_I1     , NULL },
	{ "reserved"                    , GETDNS_RDF_I1     , NULL },
	{ "digest"                      , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        spf_rdata[] = {
	{ "text"                        , GETDNS_RDF_S_M    , NULL }};
static _getdns_rdata_def        nid_rdata[] = {
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "node_id"                     , GETDNS_RDF_AA     , NULL }};
static _getdns_rdata_def        l32_rdata[] = {
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "locator32"                   , GETDNS_RDF_A      , NULL }};
static _getdns_rdata_def        l64_rdata[] = {
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "locator64"                   , GETDNS_RDF_AA     , NULL }};
static _getdns_rdata_def         lp_rdata[] = {
	{ "preference"                  , GETDNS_RDF_I2     , NULL },
	{ "fqdn"                        , GETDNS_RDF_N      , NULL }};
static _getdns_rdata_def      eui48_rdata[] = {
	{ "eui48_address"               , GETDNS_RDF_X6     , NULL }};
static _getdns_rdata_def      eui64_rdata[] = {
	{ "eui64_address"               , GETDNS_RDF_X8     , NULL }};
static _getdns_rdata_def       tkey_rdata[] = {
	{ "algorithm"                   , GETDNS_RDF_N      , NULL },
	{ "inception"                   , GETDNS_RDF_T      , NULL },
	{ "expiration"                  , GETDNS_RDF_T      , NULL },
	{ "mode"                        , GETDNS_RDF_I2     , NULL },
	{ "error"                       , GETDNS_RDF_I2     , NULL },
	{ "key_data"                    , GETDNS_RDF_X_S    , NULL },
	{ "other_data"                  , GETDNS_RDF_X_S    , NULL }};
static _getdns_rdata_def       tsig_rdata[] = {
	{ "algorithm"                   , GETDNS_RDF_N      , NULL },
	{ "time_signed"                 , GETDNS_RDF_T6     , NULL },
	{ "fudge"                       , GETDNS_RDF_I2     , NULL },
	{ "mac"                         , GETDNS_RDF_X_S    , NULL },
	{ "original_id"                 , GETDNS_RDF_I2     , NULL },
	{ "error"                       , GETDNS_RDF_I2     , NULL },
	{ "other_data"                  , GETDNS_RDF_X_S    , NULL }};
static _getdns_rdata_def        uri_rdata[] = {
	{ "priority"                    , GETDNS_RDF_I2     , NULL },
	{ "weight"                      , GETDNS_RDF_I2     , NULL },
	{ "target"                      , GETDNS_RDF_S_L    , NULL }};
static _getdns_rdata_def        caa_rdata[] = {
	{ "flags"                       , GETDNS_RDF_I1     , NULL },
	{ "tag"                         , GETDNS_RDF_S      , NULL },
	{ "value"                       , GETDNS_RDF_S_L    , NULL }};
static _getdns_rdata_def        dlv_rdata[] = {
	{ "key_tag"                     , GETDNS_RDF_I2     , NULL },
	{ "algorithm"                   , GETDNS_RDF_I1     , NULL },
	{ "digest_type"                 , GETDNS_RDF_I1     , NULL },
	{ "digest"                      , GETDNS_RDF_X      , NULL }};
static _getdns_rdata_def        doa_rdata[] = {
	{ "enterprise"                  , GETDNS_RDF_I4     , NULL },
	{ "type"                        , GETDNS_RDF_I4     , NULL },
	{ "location"                    , GETDNS_RDF_I1     , NULL },
	{ "media_type"                  , GETDNS_RDF_S      , NULL },
	{ "data"                        , GETDNS_RDF_B      , NULL }};
static _getdns_rdata_def   amtrelay_rdata[] = {
	{ "precedence"                  , GETDNS_RDF_I1     , NULL },
	{ "discovery_optional"          , GETDNS_RDF_SPECIAL, &amtrelay_D},
	{ "relay_type"                  , GETDNS_RDF_SPECIAL, &amtrelay_rtype },
	{ "relay"                       , GETDNS_RDF_SPECIAL, &amtrelay_relay }};

static _getdns_rr_def _getdns_rr_defs[] = {
	{         NULL,             NULL, 0                      },
	{          "A",          a_rdata, ALEN(         a_rdata) }, /* 1 - */
	{         "NS",         ns_rdata, ALEN(        ns_rdata) },
	{         "MD",         md_rdata, ALEN(        md_rdata) },
	{         "MF",         md_rdata, ALEN(        md_rdata) },
	{      "CNAME",      cname_rdata, ALEN(     cname_rdata) },
	{        "SOA",        soa_rdata, ALEN(       soa_rdata) },
	{         "MB",         md_rdata, ALEN(        md_rdata) },
	{         "MG",         mg_rdata, ALEN(        mg_rdata) },
	{         "MR",         mr_rdata, ALEN(        mr_rdata) },
	{       "NULL",       null_rdata, ALEN(      null_rdata) },
	{        "WKS",        wks_rdata, ALEN(       wks_rdata) },
	{        "PTR",        ptr_rdata, ALEN(       ptr_rdata) },
	{      "HINFO",      hinfo_rdata, ALEN(     hinfo_rdata) },
	{      "MINFO",      minfo_rdata, ALEN(     minfo_rdata) },
	{         "MX",         mx_rdata, ALEN(        mx_rdata) },
	{        "TXT",        txt_rdata, ALEN(       txt_rdata) },
	{         "RP",         rp_rdata, ALEN(        rp_rdata) },
	{      "AFSDB",      afsdb_rdata, ALEN(     afsdb_rdata) },
	{        "X25",        x25_rdata, ALEN(       x25_rdata) },
	{       "ISDN",       isdn_rdata, ALEN(      isdn_rdata) },
	{         "RT",         rt_rdata, ALEN(        rt_rdata) },
	{       "NSAP",       nsap_rdata, ALEN(      nsap_rdata) },
	{   "NSAP_PTR",   nsap_ptr_rdata, ALEN(  nsap_ptr_rdata) },
	{        "SIG",        sig_rdata, ALEN(       sig_rdata) },
	{        "KEY",        key_rdata, ALEN(       key_rdata) },
	{         "PX",         px_rdata, ALEN(        px_rdata) },
	{       "GPOS",       gpos_rdata, ALEN(      gpos_rdata) },
	{       "AAAA",       aaaa_rdata, ALEN(      aaaa_rdata) },
	{        "LOC",        loc_rdata, ALEN(       loc_rdata) },
	{        "NXT",        nxt_rdata, ALEN(       nxt_rdata) },
	{        "EID",        eid_rdata, ALEN(       eid_rdata) },
	{     "NIMLOC",     nimloc_rdata, ALEN(    nimloc_rdata) },
	{        "SRV",        srv_rdata, ALEN(       srv_rdata) },
	{       "ATMA",       atma_rdata, ALEN(      atma_rdata) },
	{      "NAPTR",      naptr_rdata, ALEN(     naptr_rdata) },
	{         "KX",         kx_rdata, ALEN(        kx_rdata) },
	{       "CERT",       cert_rdata, ALEN(      cert_rdata) },
	{         "A6",         a6_rdata, ALEN(        a6_rdata) },
	{      "DNAME",      dname_rdata, ALEN(     dname_rdata) },
	{       "SINK",    UNKNOWN_RDATA, 0                      },
	{        "OPT",        opt_rdata, ALEN(       opt_rdata) },
	{        "APL",        apl_rdata, ALEN(       apl_rdata) },
	{         "DS",         ds_rdata, ALEN(        ds_rdata) },
	{      "SSHFP",      sshfp_rdata, ALEN(     sshfp_rdata) },
	{   "IPSECKEY",   ipseckey_rdata, ALEN(  ipseckey_rdata) },
	{      "RRSIG",      rrsig_rdata, ALEN(     rrsig_rdata) },
	{       "NSEC",       nsec_rdata, ALEN(      nsec_rdata) },
	{     "DNSKEY",     dnskey_rdata, ALEN(    dnskey_rdata) },
	{      "DHCID",      dhcid_rdata, ALEN(     dhcid_rdata) },
	{      "NSEC3",      nsec3_rdata, ALEN(     nsec3_rdata) },
	{ "NSEC3PARAM", nsec3param_rdata, ALEN(nsec3param_rdata) },
	{       "TLSA",       tlsa_rdata, ALEN(      tlsa_rdata) },
	{     "SMIMEA",       tlsa_rdata, ALEN(      tlsa_rdata) }, /* - 53 */
	{         NULL,             NULL, 0                      },
	{        "HIP",        hip_rdata, ALEN(       hip_rdata) }, /* 55 - */
	{      "NINFO",    UNKNOWN_RDATA, 0                      },
	{       "RKEY",    UNKNOWN_RDATA, 0                      },
	{     "TALINK",     talink_rdata, ALEN(    talink_rdata) },
	{        "CDS",         ds_rdata, ALEN(        ds_rdata) },
	{    "CDNSKEY",     dnskey_rdata, ALEN(    dnskey_rdata) },
	{ "OPENPGPKEY", openpgpkey_rdata, ALEN(openpgpkey_rdata) },
	{      "CSYNC",      csync_rdata, ALEN(     csync_rdata) },
	{     "ZONEMD",     zonemd_rdata, ALEN(    zonemd_rdata) }, /* - 63 */
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{        "SPF",        spf_rdata, ALEN(       spf_rdata) }, /* 99 - */
	{      "UINFO",    UNKNOWN_RDATA, 0                      },
	{        "UID",    UNKNOWN_RDATA, 0                      },
	{        "GID",    UNKNOWN_RDATA, 0                      },
	{     "UNSPEC",    UNKNOWN_RDATA, 0                      },
	{        "NID",        nid_rdata, ALEN(       nid_rdata) },
	{        "L32",        l32_rdata, ALEN(       l32_rdata) },
	{        "L64",        l64_rdata, ALEN(       l64_rdata) },
	{         "LP",         lp_rdata, ALEN(        lp_rdata) },
	{      "EUI48",      eui48_rdata, ALEN(     eui48_rdata) },
	{      "EUI64",      eui64_rdata, ALEN(     eui64_rdata) }, /* - 109 */
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{       "TKEY",       tkey_rdata, ALEN(      tkey_rdata) }, /* 249 - */
	{       "TSIG",       tsig_rdata, ALEN(      tsig_rdata) }, /* - 250 */
	{         NULL,             NULL, 0                      },
	{         NULL,             NULL, 0                      },
	{      "MAILB",    UNKNOWN_RDATA, 0                      }, /* 253 - */
	{      "MAILA",    UNKNOWN_RDATA, 0                      }, /* - 254 */
	{         NULL,             NULL, 0                      },
	{        "URI",        uri_rdata, ALEN(       uri_rdata) }, /* 256 - */
	{        "CAA",        caa_rdata, ALEN(       caa_rdata) },
	{        "AVC",        txt_rdata, ALEN(       txt_rdata) },
	{        "DOA",        doa_rdata, ALEN(       doa_rdata) },
	{   "AMTRELAY",   amtrelay_rdata, ALEN(  amtrelay_rdata) }, /* - 260 */
	{         "TA",         ds_rdata, ALEN(        ds_rdata) }, /* 32768 */
	{        "DLV",        dlv_rdata, ALEN(       dlv_rdata) }  /* 32769 */
};

const _getdns_rr_def *
_getdns_rr_def_lookup(uint16_t rr_type)
{
	if (rr_type <= 260)
		return &_getdns_rr_defs[rr_type];
	else if (rr_type == 32768)
		return &_getdns_rr_defs[261];
	else if (rr_type == 32769)
		return &_getdns_rr_defs[262];
	return _getdns_rr_defs;
}

const char *
_getdns_rr_type_name(int rr_type)
{
	return _getdns_rr_def_lookup(rr_type)->name;
}

static void
write_int_rdata(gldns_buffer *buf, _getdns_rdf_type type, uint32_t value)
{
	size_t j;

	for (j = type & GETDNS_RDF_FIXEDSZ; j; j--)
		gldns_buffer_write_u8(buf,
		    (uint8_t)(value >> (8 * (j - 1))) & 0xff);
}

static void
write_bindata_rdata(gldns_buffer *buf,
    _getdns_rdf_type type, getdns_bindata *bindata)
{
	if (type & GETDNS_RDF_LEN_VAL)
		write_int_rdata(buf, type >> 8, bindata->size);

	gldns_buffer_write(buf, bindata->data, bindata->size);
}


static getdns_return_t
write_rdata_field(gldns_buffer *buf, uint8_t *rdata_start,
    const _getdns_rdata_def *rd_def, getdns_dict *rdata)
{
	getdns_return_t  r;
	getdns_list     *list;
	uint32_t         value;
	getdns_bindata  *bindata;
	size_t           i, rdf_len;

	if (rd_def->type & GETDNS_RDF_INTEGER) {
		if (!(rd_def->type & GETDNS_RDF_REPEAT)) {
			if ((r = getdns_dict_get_int(
			    rdata, rd_def->name, &value)))
				return r;
			else
				write_int_rdata(buf, rd_def->type, value);

		} else if ((r = getdns_dict_get_list(
		    rdata, rd_def->name, &list)))

			return r == GETDNS_RETURN_NO_SUCH_DICT_NAME
			          ? GETDNS_RETURN_GOOD : r;

		else for ( i = 0
			 ; GETDNS_RETURN_GOOD ==
			       (r = getdns_list_get_int(list, i, &value))
			 ; i++)
			write_int_rdata(buf, rd_def->type, value);

		
	} else if (rd_def->type & GETDNS_RDF_BINDATA) {


		if (!(rd_def->type & GETDNS_RDF_REPEAT)) {
			if ((r = getdns_dict_get_bindata(
			    rdata, rd_def->name, &bindata)))
				return r;
			else
				write_bindata_rdata(buf, rd_def->type, bindata);

		} else if ((r = getdns_dict_get_list(
		    rdata, rd_def->name, &list)))

			return r == GETDNS_RETURN_NO_SUCH_DICT_NAME
			          ? GETDNS_RETURN_GOOD : r;

		else for ( i = 0
			 ; GETDNS_RETURN_GOOD ==
			       (r = getdns_list_get_bindata(list, i, &bindata))
			 ; i++)
			write_bindata_rdata(buf, rd_def->type, bindata);


	} else if (!(rd_def->type & GETDNS_RDF_SPECIAL)) {
		/* Unknown rdata type */
		return GETDNS_RETURN_GENERIC_ERROR;

	} else if (!(rd_def->type & GETDNS_RDF_REPEAT)) {
		/*
		 * Non repetitive special rdatafield,
		 * We must have a dict2wire function
		 */
		assert(rd_def->special->dict2wire);

		rdf_len = gldns_buffer_remaining(buf);
		r = rd_def->special->dict2wire(rdata, rdata_start,
		    gldns_buffer_current(buf), &rdf_len);
		if (r == GETDNS_RETURN_GOOD ||
		    r == GETDNS_RETURN_NEED_MORE_SPACE)
			gldns_buffer_skip(buf, rdf_len);
		if (r)
			return r;

	/* We do not have repetitive special rdata fields (yet)
	 *
	 * LCOV_EXCL_START
	 */
	} else if ((r = getdns_dict_get_list(rdata, rd_def->name, &list))) {

		return r == GETDNS_RETURN_NO_SUCH_DICT_NAME
			  ? GETDNS_RETURN_GOOD : r;

	} else for ( i = 0; r == GETDNS_RETURN_GOOD; i++ ) {
		/*
		 * A repetitive special rdata field must have the list2wire
		 * function.
		 */
		assert(rd_def->special->list2wire);
		
		rdf_len = gldns_buffer_remaining(buf);
		r = rd_def->special->list2wire(list, i, rdata_start,
		    gldns_buffer_current(buf), &rdf_len);
		if (r == GETDNS_RETURN_GOOD ||
		    r == GETDNS_RETURN_NEED_MORE_SPACE)
			gldns_buffer_skip(buf, rdf_len);
	}
	/* LCOV_EXCL_STOP */

	return r != GETDNS_RETURN_NO_SUCH_LIST_ITEM ? r : GETDNS_RETURN_GOOD;
}

getdns_return_t
_getdns_rr_dict2wire(const getdns_dict *rr_dict, gldns_buffer *buf)
{
	getdns_return_t r = GETDNS_RETURN_GOOD;
	getdns_bindata root = { 1, (void *)"" };
	getdns_bindata *name;
	getdns_bindata *rdata_raw;
	getdns_dict *rdata;
	uint32_t rr_type;
	uint32_t rr_class = GETDNS_RRCLASS_IN;
	uint32_t rr_ttl = 0;
	uint32_t value;
	const _getdns_rr_def *rr_def;
	const _getdns_rdata_def *rd_def, *rep_rd_def;
	int n_rdata_fields, rep_n_rdata_fields;
	size_t rdata_size_mark;
	uint8_t *rdata_start;
	getdns_list *list;
	size_t i;

	assert(rr_dict);
	assert(buf);

	if ((r = getdns_dict_get_int(rr_dict, "type", &rr_type)))
		return r;
	if ((r = getdns_dict_get_bindata(rr_dict, "name", &name))) {
		if (r == GETDNS_RETURN_NO_SUCH_DICT_NAME &&
		    rr_type == GETDNS_RRTYPE_OPT) {
			name = &root;
		} else
			return r;
	}
	gldns_buffer_write(buf, name->data, name->size);
	gldns_buffer_write_u16(buf, (uint16_t)rr_type);

	(void) getdns_dict_get_int(rr_dict, "class", &rr_class);
	if (rr_type == GETDNS_RRTYPE_OPT) 
		(void) getdns_dict_get_int(
		    rr_dict, "udp_payload_size", &rr_class);
	gldns_buffer_write_u16(buf, (uint16_t)rr_class);

	(void) getdns_dict_get_int(rr_dict, "ttl", &rr_ttl);
	if (rr_type == GETDNS_RRTYPE_OPT) {
		if (!getdns_dict_get_int(rr_dict, "extended_rcode", &value))
			rr_ttl = (rr_ttl & 0x00FFFFFF)|((value & 0xFF) << 24);
		if (!getdns_dict_get_int(rr_dict, "version", &value))
			rr_ttl = (rr_ttl & 0xFF00FFFF)|((value & 0xFF) << 16);
		if (!getdns_dict_get_int(rr_dict, "z", &value))
			rr_ttl = (rr_ttl & 0xFFFF0000)| (value & 0xFFFF);
		if (!getdns_dict_get_int(rr_dict, "do", &value))
			rr_ttl = (rr_ttl & 0xFFFF7FFF)| (value ? 0x8000 : 0);
	}
	gldns_buffer_write_u32(buf, rr_ttl);

	/* Does rdata contain compressed names?
	 * Because rdata_raw is unusable then.
	 */
	rr_def = _getdns_rr_def_lookup(rr_type);
	for ( rd_def = rr_def->rdata
	    , n_rdata_fields = rr_def->n_rdata_fields
	    ; n_rdata_fields ; n_rdata_fields-- , rd_def++ ) {

		if (rd_def->type & GETDNS_RDF_COMPRESSED)
			break;
	}

	if ((r = getdns_dict_get_dict(rr_dict, "rdata", &rdata))) {
		if (r == GETDNS_RETURN_NO_SUCH_DICT_NAME) {
			gldns_buffer_write_u16(buf, 0);
			r = GETDNS_RETURN_GOOD;
		}

	} else if (n_rdata_fields == 0 && GETDNS_RETURN_GOOD ==
	    (r = getdns_dict_get_bindata(rdata, "rdata_raw", &rdata_raw))) {

		gldns_buffer_write_u16(buf, (uint16_t)rdata_raw->size);
		gldns_buffer_write(buf, rdata_raw->data, rdata_raw->size);

	} else if (n_rdata_fields || r == GETDNS_RETURN_NO_SUCH_DICT_NAME) {

		r = GETDNS_RETURN_GOOD;
		rdata_size_mark = gldns_buffer_position(buf);
		gldns_buffer_skip(buf, 2);
		rdata_start = gldns_buffer_current(buf);

		for ( rd_def = rr_def->rdata
		    , n_rdata_fields = rr_def->n_rdata_fields
		    ; n_rdata_fields ; n_rdata_fields-- , rd_def++ ) {

			if (rd_def->type == GETDNS_RDF_REPEAT)
				break;

			if ((r = write_rdata_field(buf,
			    rdata_start, rd_def, rdata)))
				break;
		}
		if (n_rdata_fields == 0 || r) { 
			/* pass */;

		} else if ((r = getdns_dict_get_list(
		    rdata, rd_def->name, &list))) {
			/* pass */;

		} else for ( i = 0
		           ; r == GETDNS_RETURN_GOOD
		           ; i++) {

			if ((r = getdns_list_get_dict(list, i, &rdata))) {
				if (r == GETDNS_RETURN_NO_SUCH_LIST_ITEM)
					r = GETDNS_RETURN_GOOD;
				break;
			}
			for ( rep_rd_def = rd_def + 1
			    , rep_n_rdata_fields = n_rdata_fields - 1
			    ; rep_n_rdata_fields
			    ; rep_n_rdata_fields--, rep_rd_def++ ) {

				if ((r = write_rdata_field(buf,
				    rdata_start, rep_rd_def, rdata)))
					break;
			}
		}
		gldns_buffer_write_u16_at(buf, rdata_size_mark,
		    (uint16_t)(gldns_buffer_position(buf)-rdata_size_mark-2));
	}
	return r;
}

