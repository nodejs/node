/*
 * wire2str.c
 *
 * conversion routines from the wire format
 * to the presentation format (strings)
 *
 * (c) NLnet Labs, 2004-2006
 *
 * See the file LICENSE for the license
 */
/**
 * \file
 *
 * Contains functions to translate the wireformat to text
 * representation, as well as functions to print them.
 */
#include "config.h"
#include "gldns/wire2str.h"
#include "gldns/str2wire.h"
#include "gldns/rrdef.h"
#include "gldns/pkthdr.h"
#include "gldns/parseutil.h"
#include "gldns/gbuffer.h"
#include "gldns/keyraw.h"
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <stdarg.h>
#include <ctype.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

/* lookup tables for standard DNS stuff  */
/* Taken from RFC 2535, section 7.  */
static gldns_lookup_table gldns_algorithms_data[] = {
	{ GLDNS_RSAMD5, "RSAMD5" },
	{ GLDNS_DH, "DH" },
	{ GLDNS_DSA, "DSA" },
	{ GLDNS_ECC, "ECC" },
	{ GLDNS_RSASHA1, "RSASHA1" },
	{ GLDNS_DSA_NSEC3, "DSA-NSEC3-SHA1" },
	{ GLDNS_RSASHA1_NSEC3, "RSASHA1-NSEC3-SHA1" },
	{ GLDNS_RSASHA256, "RSASHA256"},
	{ GLDNS_RSASHA512, "RSASHA512"},
	{ GLDNS_ECC_GOST, "ECC-GOST"},
	{ GLDNS_ECDSAP256SHA256, "ECDSAP256SHA256"},
	{ GLDNS_ECDSAP384SHA384, "ECDSAP384SHA384"},
	{ GLDNS_ED25519, "ED25519"},
	{ GLDNS_ED448, "ED448"},
	{ GLDNS_INDIRECT, "INDIRECT" },
	{ GLDNS_PRIVATEDNS, "PRIVATEDNS" },
	{ GLDNS_PRIVATEOID, "PRIVATEOID" },
	{ 0, NULL }
};
gldns_lookup_table* gldns_algorithms = gldns_algorithms_data;

/* hash algorithms in DS record */
static gldns_lookup_table gldns_hashes_data[] = {
	{ GLDNS_SHA1, "SHA1" },
	{ GLDNS_SHA256, "SHA256" },
	{ GLDNS_HASH_GOST, "HASH-GOST" },
	{ GLDNS_SHA384, "SHA384" },
	{ 0, NULL }
};
gldns_lookup_table* gldns_hashes = gldns_hashes_data;

/* Taken from RFC 4398  */
static gldns_lookup_table gldns_cert_algorithms_data[] = {
	{ GLDNS_CERT_PKIX, "PKIX" },
	{ GLDNS_CERT_SPKI, "SPKI" },
	{ GLDNS_CERT_PGP, "PGP" },
	{ GLDNS_CERT_IPKIX, "IPKIX" },
	{ GLDNS_CERT_ISPKI, "ISPKI" },
	{ GLDNS_CERT_IPGP, "IPGP" },
	{ GLDNS_CERT_ACPKIX, "ACPKIX" },
	{ GLDNS_CERT_IACPKIX, "IACPKIX" },
	{ GLDNS_CERT_URI, "URI" },
	{ GLDNS_CERT_OID, "OID" },
	{ 0, NULL }
};
gldns_lookup_table* gldns_cert_algorithms = gldns_cert_algorithms_data;

/* if these are used elsewhere */
static gldns_lookup_table gldns_rcodes_data[] = {
	{ GLDNS_RCODE_NOERROR, "NOERROR" },
	{ GLDNS_RCODE_FORMERR, "FORMERR" },
	{ GLDNS_RCODE_SERVFAIL, "SERVFAIL" },
	{ GLDNS_RCODE_NXDOMAIN, "NXDOMAIN" },
	{ GLDNS_RCODE_NOTIMPL, "NOTIMPL" },
	{ GLDNS_RCODE_REFUSED, "REFUSED" },
	{ GLDNS_RCODE_YXDOMAIN, "YXDOMAIN" },
	{ GLDNS_RCODE_YXRRSET, "YXRRSET" },
	{ GLDNS_RCODE_NXRRSET, "NXRRSET" },
	{ GLDNS_RCODE_NOTAUTH, "NOTAUTH" },
	{ GLDNS_RCODE_NOTZONE, "NOTZONE" },
	{ 0, NULL }
};
gldns_lookup_table* gldns_rcodes = gldns_rcodes_data;

static gldns_lookup_table gldns_opcodes_data[] = {
	{ GLDNS_PACKET_QUERY, "QUERY" },
	{ GLDNS_PACKET_IQUERY, "IQUERY" },
	{ GLDNS_PACKET_STATUS, "STATUS" },
	{ GLDNS_PACKET_NOTIFY, "NOTIFY" },
	{ GLDNS_PACKET_UPDATE, "UPDATE" },
	{ 0, NULL }
};
gldns_lookup_table* gldns_opcodes = gldns_opcodes_data;

static gldns_lookup_table gldns_wireparse_errors_data[] = {
	{ GLDNS_WIREPARSE_ERR_OK, "no parse error" },
	{ GLDNS_WIREPARSE_ERR_GENERAL, "parse error" },
	{ GLDNS_WIREPARSE_ERR_DOMAINNAME_OVERFLOW, "Domainname length overflow" },
	{ GLDNS_WIREPARSE_ERR_DOMAINNAME_UNDERFLOW, "Domainname length underflow (zero length)" },
	{ GLDNS_WIREPARSE_ERR_BUFFER_TOO_SMALL, "buffer too small" },
	{ GLDNS_WIREPARSE_ERR_LABEL_OVERFLOW, "Label length overflow" },
	{ GLDNS_WIREPARSE_ERR_EMPTY_LABEL, "Empty label" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_BAD_ESCAPE, "Syntax error, bad escape sequence" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX, "Syntax error, could not parse the RR" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_TTL, "Syntax error, could not parse the RR's TTL" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_TYPE, "Syntax error, could not parse the RR's type" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_CLASS, "Syntax error, could not parse the RR's class" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_RDATA, "Syntax error, could not parse the RR's rdata" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_MISSING_VALUE, "Syntax error, value expected" },
	{ GLDNS_WIREPARSE_ERR_INVALID_STR, "Conversion error, string expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_B64, "Conversion error, b64 encoding expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_B32_EXT, "Conversion error, b32 ext encoding expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_HEX, "Conversion error, hex encoding expected" },
	{ GLDNS_WIREPARSE_ERR_CERT_BAD_ALGORITHM, "Bad algorithm type for CERT record" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_TIME, "Conversion error, time encoding expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_PERIOD, "Conversion error, time period encoding expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_ILNP64, "Conversion error, 4 colon separated hex numbers expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_EUI48,
		"Conversion error, 6 two character hex numbers "
		"separated by dashes expected (i.e. xx-xx-xx-xx-xx-xx" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_EUI64,
		"Conversion error, 8 two character hex numbers "
		"separated by dashes expected (i.e. xx-xx-xx-xx-xx-xx-xx-xx" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_TAG,
		"Conversion error, a non-zero sequence of US-ASCII letters "
		"and numbers in lower case expected" },
	{ GLDNS_WIREPARSE_ERR_NOT_IMPL, "not implemented" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_INT, "Conversion error, integer expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_IP4, "Conversion error, ip4 addr expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_IP6, "Conversion error, ip6 addr expected" },
	{ GLDNS_WIREPARSE_ERR_SYNTAX_INTEGER_OVERFLOW, "Syntax error, integer overflow" },
	{ GLDNS_WIREPARSE_ERR_INCLUDE, "$INCLUDE directive was seen in the zone" },
	{ GLDNS_WIREPARSE_ERR_PARENTHESIS, "Parse error, parenthesis mismatch" },
	{ 0, NULL }
};
gldns_lookup_table* gldns_wireparse_errors = gldns_wireparse_errors_data;

static gldns_lookup_table gldns_edns_flags_data[] = {
	{ 3600, "do"},
	{ 0, NULL}
};
gldns_lookup_table* gldns_edns_flags = gldns_edns_flags_data;

static gldns_lookup_table gldns_edns_options_data[] = {
	{ 1, "LLQ" },
	{ 2, "UL" },
	{ 3, "NSID" },
	/* 4 draft-cheshire-edns0-owner-option */
	{ 5, "DAU" },
	{ 6, "DHU" },
	{ 7, "N3U" },
	{ 8, "edns-client-subnet" },
	{ 11, "edns-tcp-keepalive"},
	{ 12, "Padding" },
	{ 0, NULL}
};
gldns_lookup_table* gldns_edns_options = gldns_edns_options_data;

static gldns_lookup_table gldns_tsig_errors_data[] = {
	{ GLDNS_TSIG_ERROR_NOERROR, "NOERROR" },
	{ GLDNS_RCODE_FORMERR, "FORMERR" },
	{ GLDNS_RCODE_SERVFAIL, "SERVFAIL" },
	{ GLDNS_RCODE_NXDOMAIN, "NXDOMAIN" },
	{ GLDNS_RCODE_NOTIMPL, "NOTIMPL" },
	{ GLDNS_RCODE_REFUSED, "REFUSED" },
	{ GLDNS_RCODE_YXDOMAIN, "YXDOMAIN" },
	{ GLDNS_RCODE_YXRRSET, "YXRRSET" },
	{ GLDNS_RCODE_NXRRSET, "NXRRSET" },
	{ GLDNS_RCODE_NOTAUTH, "NOTAUTH" },
	{ GLDNS_RCODE_NOTZONE, "NOTZONE" },
	{ GLDNS_TSIG_ERROR_BADSIG, "BADSIG" },
	{ GLDNS_TSIG_ERROR_BADKEY, "BADKEY" },
	{ GLDNS_TSIG_ERROR_BADTIME, "BADTIME" },
	{ GLDNS_TSIG_ERROR_BADMODE, "BADMODE" },
	{ GLDNS_TSIG_ERROR_BADNAME, "BADNAME" },
	{ GLDNS_TSIG_ERROR_BADALG, "BADALG" },
	{ 0, NULL }
};
gldns_lookup_table* gldns_tsig_errors = gldns_tsig_errors_data;

char* gldns_wire2str_pkt(uint8_t* data, size_t len)
{
	size_t slen = (size_t)gldns_wire2str_pkt_buf(data, len, NULL, 0);
	char* result = (char*)malloc(slen+1);
	if(!result) return NULL;
	gldns_wire2str_pkt_buf(data, len, result, slen+1);
	return result;
}

char* gldns_wire2str_rr(uint8_t* rr, size_t len)
{
	size_t slen = (size_t)gldns_wire2str_rr_buf(rr, len, NULL, 0);
	char* result = (char*)malloc(slen+1);
	if(!result) return NULL;
	gldns_wire2str_rr_buf(rr, len, result, slen+1);
	return result;
}

char* gldns_wire2str_type(uint16_t rrtype)
{
	char buf[16];
	gldns_wire2str_type_buf(rrtype, buf, sizeof(buf));
	return strdup(buf);
}

char* gldns_wire2str_class(uint16_t rrclass)
{
	char buf[16];
	gldns_wire2str_class_buf(rrclass, buf, sizeof(buf));
	return strdup(buf);
}

char* gldns_wire2str_dname(uint8_t* dname, size_t dname_len)
{
	size_t slen=(size_t)gldns_wire2str_dname_buf(dname, dname_len, NULL, 0);
	char* result = (char*)malloc(slen+1);
	if(!result) return NULL;
	gldns_wire2str_dname_buf(dname, dname_len, result, slen+1);
	return result;
}

char* gldns_wire2str_rcode(int rcode)
{
	char buf[16];
	gldns_wire2str_rcode_buf(rcode, buf, sizeof(buf));
	return strdup(buf);
}

int gldns_wire2str_pkt_buf(uint8_t* d, size_t dlen, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_pkt_scan(&d, &dlen, &s, &slen);
}

int gldns_wire2str_rr_buf(uint8_t* d, size_t dlen, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_rr_scan(&d, &dlen, &s, &slen, NULL, 0, NULL);
}

int gldns_wire2str_rrquestion_buf(uint8_t* d, size_t dlen, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_rrquestion_scan(&d, &dlen, &s, &slen, NULL, 0, NULL);
}

int gldns_wire2str_rdata_buf(uint8_t* rdata, size_t rdata_len, char* str,
	size_t str_len, uint16_t rrtype)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_rdata_scan(&rdata, &rdata_len, &str, &str_len,
		rrtype, NULL, 0, NULL);
}

int gldns_wire2str_rr_unknown_buf(uint8_t* d, size_t dlen, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_rr_unknown_scan(&d, &dlen, &s, &slen, NULL, 0, NULL);
}

int gldns_wire2str_rr_comment_buf(uint8_t* rr, size_t rrlen, size_t dname_len,
	char* s, size_t slen)
{
	uint16_t rrtype = gldns_wirerr_get_type(rr, rrlen, dname_len);
	return gldns_wire2str_rr_comment_print(&s, &slen, rr, rrlen, dname_len,
		rrtype);
}

int gldns_wire2str_type_buf(uint16_t rrtype, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_type_print(&s, &slen, rrtype);
}

int gldns_wire2str_class_buf(uint16_t rrclass, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_class_print(&s, &slen, rrclass);
}

int gldns_wire2str_rcode_buf(int rcode, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_rcode_print(&s, &slen, rcode);
}

int gldns_wire2str_opcode_buf(int opcode, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_opcode_print(&s, &slen, opcode);
}

int gldns_wire2str_dname_buf(uint8_t* d, size_t dlen, char* s, size_t slen)
{
	/* use arguments as temporary variables */
	return gldns_wire2str_dname_scan(&d, &dlen, &s, &slen, NULL, 0, NULL);
}

int gldns_str_vprint(char** str, size_t* slen, const char* format, va_list args)
{
	int w = vsnprintf(*str, *slen, format, args);
	if(w < 0) {
		/* error in printout */
		return 0;
	} else if((size_t)w >= *slen) {
		*str = NULL; /* we do not want str to point outside of buffer*/
		*slen = 0;
	} else {
		*str += w;
		*slen -= w;
	}
	return w;
}

int gldns_str_print(char** str, size_t* slen, const char* format, ...)
{
	int w;
	va_list args;
	va_start(args, format);
	w = gldns_str_vprint(str, slen, format, args);
	va_end(args);
	return w;
}

/** print hex format into text buffer for specified length */
static int print_hex_buf(char** s, size_t* slen, uint8_t* buf, size_t len)
{
	const char* hex = "0123456789ABCDEF";
	size_t i;
	for(i=0; i<len; i++) {
		(void)gldns_str_print(s, slen, "%c%c", hex[(buf[i]&0xf0)>>4],
			hex[buf[i]&0x0f]);
	}
	return (int)len*2;
}

/** print remainder of buffer in hex format with prefixed text */
static int print_remainder_hex(const char* pref, uint8_t** d, size_t* dlen,
	char** s, size_t* slen)
{
	int w = 0;
	w += gldns_str_print(s, slen, "%s", pref);
	w += print_hex_buf(s, slen, *d, *dlen);
	*d += *dlen;
	*dlen = 0;
	return w;
}

int gldns_wire2str_pkt_scan(uint8_t** d, size_t* dlen, char** s, size_t* slen)
{
	int w = 0, comprloop = 0;
	unsigned qdcount, ancount, nscount, arcount, i;
	uint8_t* pkt = *d;
	size_t pktlen = *dlen;
	if(*dlen >= GLDNS_HEADER_SIZE) {
		qdcount = (unsigned)GLDNS_QDCOUNT(*d);
		ancount = (unsigned)GLDNS_ANCOUNT(*d);
		nscount = (unsigned)GLDNS_NSCOUNT(*d);
		arcount = (unsigned)GLDNS_ARCOUNT(*d);
	} else {
		qdcount = ancount = nscount = arcount = 0;
	}
	w += gldns_wire2str_header_scan(d, dlen, s, slen);
	w += gldns_str_print(s, slen, "\n");
	w += gldns_str_print(s, slen, ";; QUESTION SECTION:\n");
	for(i=0; i<qdcount; i++) {
		w += gldns_wire2str_rrquestion_scan(d, dlen, s, slen,
			pkt, pktlen, &comprloop);
		if(!*dlen) break;
	}
	w += gldns_str_print(s, slen, "\n");
	w += gldns_str_print(s, slen, ";; ANSWER SECTION:\n");
	for(i=0; i<ancount; i++) {
		w += gldns_wire2str_rr_scan(d, dlen, s, slen, pkt, pktlen, &comprloop);
		if(!*dlen) break;
	}
	w += gldns_str_print(s, slen, "\n");
	w += gldns_str_print(s, slen, ";; AUTHORITY SECTION:\n");
	for(i=0; i<nscount; i++) {
		w += gldns_wire2str_rr_scan(d, dlen, s, slen, pkt, pktlen, &comprloop);
		if(!*dlen) break;
	}
	w += gldns_str_print(s, slen, "\n");
	w += gldns_str_print(s, slen, ";; ADDITIONAL SECTION:\n");
	for(i=0; i<arcount; i++) {
		w += gldns_wire2str_rr_scan(d, dlen, s, slen, pkt, pktlen, &comprloop);
		if(!*dlen) break;
	}
	/* other fields: WHEN(time), SERVER(IP) not available here. */
	w += gldns_str_print(s, slen, ";; MSG SIZE  rcvd: %d\n", (int)pktlen);
	if(*dlen > 0) {
		w += print_remainder_hex(";; trailing garbage 0x",
			d, dlen, s, slen);
		w += gldns_str_print(s, slen, "\n");
	}
	return w;
}

/** scan type, class and ttl and printout, for rr */
static int gldns_rr_tcttl_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w = 0;
	uint16_t t, c;
	uint32_t ttl;
	if(*dl < 8) {
		if(*dl < 4)
			return w + print_remainder_hex("; Error malformed 0x",
				d, dl, s, sl);
		/* these print values or 0x.. if none left */
		t = gldns_read_uint16(*d);
		c = gldns_read_uint16((*d)+2);
		(*d)+=4;
		(*dl)-=4;
		w += gldns_wire2str_class_print(s, sl, c);
		w += gldns_str_print(s, sl, "\t");
		w += gldns_wire2str_type_print(s, sl, t);
		if(*dl == 0)
			return w + gldns_str_print(s, sl, "; Error no ttl");
		return w + print_remainder_hex(
			"; Error malformed ttl 0x", d, dl, s, sl);
	}
	t = gldns_read_uint16(*d);
	c = gldns_read_uint16((*d)+2);
	ttl = gldns_read_uint32((*d)+4);
	(*d)+=8;
	(*dl)-=8;
	w += gldns_str_print(s, sl, "%lu\t", (unsigned long)ttl);
	w += gldns_wire2str_class_print(s, sl, c);
	w += gldns_str_print(s, sl, "\t");
	w += gldns_wire2str_type_print(s, sl, t);
	return w;
}

int gldns_wire2str_rr_scan(uint8_t** d, size_t* dlen, char** s, size_t* slen,
	uint8_t* pkt, size_t pktlen, int* comprloop)
{
	int w = 0;
	uint8_t* rr = *d;
	size_t rrlen = *dlen, dname_off, rdlen, ordlen;
	uint16_t rrtype = 0;
	
	if(*dlen >= 3 && (*d)[0]==0 &&
		gldns_read_uint16((*d)+1)==GLDNS_RR_TYPE_OPT) {
		/* perform EDNS OPT processing */
		return gldns_wire2str_edns_scan(d, dlen, s, slen, pkt, pktlen);
	}

	/* try to scan the rdata with pretty-printing, but if that fails, then
	 * scan the rdata as an unknown RR type */
	w += gldns_wire2str_dname_scan(d, dlen, s, slen, pkt, pktlen, comprloop);
	w += gldns_str_print(s, slen, "\t");
	dname_off = rrlen-(*dlen);
	if(*dlen == 4) {
		/* like a question-RR */
		uint16_t t = gldns_read_uint16(*d);
		uint16_t c = gldns_read_uint16((*d)+2);
		(*d)+=4;
		(*dlen)-=4;
		w += gldns_wire2str_class_print(s, slen, c);
		w += gldns_str_print(s, slen, "\t");
		w += gldns_wire2str_type_print(s, slen, t);
		w += gldns_str_print(s, slen, " ; Error no ttl,rdata\n");
		return w;
	}
	if(*dlen < 8) {
		if(*dlen == 0)
			return w + gldns_str_print(s, slen, ";Error missing RR\n");
		w += print_remainder_hex(";Error partial RR 0x", d, dlen, s, slen);
		return w + gldns_str_print(s, slen, "\n");
	}
	rrtype = gldns_read_uint16(*d);
	w += gldns_rr_tcttl_scan(d, dlen, s, slen);
	w += gldns_str_print(s, slen, "\t");

	/* rdata */
	if(*dlen < 2) {
		if(*dlen == 0)
			return w + gldns_str_print(s, slen, ";Error missing rdatalen\n");
		w += print_remainder_hex(";Error missing rdatalen 0x",
			d, dlen, s, slen);
		return w + gldns_str_print(s, slen, "\n");
	}
	rdlen = gldns_read_uint16(*d);
	ordlen = rdlen;
	(*d)+=2;
	(*dlen)-=2;
	if(*dlen < rdlen) {
		w += gldns_str_print(s, slen, "\\# %u ", (unsigned)rdlen);
		if(*dlen == 0)
			return w + gldns_str_print(s, slen, ";Error missing rdata\n");
		w += print_remainder_hex(";Error partial rdata 0x", d, dlen, s, slen);
		return w + gldns_str_print(s, slen, "\n");
	}
	w += gldns_wire2str_rdata_scan(d, &rdlen, s, slen, rrtype, pkt, pktlen,
		comprloop);
	(*dlen) -= (ordlen-rdlen);

	/* default comment */
	w += gldns_wire2str_rr_comment_print(s, slen, rr, rrlen, dname_off,
		rrtype);
	w += gldns_str_print(s, slen, "\n");
	return w;
}

int gldns_wire2str_rrquestion_scan(uint8_t** d, size_t* dlen, char** s,
	size_t* slen, uint8_t* pkt, size_t pktlen, int* comprloop)
{
	int w = 0;
	uint16_t t, c;
	w += gldns_wire2str_dname_scan(d, dlen, s, slen, pkt, pktlen, comprloop);
	w += gldns_str_print(s, slen, "\t");
	if(*dlen < 4) {
		if(*dlen == 0)
			return w + gldns_str_print(s, slen, "Error malformed\n");
		w += print_remainder_hex("Error malformed 0x", d, dlen, s, slen);
		return w + gldns_str_print(s, slen, "\n");
	}
	t = gldns_read_uint16(*d);
	c = gldns_read_uint16((*d)+2);
	(*d)+=4;
	(*dlen)-=4;
	w += gldns_wire2str_class_print(s, slen, c);
	w += gldns_str_print(s, slen, "\t");
	w += gldns_wire2str_type_print(s, slen, t);
	w += gldns_str_print(s, slen, "\n");
	return w;
}

int gldns_wire2str_rr_unknown_scan(uint8_t** d, size_t* dlen, char** s,
	size_t* slen, uint8_t* pkt, size_t pktlen, int* comprloop)
{
	size_t rdlen, ordlen;
	int w = 0;
	w += gldns_wire2str_dname_scan(d, dlen, s, slen, pkt, pktlen, comprloop);
	w += gldns_str_print(s, slen, "\t");
	w += gldns_rr_tcttl_scan(d, dlen, s, slen);
	w += gldns_str_print(s, slen, "\t");
	if(*dlen < 2) {
		if(*dlen == 0)
			return w + gldns_str_print(s, slen, ";Error missing rdatalen\n");
		w += print_remainder_hex(";Error missing rdatalen 0x",
			d, dlen, s, slen);
		return w + gldns_str_print(s, slen, "\n");
	}
	rdlen = gldns_read_uint16(*d);
	ordlen = rdlen;
	(*d) += 2;
	(*dlen) -= 2;
	if(*dlen < rdlen) {
		w += gldns_str_print(s, slen, "\\# %u ", (unsigned)rdlen);
		if(*dlen == 0)
			return w + gldns_str_print(s, slen, ";Error missing rdata\n");
		w += print_remainder_hex(";Error partial rdata 0x", d, dlen, s, slen);
		return w + gldns_str_print(s, slen, "\n");
	}
	w += gldns_wire2str_rdata_unknown_scan(d, &rdlen, s, slen);
	(*dlen) -= (ordlen-rdlen);
	w += gldns_str_print(s, slen, "\n");
	return w;
}

/** print rr comment for type DNSKEY */
static int rr_comment_dnskey(char** s, size_t* slen, uint8_t* rr,
	size_t rrlen, size_t dname_off)
{
	size_t rdlen;
	uint8_t* rdata;
	int flags, w = 0;
	if(rrlen < dname_off + 10) return 0;
	rdlen = gldns_read_uint16(rr+dname_off+8);
	if(rrlen < dname_off + 10 + rdlen) return 0;
	if(rdlen < 2) return 0;
	rdata = rr + dname_off + 10;
	flags = (int)gldns_read_uint16(rdata);
	w += gldns_str_print(s, slen, " ;{");

	/* id */
	w += gldns_str_print(s, slen, "id = %u",
		gldns_calc_keytag_raw(rdata, rdlen));

	/* flags */
	if((flags&GLDNS_KEY_ZONE_KEY)) {
		if((flags&GLDNS_KEY_SEP_KEY))
			w += gldns_str_print(s, slen, " (ksk)");
		else 	w += gldns_str_print(s, slen, " (zsk)");
	}

	/* keysize */
	if(rdlen > 4) {
		w += gldns_str_print(s, slen, ", ");
		w += gldns_str_print(s, slen, "size = %db",
			(int)gldns_rr_dnskey_key_size_raw(
			(unsigned char*)rdata+4, rdlen-4, (int)(rdata[3])));
	}

	w += gldns_str_print(s, slen, "}");
	return w;
}

/** print rr comment for type RRSIG */
static int rr_comment_rrsig(char** s, size_t* slen, uint8_t* rr,
	size_t rrlen, size_t dname_off)
{
	size_t rdlen;
	uint8_t* rdata;
	if(rrlen < dname_off + 10) return 0;
	rdlen = gldns_read_uint16(rr+dname_off+8);
	if(rrlen < dname_off + 10 + rdlen) return 0;
	rdata = rr + dname_off + 10;
	if(rdlen < 18) return 0;
	return gldns_str_print(s, slen, " ;{id = %d}",
		(int)gldns_read_uint16(rdata+16));
}

/** print rr comment for type NSEC3 */
static int rr_comment_nsec3(char** s, size_t* slen, uint8_t* rr,
	size_t rrlen, size_t dname_off)
{
	size_t rdlen;
	uint8_t* rdata;
	int w = 0;
	if(rrlen < dname_off + 10) return 0;
	rdlen = gldns_read_uint16(rr+dname_off+8);
	if(rrlen < dname_off + 10 + rdlen) return 0;
	rdata = rr + dname_off + 10;
	if(rdlen < 2) return 0;
	if((rdata[1] & GLDNS_NSEC3_VARS_OPTOUT_MASK))
		w += gldns_str_print(s, slen, " ;{flags: optout}");
	return w;
}

int gldns_wire2str_rr_comment_print(char** s, size_t* slen, uint8_t* rr,
	size_t rrlen, size_t dname_off, uint16_t rrtype)
{
	if(rrtype == GLDNS_RR_TYPE_DNSKEY) {
		return rr_comment_dnskey(s, slen, rr, rrlen, dname_off);
	} else if(rrtype == GLDNS_RR_TYPE_RRSIG) {
		return rr_comment_rrsig(s, slen, rr, rrlen, dname_off);
	} else if(rrtype == GLDNS_RR_TYPE_NSEC3) {
		return rr_comment_nsec3(s, slen, rr, rrlen, dname_off);
	}
	return 0;
}

int gldns_wire2str_header_scan(uint8_t** d, size_t* dlen, char** s,
	size_t* slen)
{
	int w = 0;
	int opcode, rcode;
	w += gldns_str_print(s, slen, ";; ->>HEADER<<- ");
	if(*dlen == 0)
		return w+gldns_str_print(s, slen, "Error empty packet");
	if(*dlen < 4)
		return w+print_remainder_hex("Error header too short 0x", d, dlen, s, slen);
	opcode = (int)GLDNS_OPCODE_WIRE(*d);
	rcode = (int)GLDNS_RCODE_WIRE(*d);
	w += gldns_str_print(s, slen, "opcode: ");
	w += gldns_wire2str_opcode_print(s, slen, opcode);
	w += gldns_str_print(s, slen, ", ");
	w += gldns_str_print(s, slen, "rcode: ");
	w += gldns_wire2str_rcode_print(s, slen, rcode);
	w += gldns_str_print(s, slen, ", ");
	w += gldns_str_print(s, slen, "id: %d\n", (int)GLDNS_ID_WIRE(*d));
	w += gldns_str_print(s, slen, ";; flags:");
	if(GLDNS_QR_WIRE(*d)) w += gldns_str_print(s, slen, " qr");
	if(GLDNS_AA_WIRE(*d)) w += gldns_str_print(s, slen, " aa");
	if(GLDNS_TC_WIRE(*d)) w += gldns_str_print(s, slen, " tc");
	if(GLDNS_RD_WIRE(*d)) w += gldns_str_print(s, slen, " rd");
	if(GLDNS_CD_WIRE(*d)) w += gldns_str_print(s, slen, " cd");
	if(GLDNS_RA_WIRE(*d)) w += gldns_str_print(s, slen, " ra");
	if(GLDNS_AD_WIRE(*d)) w += gldns_str_print(s, slen, " ad");
	if(GLDNS_Z_WIRE(*d))  w += gldns_str_print(s, slen, " z");
	w += gldns_str_print(s, slen, " ; ");
	if(*dlen < GLDNS_HEADER_SIZE)
		return w+print_remainder_hex("Error header too short 0x", d, dlen, s, slen);
	w += gldns_str_print(s, slen, "QUERY: %d, ", (int)GLDNS_QDCOUNT(*d));
	w += gldns_str_print(s, slen, "ANSWER: %d, ", (int)GLDNS_ANCOUNT(*d));
	w += gldns_str_print(s, slen, "AUTHORITY: %d, ", (int)GLDNS_NSCOUNT(*d));
	w += gldns_str_print(s, slen, "ADDITIONAL: %d ", (int)GLDNS_ARCOUNT(*d));
	*d += GLDNS_HEADER_SIZE;
	*dlen -= GLDNS_HEADER_SIZE;
	return w;
}

int gldns_wire2str_rdata_scan(uint8_t** d, size_t* dlen, char** s,
	size_t* slen, uint16_t rrtype, uint8_t* pkt, size_t pktlen,
	int* comprloop)
{
	/* try to prettyprint, but if that fails, use unknown format */
	uint8_t* origd = *d;
	char* origs = *s;
	size_t origdlen = *dlen, origslen = *slen;
	size_t r_cnt, r_max;
	gldns_rdf_type rdftype;
	int w = 0, n;

	const gldns_rr_descriptor *desc = gldns_rr_descript(rrtype);
	if(!desc) /* unknown format */
		return gldns_wire2str_rdata_unknown_scan(d, dlen, s, slen);
	/* dlen equals the rdatalen for the rdata */

	r_max = gldns_rr_descriptor_maximum(desc);
	for(r_cnt=0; r_cnt < r_max; r_cnt++) {
		if(*dlen == 0) {
			if(r_cnt < gldns_rr_descriptor_minimum(desc))
				goto failed;
			break; /* nothing more to print */
		}
		rdftype = gldns_rr_descriptor_field_type(desc, r_cnt);
		if(r_cnt != 0)
			w += gldns_str_print(s, slen, " ");
		n = gldns_wire2str_rdf_scan(d, dlen, s, slen, rdftype,
			pkt, pktlen, comprloop);
		if(n == -1) {
		failed:
			/* failed, use unknown format */
			*d = origd; *s = origs;
			*dlen = origdlen; *slen = origslen;
			return gldns_wire2str_rdata_unknown_scan(d, dlen,
				s, slen);
		}
		w += n;
	}
	if(*dlen != 0) {
		goto failed;
	}
	return w;
}

int gldns_wire2str_rdata_unknown_scan(uint8_t** d, size_t* dlen, char** s,
	size_t* slen)
{
	int w = 0;

	/* print length */
	w += gldns_str_print(s, slen, "\\# %u", (unsigned)*dlen);

	/* print rdlen in hex */
	if(*dlen != 0)
		w += gldns_str_print(s, slen, " ");
	w += print_hex_buf(s, slen, *d, *dlen);
	(*d) += *dlen;
	(*dlen) = 0;
	return w;
}

/** print and escape one character for a domain dname */
static int dname_char_print(char** s, size_t* slen, uint8_t c)
{
	if(c == '.' || c == ';' || c == '(' || c == ')' || c == '\\')
		return gldns_str_print(s, slen, "\\%c", c);
	else if(!(isascii((unsigned char)c) && isgraph((unsigned char)c)))
		return gldns_str_print(s, slen, "\\%03u", (unsigned)c);
	/* plain printout */
	if(*slen) {
		**s = (char)c;
		(*s)++;
		(*slen)--;
	}
	return 1;
}

int gldns_wire2str_dname_scan(uint8_t** d, size_t* dlen, char** s, size_t* slen,
	uint8_t* pkt, size_t pktlen, int* comprloop)
{
	int w = 0;
	/* spool labels onto the string, use compression if its there */
	uint8_t* pos = *d;
	unsigned i, counter=0;
	unsigned maxcompr = 1000; /* loop detection, max compr ptrs */
	int in_buf = 1;
	if(comprloop) {
		if(*comprloop != 0)
			maxcompr = 30; /* for like ipv6 reverse name, per label */
		if(*comprloop > 4)
			maxcompr = 4; /* just don't want to spend time, any more */
	}
	if(*dlen == 0) return gldns_str_print(s, slen, "ErrorMissingDname");
	if(*pos == 0) {
		(*d)++;
		(*dlen)--;
		return gldns_str_print(s, slen, ".");
	}
	while((!pkt || pos < pkt+pktlen) && *pos) {
		/* read label length */
		uint8_t labellen = *pos++;
		if(in_buf) { (*d)++; (*dlen)--; }

		/* find out what sort of label we have */
		if((labellen&0xc0) == 0xc0) {
			/* compressed */
			uint16_t target = 0;
			if(in_buf && *dlen == 0)
				return w + gldns_str_print(s, slen,
					"ErrorPartialDname");
			else if(!in_buf && pos+1 > pkt+pktlen)
				return w + gldns_str_print(s, slen,
					"ErrorPartialDname");
			target = ((labellen&0x3f)<<8) | *pos;
			if(in_buf) { (*d)++; (*dlen)--; }
			/* move to target, if possible */
			if(!pkt || target >= pktlen)
				return w + gldns_str_print(s, slen,
					"ErrorComprPtrOutOfBounds");
			if(counter++ > maxcompr)
				return w + gldns_str_print(s, slen,
					"ErrorComprPtrLooped");
			in_buf = 0;
			pos = pkt+target;
			continue;
		} else if((labellen&0xc0)) {
			/* notimpl label type */
			w += gldns_str_print(s, slen,
				"ErrorLABELTYPE%xIsUnknown",
				(int)(labellen&0xc0));
			return w;
		}

		/* spool label characters, end with '.' */
		if(in_buf && *dlen < (size_t)labellen)
			labellen = (uint8_t)*dlen;
		else if(!in_buf && pos+(size_t)labellen > pkt+pktlen)
			labellen = (uint8_t)(pkt + pktlen - pos);
		for(i=0; i<(unsigned)labellen; i++) {
			w += dname_char_print(s, slen, *pos++);
		}
		if(in_buf) {
			(*d) += labellen;
			(*dlen) -= labellen;
			if(*dlen == 0) break;
		}
		w += gldns_str_print(s, slen, ".");
	}
	/* skip over final root label */
	if(in_buf && *dlen > 0) { (*d)++; (*dlen)--; }
	/* in case we printed no labels, terminate dname */
	if(w == 0) w += gldns_str_print(s, slen, ".");
	return w;
}

int gldns_wire2str_opcode_print(char** s, size_t* slen, int opcode)
{
	gldns_lookup_table *lt = gldns_lookup_by_id(gldns_opcodes, opcode);
	if (lt && lt->name) {
		return gldns_str_print(s, slen, "%s", lt->name);
	}
	return gldns_str_print(s, slen, "OPCODE%u", (unsigned)opcode);
}

int gldns_wire2str_rcode_print(char** s, size_t* slen, int rcode)
{
	gldns_lookup_table *lt = gldns_lookup_by_id(gldns_rcodes, rcode);
	if (lt && lt->name) {
		return gldns_str_print(s, slen, "%s", lt->name);
	}
	return gldns_str_print(s, slen, "RCODE%u", (unsigned)rcode);
}

int gldns_wire2str_class_print(char** s, size_t* slen, uint16_t rrclass)
{
	gldns_lookup_table *lt = gldns_lookup_by_id(gldns_rr_classes,
		(int)rrclass);
	if (lt && lt->name) {
		return gldns_str_print(s, slen, "%s", lt->name);
	}
	return gldns_str_print(s, slen, "CLASS%u", (unsigned)rrclass);
}

int gldns_wire2str_type_print(char** s, size_t* slen, uint16_t rrtype)
{
	const gldns_rr_descriptor *descriptor = gldns_rr_descript(rrtype);
	if (descriptor && descriptor->_name) {
		return gldns_str_print(s, slen, "%s", descriptor->_name);
	}
	return gldns_str_print(s, slen, "TYPE%u", (unsigned)rrtype);
}

int gldns_wire2str_edns_option_code_print(char** s, size_t* slen,
	uint16_t opcode)
{
	gldns_lookup_table *lt = gldns_lookup_by_id(gldns_edns_options,
		(int)opcode);
	if (lt && lt->name) {
		return gldns_str_print(s, slen, "%s", lt->name);
	}
	return gldns_str_print(s, slen, "OPT%u", (unsigned)opcode);
}

int gldns_wire2str_class_scan(uint8_t** d, size_t* dlen, char** s, size_t* slen)
{
	uint16_t c;
	if(*dlen == 0) return 0;
	if(*dlen < 2) return print_remainder_hex("Error malformed 0x", d, dlen, s, slen);
	c = gldns_read_uint16(*d);
	(*d)+=2;
	(*dlen)-=2;
	return gldns_wire2str_class_print(s, slen, c);
}

int gldns_wire2str_type_scan(uint8_t** d, size_t* dlen, char** s, size_t* slen)
{
	uint16_t t;
	if(*dlen == 0) return 0;
	if(*dlen < 2) return print_remainder_hex("Error malformed 0x", d, dlen, s, slen);
	t = gldns_read_uint16(*d);
	(*d)+=2;
	(*dlen)-=2;
	return gldns_wire2str_type_print(s, slen, t);
}

int gldns_wire2str_ttl_scan(uint8_t** d, size_t* dlen, char** s, size_t* slen)
{
	uint32_t ttl;
	if(*dlen == 0) return 0;
	if(*dlen < 4) return print_remainder_hex("Error malformed 0x", d, dlen, s, slen);
	ttl = gldns_read_uint32(*d);
	(*d)+=4;
	(*dlen)-=4;
	return gldns_str_print(s, slen, "%u", (unsigned)ttl);
}

int gldns_wire2str_rdf_scan(uint8_t** d, size_t* dlen, char** s, size_t* slen,
	int rdftype, uint8_t* pkt, size_t pktlen, int* comprloop)
{
	if(*dlen == 0) return 0;
	switch(rdftype) {
	case GLDNS_RDF_TYPE_NONE:
		return 0;
	case GLDNS_RDF_TYPE_DNAME:
		return gldns_wire2str_dname_scan(d, dlen, s, slen, pkt, pktlen, comprloop);
	case GLDNS_RDF_TYPE_INT8:
		return gldns_wire2str_int8_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_INT16:
		return gldns_wire2str_int16_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_INT32:
		return gldns_wire2str_int32_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_PERIOD:
		return gldns_wire2str_period_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_TSIGTIME:
		return gldns_wire2str_tsigtime_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_A:
		return gldns_wire2str_a_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_AAAA:
		return gldns_wire2str_aaaa_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_STR:
		return gldns_wire2str_str_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_APL:
		return gldns_wire2str_apl_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_B32_EXT:
		return gldns_wire2str_b32_ext_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_B64:
		return gldns_wire2str_b64_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_HEX:
		return gldns_wire2str_hex_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_NSEC:
		return gldns_wire2str_nsec_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_NSEC3_SALT:
		return gldns_wire2str_nsec3_salt_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_TYPE:
		return gldns_wire2str_type_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_CLASS:
		return gldns_wire2str_class_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_CERT_ALG:
		return gldns_wire2str_cert_alg_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_ALG:
		return gldns_wire2str_alg_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_UNKNOWN:
		return gldns_wire2str_unknown_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_TIME:
		return gldns_wire2str_time_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_LOC:
		return gldns_wire2str_loc_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_WKS:
	case GLDNS_RDF_TYPE_SERVICE:
		return gldns_wire2str_wks_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_NSAP:
		return gldns_wire2str_nsap_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_ATMA:
		return gldns_wire2str_atma_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_IPSECKEY:
		return gldns_wire2str_ipseckey_scan(d, dlen, s, slen, pkt,
			pktlen, comprloop);
	case GLDNS_RDF_TYPE_HIP:
		return gldns_wire2str_hip_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_INT16_DATA:
		return gldns_wire2str_int16_data_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_NSEC3_NEXT_OWNER:
		return gldns_wire2str_b32_ext_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_ILNP64:
		return gldns_wire2str_ilnp64_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_EUI48:
		return gldns_wire2str_eui48_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_EUI64:
		return gldns_wire2str_eui64_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_TAG:
		return gldns_wire2str_tag_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_LONG_STR:
		return gldns_wire2str_long_str_scan(d, dlen, s, slen);
	case GLDNS_RDF_TYPE_AMTRELAY:
		return gldns_wire2str_amtrelay_scan(d, dlen, s, slen, pkt,
			pktlen, comprloop);
	case GLDNS_RDF_TYPE_TSIGERROR:
		return gldns_wire2str_tsigerror_scan(d, dlen, s, slen);
	}
	/* unknown rdf type */
	return -1;
}

int gldns_wire2str_int8_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	if(*dl < 1) return -1;
	w = gldns_str_print(s, sl, "%u", (unsigned)**d);
	(*d)++;
	(*dl)--;
	return w;
}

int gldns_wire2str_int16_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	if(*dl < 2) return -1;
	w = gldns_str_print(s, sl, "%lu", (unsigned long)gldns_read_uint16(*d));
	(*d)+=2;
	(*dl)-=2;
	return w;
}

int gldns_wire2str_int32_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	if(*dl < 4) return -1;
	w = gldns_str_print(s, sl, "%lu", (unsigned long)gldns_read_uint32(*d));
	(*d)+=4;
	(*dl)-=4;
	return w;
}

int gldns_wire2str_period_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	if(*dl < 4) return -1;
	w = gldns_str_print(s, sl, "%u", (unsigned)gldns_read_uint32(*d));
	(*d)+=4;
	(*dl)-=4;
	return w;
}

int gldns_wire2str_tsigtime_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	/* tsigtime is 48 bits network order unsigned integer */
	int w;
	uint64_t tsigtime = 0;
	uint64_t d0, d1, d2, d3, d4, d5;
	if(*dl < 6) return -1;
	d0 = (*d)[0]; /* cast to uint64 for shift operations */
	d1 = (*d)[1];
	d2 = (*d)[2];
	d3 = (*d)[3];
	d4 = (*d)[4];
	d5 = (*d)[5];
	tsigtime = (d0<<40) | (d1<<32) | (d2<<24) | (d3<<16) | (d4<<8) | d5;
#ifndef USE_WINSOCK
	w = gldns_str_print(s, sl, "%llu", (long long)tsigtime);
#else
	w = gldns_str_print(s, sl, "%I64u", (long long)tsigtime);
#endif
	(*d)+=6;
	(*dl)-=6;
	return w;
}

int gldns_wire2str_a_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	char buf[32];
	int w;
	if(*dl < 4) return -1;
	if(!inet_ntop(AF_INET, *d, buf, (socklen_t)sizeof(buf)))
		return -1;
	w = gldns_str_print(s, sl, "%s", buf);
	(*d)+=4;
	(*dl)-=4;
	return w;
}

int gldns_wire2str_aaaa_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
#ifdef AF_INET6
	char buf[64];
	int w;
	if(*dl < 16) return -1;
	if(!inet_ntop(AF_INET6, *d, buf, (socklen_t)sizeof(buf)))
		return -1;
	w = gldns_str_print(s, sl, "%s", buf);
	(*d)+=16;
	(*dl)-=16;
	return w;
#else
	return -1;
#endif
}

/** printout escaped TYPE_STR character */
static int str_char_print(char** s, size_t* sl, uint8_t c)
{
	if(isprint((unsigned char)c) || c == '\t') {
		if(c == '\"' || c == '\\')
			return gldns_str_print(s, sl, "\\%c", c);
		if(*sl) {
			**s = (char)c;
			(*s)++;
			(*sl)--;
		}
		return 1;
	}
	return gldns_str_print(s, sl, "\\%03u", (unsigned)c);
}

int gldns_wire2str_str_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w = 0;
	size_t i, len;
	if(*dl < 1) return -1;
	len = **d;
	if(*dl < 1+len) return -1;
	(*d)++;
	(*dl)--;
	w += gldns_str_print(s, sl, "\"");
	for(i=0; i<len; i++)
		w += str_char_print(s, sl, (*d)[i]);
	w += gldns_str_print(s, sl, "\"");
	(*d)+=len;
	(*dl)-=len;
	return w;
}

int gldns_wire2str_apl_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int i, w = 0;
	uint16_t family;
	uint8_t negation, prefix, adflength;
	if(*dl < 4) return -1;
	family = gldns_read_uint16(*d);
	prefix = (*d)[2];
	negation = ((*d)[3] & GLDNS_APL_NEGATION);
	adflength = ((*d)[3] & GLDNS_APL_MASK);
	if(*dl < 4+(size_t)adflength) return -1;
	if(family != GLDNS_APL_IP4 && family != GLDNS_APL_IP6)
		return -1; /* unknown address family */
	if(negation)
		w += gldns_str_print(s, sl, "!");
	w += gldns_str_print(s, sl, "%u:", (unsigned)family);
	if(family == GLDNS_APL_IP4) {
		/* check if prefix <32 ? */
		/* address is variable length 0 - 4 */
		for(i=0; i<4; i++) {
			if(i > 0)
				w += gldns_str_print(s, sl, ".");
			if(i < (int)adflength)
				w += gldns_str_print(s, sl, "%d", (*d)[4+i]);
			else	w += gldns_str_print(s, sl, "0");
		}
	} else if(family == GLDNS_APL_IP6) {
		/* check if prefix <128 ? */
		/* address is variable length 0 - 16 */
		for(i=0; i<16; i++) {
			if(i%2 == 0 && i>0)
				w += gldns_str_print(s, sl, ":");
			if(i < (int)adflength)
				w += gldns_str_print(s, sl, "%02x", (*d)[4+i]);
			else	w += gldns_str_print(s, sl, "00");
		}
	}
	w += gldns_str_print(s, sl, "/%u", (unsigned)prefix);
	(*d) += 4+adflength;
	(*dl) -= 4+adflength;
	return w;
}

int gldns_wire2str_b32_ext_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	size_t datalen;
	size_t sz;
	if(*dl < 1) return -1;
	datalen = (*d)[0];
	if(*dl < 1+datalen) return -1;
	sz = gldns_b32_ntop_calculate_size(datalen);
	if(*sl < sz+1) {
		(*d) += datalen+1;
		(*dl) -= (datalen+1);
		return (int)sz; /* out of space really, but would need buffer
			in order to truncate the output */
	}
	gldns_b32_ntop_extended_hex((*d)+1, datalen, *s, *sl);
	(*d) += datalen+1;
	(*dl) -= (datalen+1);
	(*s) += sz;
	(*sl) -= sz;
	return (int)sz;
}

/** scan number of bytes from wire into b64 presentation format */
static int gldns_wire2str_b64_scan_num(uint8_t** d, size_t* dl, char** s,
	size_t* sl, size_t num)
{
	/* b64_ntop_calculate size includes null at the end */
	size_t sz = gldns_b64_ntop_calculate_size(num)-1;
	if(*sl < sz+1) {
		(*d) += num;
		(*dl) -= num;
		return (int)sz; /* out of space really, but would need buffer
			in order to truncate the output */
	}
	gldns_b64_ntop(*d, num, *s, *sl);
	(*d) += num;
	(*dl) -= num;
	(*s) += sz;
	(*sl) -= sz;
	return (int)sz;
}

int gldns_wire2str_b64_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	if(*dl == 0) {
		return gldns_str_print(s, sl, "0");
	}
	return gldns_wire2str_b64_scan_num(d, dl, s, sl, *dl);
}

int gldns_wire2str_hex_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	if(*dl == 0) {
		return gldns_str_print(s, sl, "0");
	}
	return print_remainder_hex("", d, dl, s, sl);
}

int gldns_wire2str_nsec_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	uint8_t* p = *d;
	size_t pl = *dl;
	unsigned i, bit, window, block_len;
	uint16_t t;
	int w = 0;
	
	/* check for errors */
	while(pl) {
		if(pl < 2) return -1;
		block_len = (unsigned)p[1];
		if(pl < 2+block_len) return -1;
		p += block_len+2;
		pl -= block_len+2;
	}

	/* do it */
	p = *d;
	pl = *dl;
	while(pl) {
		if(pl < 2) return -1; /* cannot happen */
		window = (unsigned)p[0];
		block_len = (unsigned)p[1];
		if(pl < 2+block_len) return -1; /* cannot happen */
		p += 2;
		for(i=0; i<block_len; i++) {
			if(p[i] == 0) continue;
			/* base type number for this octet */
			t = ((window)<<8) | (i << 3);
			for(bit=0; bit<8; bit++) {
				if((p[i]&(0x80>>bit))) {
					if(w) w += gldns_str_print(s, sl, " ");
					w += gldns_wire2str_type_print(s, sl,
						t+bit);
				}
			}
		}
		p += block_len;
		pl -= block_len+2;
	}
	(*d) += *dl;
	(*dl) = 0;
	return w;
}

int gldns_wire2str_nsec3_salt_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	size_t salt_len;
	int w;
	if(*dl < 1) return -1;
	salt_len = (size_t)(*d)[0];
	if(*dl < 1+salt_len) return -1;
	(*d)++;
	(*dl)--;
	if(salt_len == 0) {
		return gldns_str_print(s, sl, "-");
	}
	w = print_hex_buf(s, sl, *d, salt_len);
	(*dl)-=salt_len;
	(*d)+=salt_len;
	return w;
}

int gldns_wire2str_cert_alg_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	gldns_lookup_table *lt;
	int data, w;
	if(*dl < 2) return -1;
	data = (int)gldns_read_uint16(*d);
	lt = gldns_lookup_by_id(gldns_cert_algorithms, data);
	if(lt && lt->name)
		w = gldns_str_print(s, sl, "%s", lt->name);
	else 	w = gldns_str_print(s, sl, "%d", data);
	(*dl)-=2;
	(*d)+=2;
	return w;
}

int gldns_wire2str_alg_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	/* don't use algorithm mnemonics in the presentation format
	 * this kind of got sneaked into the rfc's */
	return gldns_wire2str_int8_scan(d, dl, s, sl);
}

int gldns_wire2str_unknown_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	return gldns_wire2str_rdata_unknown_scan(d, dl, s, sl);
}

int gldns_wire2str_time_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	/* create a YYYYMMDDHHMMSS string if possible */
	struct tm tm;
	char date_buf[16];
	uint32_t t;
	memset(&tm, 0, sizeof(tm));
	if(*dl < 4) return -1;
	t = gldns_read_uint32(*d);
	date_buf[15]=0;
	if(gldns_serial_arithmetics_gmtime_r(t, time(NULL), &tm) &&
		strftime(date_buf, 15, "%Y%m%d%H%M%S", &tm)) {
		(*d) += 4;
		(*dl) -= 4;
		return gldns_str_print(s, sl, "%s", date_buf);
	}
	return -1;
}

static int
loc_cm_print(char** str, size_t* sl, uint8_t mantissa, uint8_t exponent)
{
	int w = 0;
	uint8_t i;
	/* is it 0.<two digits> ? */
	if(exponent < 2) {
		if(exponent == 1)
			mantissa *= 10;
		return gldns_str_print(str, sl, "0.%02ld", (long)mantissa);
	}
	/* always <digit><string of zeros> */
	w += gldns_str_print(str, sl, "%d", (int)mantissa);
	for(i=0; i<exponent-2; i++)
		w += gldns_str_print(str, sl, "0");
	return w;
}

int gldns_wire2str_loc_scan(uint8_t** d, size_t* dl, char** str, size_t* sl)
{
	/* we could do checking (ie degrees < 90 etc)? */
	uint8_t version;
	uint8_t size;
	uint8_t horizontal_precision;
	uint8_t vertical_precision;
	uint32_t longitude;
	uint32_t latitude;
	uint32_t altitude;
	char northerness;
	char easterness;
	uint32_t h;
	uint32_t m;
	double s;
	uint32_t equator = (uint32_t)1 << 31; /* 2**31 */
	int w = 0;

	if(*dl < 16) return -1;
	version = (*d)[0];
	if(version != 0)
		return gldns_wire2str_hex_scan(d, dl, str, sl);
	size = (*d)[1];
	horizontal_precision = (*d)[2];
	vertical_precision = (*d)[3];

	latitude = gldns_read_uint32((*d)+4);
	longitude = gldns_read_uint32((*d)+8);
	altitude = gldns_read_uint32((*d)+12);

	if (latitude > equator) {
		northerness = 'N';
		latitude = latitude - equator;
	} else {
		northerness = 'S';
		latitude = equator - latitude;
	}
	h = latitude / (1000 * 60 * 60);
	latitude = latitude % (1000 * 60 * 60);
	m = latitude / (1000 * 60);
	latitude = latitude % (1000 * 60);
	s = (double) latitude / 1000.0;
	w += gldns_str_print(str, sl, "%02u %02u %06.3f %c ",
		h, m, s, northerness);

	if (longitude > equator) {
		easterness = 'E';
		longitude = longitude - equator;
	} else {
		easterness = 'W';
		longitude = equator - longitude;
	}
	h = longitude / (1000 * 60 * 60);
	longitude = longitude % (1000 * 60 * 60);
	m = longitude / (1000 * 60);
	longitude = longitude % (1000 * 60);
	s = (double) longitude / (1000.0);
	w += gldns_str_print(str, sl, "%02u %02u %06.3f %c ",
		h, m, s, easterness);

	s = ((double) altitude) / 100;
	s -= 100000;

	if(altitude%100 != 0)
		w += gldns_str_print(str, sl, "%.2f", s);
	else
		w += gldns_str_print(str, sl, "%.0f", s);

	w += gldns_str_print(str, sl, "m ");

	w += loc_cm_print(str, sl, (size & 0xf0) >> 4, size & 0x0f);
	w += gldns_str_print(str, sl, "m ");

	w += loc_cm_print(str, sl, (horizontal_precision & 0xf0) >> 4,
		horizontal_precision & 0x0f);
	w += gldns_str_print(str, sl, "m ");

	w += loc_cm_print(str, sl, (vertical_precision & 0xf0) >> 4,
		vertical_precision & 0x0f);
	w += gldns_str_print(str, sl, "m");

	(*d)+=16;
	(*dl)-=16;
	return w;
}

int gldns_wire2str_wks_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	/* protocol, followed by bitmap of services */
	const char* proto_name = NULL;
	struct protoent *protocol;
	struct servent *service;
	uint8_t protocol_nr;
	int bit, port, w = 0;
	size_t i;
	/* we cannot print with strings because they
	 * are not portable, the presentation format may
	 * not be able to be read in on another computer.  */
	int print_symbols = 0;

	/* protocol */
	if(*dl < 1) return -1;
	protocol_nr = (*d)[0];
	(*d)++;
	(*dl)--;
	protocol = getprotobynumber((int)protocol_nr);
	if(protocol && (protocol->p_name != NULL)) {
		w += gldns_str_print(s, sl, "%s", protocol->p_name);
		proto_name = protocol->p_name;
	} else if(protocol_nr == 6) {
		w += gldns_str_print(s, sl, "tcp");
	} else if(protocol_nr == 17) {
		w += gldns_str_print(s, sl, "udp");
	} else	{
		w += gldns_str_print(s, sl, "%u", (unsigned)protocol_nr);
	}

	for(i=0; i<*dl; i++) {
		if((*d)[i] == 0)
			continue;
		for(bit=0; bit<8; bit++) {
			if(!(((*d)[i])&(0x80>>bit)))
				continue;
			port = (int)i*8 + bit;

			if(!print_symbols)
				service = NULL;
			else
				service = getservbyport(
					(int)htons((uint16_t)port), proto_name);
			if(service && service->s_name)
				w += gldns_str_print(s, sl, " %s",
					service->s_name);
			else 	w += gldns_str_print(s, sl, " %u",
					(unsigned)port);
		}
	}

#ifdef HAVE_ENDSERVENT
	endservent();
#endif
#ifdef HAVE_ENDPROTOENT
        endprotoent();
#endif
	(*d) += *dl;
	(*dl) = 0;
	return w;
}

int gldns_wire2str_nsap_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	return print_remainder_hex("0x", d, dl, s, sl);
}

int gldns_wire2str_atma_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	return print_remainder_hex("", d, dl, s, sl);
}

/* internal scan routine that can modify arguments on failure */
static int gldns_wire2str_ipseckey_scan_internal(uint8_t** d, size_t* dl,
	char** s, size_t* sl, uint8_t* pkt, size_t pktlen, int* comprloop)
{
	/* http://www.ietf.org/internet-drafts/draft-ietf-ipseckey-rr-12.txt*/
	uint8_t precedence, gateway_type, algorithm;
	int w = 0;

	if(*dl < 3) return -1;
	precedence = (*d)[0];
	gateway_type = (*d)[1];
	algorithm = (*d)[2];
	if(gateway_type > 3)
		return -1; /* unknown */
	(*d)+=3;
	(*dl)-=3;
	w += gldns_str_print(s, sl, "%d %d %d ",
		(int)precedence, (int)gateway_type, (int)algorithm);

	switch(gateway_type) {
	case 0: /* no gateway */
		w += gldns_str_print(s, sl, ".");
		break;
	case 1: /* ip4 */
		w += gldns_wire2str_a_scan(d, dl, s, sl);
		break;
	case 2: /* ip6 */
		w += gldns_wire2str_aaaa_scan(d, dl, s, sl);
		break;
	case 3: /* dname */
		w += gldns_wire2str_dname_scan(d, dl, s, sl, pkt, pktlen, comprloop);
		break;
	default: /* unknown */
		return -1;
	}

	if(*dl < 1)
		return -1;
	w += gldns_str_print(s, sl, " ");
	w += gldns_wire2str_b64_scan_num(d, dl, s, sl, *dl);
	return w;
}

int gldns_wire2str_ipseckey_scan(uint8_t** d, size_t* dl, char** s, size_t* sl,
	uint8_t* pkt, size_t pktlen, int* comprloop)
{
	uint8_t* od = *d;
	char* os = *s;
	size_t odl = *dl, osl = *sl;
	int w=gldns_wire2str_ipseckey_scan_internal(d, dl, s, sl, pkt, pktlen, comprloop);
	if(w == -1) {
		*d = od;
		*s = os;
		*dl = odl;
		*sl = osl;
		return -1;
	}
	return w;
}

int gldns_wire2str_hip_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	uint8_t algo, hitlen;
	uint16_t pklen;

	/* read lengths */
	if(*dl < 4)
		return -1;
	hitlen = (*d)[0];
	algo = (*d)[1];
	pklen = gldns_read_uint16((*d)+2);
	if(*dl < (size_t)4 + (size_t)hitlen + (size_t)pklen)
		return -1;

	/* write: algo hit pubkey */
	w = gldns_str_print(s, sl, "%u ", (unsigned)algo);
	w += print_hex_buf(s, sl, (*d)+4, hitlen);
	w += gldns_str_print(s, sl, " ");
	(*d)+=4+hitlen;
	(*dl)-= (4+hitlen);
	w += gldns_wire2str_b64_scan_num(d, dl, s, sl, pklen);
	return w;
}

int gldns_wire2str_int16_data_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	uint16_t n;
	if(*dl < 2)
		return -1;
	n = gldns_read_uint16(*d);
	if(*dl < 2+(size_t)n)
		return -1;
	(*d)+=2;
	(*dl)-=2;
	if(n == 0) {
		return gldns_str_print(s, sl, "0");
	}
	w = gldns_str_print(s, sl, "%u ", (unsigned)n);
	w += gldns_wire2str_b64_scan_num(d, dl, s, sl, n);
	return w;
}

int gldns_wire2str_nsec3_next_owner_scan(uint8_t** d, size_t* dl, char** s,
	size_t* sl)
{
	return gldns_wire2str_b32_ext_scan(d, dl, s, sl);
}

int gldns_wire2str_ilnp64_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	if(*dl < 8)
		return -1;
	w = gldns_str_print(s, sl, "%.4x:%.4x:%.4x:%.4x",
		gldns_read_uint16(*d), gldns_read_uint16((*d)+2),
		gldns_read_uint16((*d)+4), gldns_read_uint16((*d)+6));
	(*d)+=8;
	(*dl)-=8;
	return w;
}

int gldns_wire2str_eui48_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	if(*dl < 6)
		return -1;
	w = gldns_str_print(s, sl, "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x",
		(*d)[0], (*d)[1], (*d)[2], (*d)[3], (*d)[4], (*d)[5]);
	(*d)+=6;
	(*dl)-=6;
	return w;
}

int gldns_wire2str_eui64_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	int w;
	if(*dl < 8)
		return -1;
	w = gldns_str_print(s, sl, "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x-%.2x-%.2x",
		(*d)[0], (*d)[1], (*d)[2], (*d)[3], (*d)[4], (*d)[5],
		(*d)[6], (*d)[7]);
	(*d)+=8;
	(*dl)-=8;
	return w;
}

int gldns_wire2str_tag_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	size_t i, n;
	int w = 0;
	if(*dl < 1)
		return -1;
	n = (size_t)((*d)[0]);
	if(*dl < 1+n)
		return -1;
	for(i=0; i<n; i++)
		if(!isalnum((unsigned char)(*d)[i+1]))
			return -1;
	for(i=0; i<n; i++)
		w += gldns_str_print(s, sl, "%c", (char)(*d)[i+1]);
	(*d)+=n+1;
	(*dl)-=(n+1);
	return w;
}

int gldns_wire2str_long_str_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	size_t i;
	int w = 0;
	w += gldns_str_print(s, sl, "\"");
	for(i=0; i<*dl; i++)
		w += str_char_print(s, sl, (*d)[i]);
	w += gldns_str_print(s, sl, "\"");
	(*d)+=*dl;
	(*dl)=0;
	return w;
}

/* internal scan routine that can modify arguments on failure */
static int gldns_wire2str_amtrelay_scan_internal(uint8_t** d, size_t* dl,
	char** s, size_t* sl, uint8_t* pkt, size_t pktlen, int* comprloop)
{
	/* https://www.ietf.org/id/draft-ietf-mboned-driad-amt-discovery-01.txt */
	uint8_t precedence, discovery_optional, relay_type;
	int w = 0;

	if(*dl < 2) return -1;
	precedence = (*d)[0];
	discovery_optional= (*d)[1] >> 7;
	relay_type = (*d)[1] % 0x7F;
	if(relay_type > 3)
		return -1; /* unknown */
	(*d)+=2;
	(*dl)-=2;
	w += gldns_str_print(s, sl, "%d %d %d ",
		(int)precedence, (int)discovery_optional, (int)relay_type);

	switch(relay_type) {
	case 0: /* no relay */
		break;
	case 1: /* ip4 */
		w += gldns_wire2str_a_scan(d, dl, s, sl);
		break;
	case 2: /* ip6 */
		w += gldns_wire2str_aaaa_scan(d, dl, s, sl);
		break;
	case 3: /* dname */
		w += gldns_wire2str_dname_scan(d, dl, s, sl, pkt, pktlen, comprloop);
		break;
	default: /* unknown */
		return -1;
	}
	return w;
}

int gldns_wire2str_amtrelay_scan(uint8_t** d, size_t* dl, char** s, size_t* sl,
	uint8_t* pkt, size_t pktlen, int* comprloop)
{
	uint8_t* od = *d;
	char* os = *s;
	size_t odl = *dl, osl = *sl;
	int w=gldns_wire2str_amtrelay_scan_internal(d, dl, s, sl, pkt, pktlen, comprloop);
	if(w == -1) {
		*d = od;
		*s = os;
		*dl = odl;
		*sl = osl;
		return -1;
	}
	return w;
}


int gldns_wire2str_tsigerror_scan(uint8_t** d, size_t* dl, char** s, size_t* sl)
{
	gldns_lookup_table *lt;
	int data, w;
	if(*dl < 2) return -1;
	data = (int)gldns_read_uint16(*d);
	lt = gldns_lookup_by_id(gldns_tsig_errors, data);
	if(lt && lt->name)
		w = gldns_str_print(s, sl, "%s", lt->name);
	else 	w = gldns_str_print(s, sl, "%d", data);
	(*dl)-=2;
	(*d)+=2;
	return w;
}

int gldns_wire2str_edns_llq_print(char** s, size_t* sl, uint8_t* data,
	size_t len)
{
	/* LLQ constants */
	const char* llq_errors[] = {"NO-ERROR", "SERV-FULL", "STATIC",
		"FORMAT-ERR", "NO-SUCH-LLQ", "BAD-VERS", "UNKNOWN_ERR"};
	const unsigned int llq_errors_num = 7;
	const char* llq_opcodes[] = {"LLQ-SETUP", "LLQ-REFRESH", "LLQ-EVENT"};
	const unsigned int llq_opcodes_num = 3;
	uint16_t version, llq_opcode, error_code;
	uint64_t llq_id;
	uint32_t lease_life; /* Requested or granted life of LLQ, in seconds */
	int w = 0;

	/* read the record */
	if(len != 18) {
		w += gldns_str_print(s, sl, "malformed LLQ ");
		w += print_hex_buf(s, sl, data, len);
		return w;
	}
	version = gldns_read_uint16(data);
	llq_opcode = gldns_read_uint16(data+2);
	error_code = gldns_read_uint16(data+4);
	memmove(&llq_id, data+6, sizeof(llq_id));
	lease_life = gldns_read_uint32(data+14);

	/* print it */
	w += gldns_str_print(s, sl, "v%d ", (int)version);
	if(llq_opcode < llq_opcodes_num)
		w += gldns_str_print(s, sl, "%s", llq_opcodes[llq_opcode]);
	else	w += gldns_str_print(s, sl, "opcode %d", (int)llq_opcode);
	if(error_code < llq_errors_num)
		w += gldns_str_print(s, sl, " %s", llq_errors[error_code]);
	else	w += gldns_str_print(s, sl, " error %d", (int)error_code);
#ifndef USE_WINSOCK
	w += gldns_str_print(s, sl, " id %llx lease-life %lu",
		(unsigned long long)llq_id, (unsigned long)lease_life);
#else
	w += gldns_str_print(s, sl, " id %I64x lease-life %lu",
		(unsigned long long)llq_id, (unsigned long)lease_life);
#endif
	return w;
}

int gldns_wire2str_edns_ul_print(char** s, size_t* sl, uint8_t* data,
	size_t len)
{
	uint32_t lease;
	int w = 0;
	if(len != 4) {
		w += gldns_str_print(s, sl, "malformed UL ");
		w += print_hex_buf(s, sl, data, len);
		return w;
	}
	lease = gldns_read_uint32(data);
	w += gldns_str_print(s, sl, "lease %lu", (unsigned long)lease);
	return w;
}

int gldns_wire2str_edns_nsid_print(char** s, size_t* sl, uint8_t* data,
	size_t len)
{
	int w = 0;
	size_t i, printed=0;
	w += print_hex_buf(s, sl, data, len);
	for(i=0; i<len; i++) {
		if(isprint((unsigned char)data[i]) || data[i] == '\t') {
			if(!printed) {
				w += gldns_str_print(s, sl, " (");
				printed = 1;
			}
			w += gldns_str_print(s, sl, "%c", (char)data[i]);
		}
	}
	if(printed)
		w += gldns_str_print(s, sl, ")");
	return w;
}

int gldns_wire2str_edns_dau_print(char** s, size_t* sl, uint8_t* data,
	size_t len)
{
	gldns_lookup_table *lt;
	size_t i;
	int w = 0;
	for(i=0; i<len; i++) {
		lt = gldns_lookup_by_id(gldns_algorithms, (int)data[i]);
		if(lt && lt->name)
			w += gldns_str_print(s, sl, " %s", lt->name);
		else 	w += gldns_str_print(s, sl, " %d", (int)data[i]);
	}
	return w;
}

int gldns_wire2str_edns_dhu_print(char** s, size_t* sl, uint8_t* data,
	size_t len)
{
	gldns_lookup_table *lt;
	size_t i;
	int w = 0;
	for(i=0; i<len; i++) {
		lt = gldns_lookup_by_id(gldns_hashes, (int)data[i]);
		if(lt && lt->name)
			w += gldns_str_print(s, sl, " %s", lt->name);
		else 	w += gldns_str_print(s, sl, " %d", (int)data[i]);
	}
	return w;
}

int gldns_wire2str_edns_n3u_print(char** s, size_t* sl, uint8_t* data,
	size_t len)
{
	size_t i;
	int w = 0;
	for(i=0; i<len; i++) {
		if(data[i] == 1)
			w += gldns_str_print(s, sl, " SHA1");
		else 	w += gldns_str_print(s, sl, " %d", (int)data[i]);
	}
	return w;
}

int gldns_wire2str_edns_subnet_print(char** s, size_t* sl, uint8_t* data,
	size_t len)
{
	int w = 0;
	uint16_t family;
	uint8_t source, scope;
	if(len < 4) {
		w += gldns_str_print(s, sl, "malformed subnet ");
		w += print_hex_buf(s, sl, data, len);
		return w;
	}
	family = gldns_read_uint16(data);
	source = data[2];
	scope = data[3];
	if(family == 1) {
		/* IP4 */
		char buf[64];
		uint8_t ip4[4];
		memset(ip4, 0, sizeof(ip4));
		if(len-4 > 4) {
			w += gldns_str_print(s, sl, "trailingdata:");
			w += print_hex_buf(s, sl, data+4+4, len-4-4);
			w += gldns_str_print(s, sl, " ");
			len = 4+4;
		}
		memmove(ip4, data+4, len-4);
		if(!inet_ntop(AF_INET, ip4, buf, (socklen_t)sizeof(buf))) {
			w += gldns_str_print(s, sl, "ip4ntoperror ");
			w += print_hex_buf(s, sl, data+4+4, len-4-4);
		} else {
			w += gldns_str_print(s, sl, "%s", buf);
		}
	} else if(family == 2) {
		/* IP6 */
		char buf[64];
		uint8_t ip6[16];
		memset(ip6, 0, sizeof(ip6));
		if(len-4 > 16) {
			w += gldns_str_print(s, sl, "trailingdata:");
			w += print_hex_buf(s, sl, data+4+16, len-4-16);
			w += gldns_str_print(s, sl, " ");
			len = 4+16;
		}
		memmove(ip6, data+4, len-4);
#ifdef AF_INET6
		if(!inet_ntop(AF_INET6, ip6, buf, (socklen_t)sizeof(buf))) {
			w += gldns_str_print(s, sl, "ip6ntoperror ");
			w += print_hex_buf(s, sl, data+4+4, len-4-4);
		} else {
			w += gldns_str_print(s, sl, "%s", buf);
		}
#else
		w += print_hex_buf(s, sl, data+4+4, len-4-4);
#endif
	} else {
		/* unknown */
		w += gldns_str_print(s, sl, "family %d ",
			(int)family);
		w += print_hex_buf(s, sl, data, len);
	}
	w += gldns_str_print(s, sl, "/%d scope /%d", (int)source, (int)scope);
	return w;
}

static int gldns_wire2str_edns_keepalive_print(char** s, size_t* sl,
	uint8_t* data, size_t len)
{
	int w = 0;
	uint16_t timeout;
	if(!(len == 0 || len == 2)) {
		w += gldns_str_print(s, sl, "malformed keepalive ");
		w += print_hex_buf(s, sl, data, len);
		return w;
	}
	if(len == 0 ) {
		w += gldns_str_print(s, sl, "no timeout value (only valid for client option) ");
	} else {
		timeout = gldns_read_uint16(data);
		w += gldns_str_print(s, sl, "timeout value in units of 100ms %u", (int)timeout);
	}
	return w;
}

int gldns_wire2str_edns_option_print(char** s, size_t* sl,
	uint16_t option_code, uint8_t* optdata, size_t optlen)
{
	int w = 0;
	w += gldns_wire2str_edns_option_code_print(s, sl, option_code);
	w += gldns_str_print(s, sl, ": ");
	switch(option_code) {
	case GLDNS_EDNS_LLQ:
		w += gldns_wire2str_edns_llq_print(s, sl, optdata, optlen);
		break;
	case GLDNS_EDNS_UL:
		w += gldns_wire2str_edns_ul_print(s, sl, optdata, optlen);
		break;
	case GLDNS_EDNS_NSID:
		w += gldns_wire2str_edns_nsid_print(s, sl, optdata, optlen);
		break;
	case GLDNS_EDNS_DAU:
		w += gldns_wire2str_edns_dau_print(s, sl, optdata, optlen);
		break;
	case GLDNS_EDNS_DHU:
		w += gldns_wire2str_edns_dhu_print(s, sl, optdata, optlen);
		break;
	case GLDNS_EDNS_N3U:
		w += gldns_wire2str_edns_n3u_print(s, sl, optdata, optlen);
		break;
	case GLDNS_EDNS_CLIENT_SUBNET:
		w += gldns_wire2str_edns_subnet_print(s, sl, optdata, optlen);
		break;
	 case GLDNS_EDNS_KEEPALIVE:
		w += gldns_wire2str_edns_keepalive_print(s, sl, optdata, optlen);
		break;
	case GLDNS_EDNS_PADDING:
		w += print_hex_buf(s, sl, optdata, optlen);
		break;
	default:
		/* unknown option code */
		w += print_hex_buf(s, sl, optdata, optlen);
		break;
	}
	return w;
}

/** print the edns options to string */
static int
print_edns_opts(char** s, size_t* sl, uint8_t* rdata, size_t rdatalen)
{
	uint16_t option_code, option_len;
	int w = 0;
	while(rdatalen > 0) {
		/* option name */
		if(rdatalen < 4) {
			w += gldns_str_print(s, sl, " ; malformed: ");
			w += print_hex_buf(s, sl, rdata, rdatalen);
			return w;
		}
		option_code = gldns_read_uint16(rdata);
		option_len = gldns_read_uint16(rdata+2);
		rdata += 4;
		rdatalen -= 4;

		/* option value */
		if(rdatalen < (size_t)option_len) {
			w += gldns_str_print(s, sl, " ; malformed ");
			w += gldns_wire2str_edns_option_code_print(s, sl,
				option_code);
			w += gldns_str_print(s, sl, ": ");
			w += print_hex_buf(s, sl, rdata, rdatalen);
			return w;
		}
		w += gldns_str_print(s, sl, " ; ");
		w += gldns_wire2str_edns_option_print(s, sl, option_code,
			rdata, option_len);
		rdata += option_len;
		rdatalen -= option_len;
	}
	return w;
}

int gldns_wire2str_edns_scan(uint8_t** data, size_t* data_len, char** str,
        size_t* str_len, uint8_t* pkt, size_t pktlen)
{
	int w = 0;
	uint8_t ext_rcode, edns_version;
	uint16_t udpsize, edns_bits, rdatalen;
	w += gldns_str_print(str, str_len, "; EDNS:");

	/* some input checks, domain name */
	if(*data_len < 1+10)
		return w + print_remainder_hex("Error malformed 0x",
			data, data_len, str, str_len);
	if(*data[0] != 0) {
		return w + print_remainder_hex("Error nonrootdname 0x",
			data, data_len, str, str_len);
	}
	(*data)++;
	(*data_len)--;

	/* check type and read fixed contents */
	if(gldns_read_uint16((*data)) != GLDNS_RR_TYPE_OPT) {
		return w + print_remainder_hex("Error nottypeOPT 0x",
			data, data_len, str, str_len);
	}
	udpsize = gldns_read_uint16((*data)+2);
	ext_rcode = (*data)[4];
	edns_version = (*data)[5];
	edns_bits = gldns_read_uint16((*data)+6);
	rdatalen = gldns_read_uint16((*data)+8);
	(*data)+=10;
	(*data_len)-=10;

	w += gldns_str_print(str, str_len, " version: %u;",
		(unsigned)edns_version);
	w += gldns_str_print(str, str_len, " flags:");
	if((edns_bits & GLDNS_EDNS_MASK_DO_BIT))
		w += gldns_str_print(str, str_len, " do");
	/* the extended rcode is the value set, shifted four bits,
	 * and or'd with the original rcode */
	if(ext_rcode) {
		int rc = ((int)ext_rcode)<<4;
		if(pkt && pktlen >= GLDNS_HEADER_SIZE)
			rc |= GLDNS_RCODE_WIRE(pkt);
		w += gldns_str_print(str, str_len, " ; ext-rcode: %d", rc);
	}
	w += gldns_str_print(str, str_len, " ; udp: %u", (unsigned)udpsize);

	if(rdatalen) {
		if((size_t)*data_len < rdatalen) {
			w += gldns_str_print(str, str_len,
				" ; Error EDNS rdata too short; ");
			rdatalen = (uint16_t)*data_len;
		}
		w += print_edns_opts(str, str_len, *data, rdatalen);
		(*data) += rdatalen;
		(*data_len) -= rdatalen;
	}
	w += gldns_str_print(str, str_len, "\n");
	return w;
}
