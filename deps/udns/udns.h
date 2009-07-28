/* $Id: udns.h,v 1.51 2007/01/15 21:19:08 mjt Exp $
   header file for the UDNS library.

   Copyright (C) 2005  Michael Tokarev <mjt@corpit.ru>
   This file is part of UDNS library, an async DNS stub resolver.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library, in file named COPYING.LGPL; if not,
   write to the Free Software Foundation, Inc., 59 Temple Place,
   Suite 330, Boston, MA  02111-1307  USA

 */

#ifndef UDNS_VERSION	/* include guard */

#define UDNS_VERSION "0.0.9"

#ifdef WINDOWS
# ifdef UDNS_DYNAMIC_LIBRARY
#  ifdef DNS_LIBRARY_BUILD
#   define UDNS_API __declspec(dllexport)
#   define UDNS_DATA_API __declspec(dllexport)
#  else
#   define UDNS_API __declspec(dllimport)
#   define UDNS_DATA_API __declspec(dllimport)
#  endif
# endif
#endif

#ifndef UDNS_API
# define UDNS_API
#endif
#ifndef UDNS_DATA_API
# define UDNS_DATA_API
#endif

#include <sys/types.h>		/* for time_t */

#ifdef __cplusplus
extern "C" {
#endif

/* forward declarations if sockets stuff isn't #include'd */
struct in_addr;
struct in6_addr;
struct sockaddr;

/**************************************************************************/
/**************** Common definitions **************************************/

UDNS_API const char *
dns_version(void);

struct dns_ctx;
struct dns_query;

/* shorthand for [const] unsigned char */
typedef unsigned char dnsc_t;
typedef const unsigned char dnscc_t;

#define DNS_MAXDN	255	/* max DN length */
#define DNS_DNPAD	1	/* padding for DN buffers */
#define DNS_MAXLABEL	63	/* max DN label length */
#define DNS_MAXNAME	1024	/* max asciiz domain name length */
#define DNS_HSIZE	12	/* DNS packet header size */
#define DNS_PORT	53	/* default domain port */
#define DNS_MAXSERV	6	/* max servers to consult */
#define DNS_MAXPACKET	512	/* max traditional-DNS UDP packet size */
#define DNS_EDNS0PACKET	4096	/* EDNS0 packet size to use */

enum dns_class {	/* DNS RR Classes */
  DNS_C_INVALID	= 0,	/* invalid class */
  DNS_C_IN	= 1,	/* Internet */
  DNS_C_CH	= 3,	/* CHAOS */
  DNS_C_HS	= 4,	/* HESIOD */
  DNS_C_ANY	= 255	/* wildcard */
};

enum dns_type {		/* DNS RR Types */
  DNS_T_INVALID		= 0,	/* Cookie. */
  DNS_T_A		= 1,	/* Host address. */
  DNS_T_NS		= 2,	/* Authoritative server. */
  DNS_T_MD		= 3,	/* Mail destination. */
  DNS_T_MF		= 4,	/* Mail forwarder. */
  DNS_T_CNAME		= 5,	/* Canonical name. */
  DNS_T_SOA		= 6,	/* Start of authority zone. */
  DNS_T_MB		= 7,	/* Mailbox domain name. */
  DNS_T_MG		= 8,	/* Mail group member. */
  DNS_T_MR		= 9,	/* Mail rename name. */
  DNS_T_NULL		= 10,	/* Null resource record. */
  DNS_T_WKS		= 11,	/* Well known service. */
  DNS_T_PTR		= 12,	/* Domain name pointer. */
  DNS_T_HINFO		= 13,	/* Host information. */
  DNS_T_MINFO		= 14,	/* Mailbox information. */
  DNS_T_MX		= 15,	/* Mail routing information. */
  DNS_T_TXT		= 16,	/* Text strings. */
  DNS_T_RP		= 17,	/* Responsible person. */
  DNS_T_AFSDB		= 18,	/* AFS cell database. */
  DNS_T_X25		= 19,	/* X_25 calling address. */
  DNS_T_ISDN		= 20,	/* ISDN calling address. */
  DNS_T_RT		= 21,	/* Router. */
  DNS_T_NSAP		= 22,	/* NSAP address. */
  DNS_T_NSAP_PTR	= 23,	/* Reverse NSAP lookup (deprecated). */
  DNS_T_SIG		= 24,	/* Security signature. */
  DNS_T_KEY		= 25,	/* Security key. */
  DNS_T_PX		= 26,	/* X.400 mail mapping. */
  DNS_T_GPOS		= 27,	/* Geographical position (withdrawn). */
  DNS_T_AAAA		= 28,	/* Ip6 Address. */
  DNS_T_LOC		= 29,	/* Location Information. */
  DNS_T_NXT		= 30,	/* Next domain (security). */
  DNS_T_EID		= 31,	/* Endpoint identifier. */
  DNS_T_NIMLOC		= 32,	/* Nimrod Locator. */
  DNS_T_SRV		= 33,	/* Server Selection. */
  DNS_T_ATMA		= 34,	/* ATM Address */
  DNS_T_NAPTR		= 35,	/* Naming Authority PoinTeR */
  DNS_T_KX		= 36,	/* Key Exchange */
  DNS_T_CERT		= 37,	/* Certification record */
  DNS_T_A6		= 38,	/* IPv6 address (deprecates AAAA) */
  DNS_T_DNAME		= 39,	/* Non-terminal DNAME (for IPv6) */
  DNS_T_SINK		= 40,	/* Kitchen sink (experimentatl) */
  DNS_T_OPT		= 41,	/* EDNS0 option (meta-RR) */
  DNS_T_DS		= 43,	/* DNSSEC */
  DNS_T_NSEC		= 47,	/* DNSSEC */
  DNS_T_TSIG		= 250,	/* Transaction signature. */
  DNS_T_IXFR		= 251,	/* Incremental zone transfer. */
  DNS_T_AXFR		= 252,	/* Transfer zone of authority. */
  DNS_T_MAILB		= 253,	/* Transfer mailbox records. */
  DNS_T_MAILA		= 254,	/* Transfer mail agent records. */
  DNS_T_ANY		= 255,	/* Wildcard match. */
  DNS_T_ZXFR		= 256,	/* BIND-specific, nonstandard. */
  DNS_T_MAX		= 65536
};

/**************************************************************************/
/**************** Domain Names (DNs) **************************************/

/* return length of the DN */
UDNS_API unsigned
dns_dnlen(dnscc_t *dn);

/* return #of labels in a DN */
UDNS_API unsigned
dns_dnlabels(dnscc_t *dn);

/* lower- and uppercase single DN char */
#define DNS_DNLC(c) ((c) >= 'A' && (c) <= 'Z' ? (c) - 'A' + 'a' : (c))
#define DNS_DNUC(c) ((c) >= 'a' && (c) <= 'z' ? (c) - 'a' + 'A' : (c))

/* compare the DNs, return dnlen of equal or 0 if not */
UDNS_API unsigned
dns_dnequal(dnscc_t *dn1, dnscc_t *dn2);

/* copy one DN to another, size checking */
UDNS_API unsigned
dns_dntodn(dnscc_t *sdn, dnsc_t *ddn, unsigned ddnsiz);

/* convert asciiz string of length namelen (0 to use strlen) to DN */
UDNS_API int
dns_ptodn(const char *name, unsigned namelen,
          dnsc_t *dn, unsigned dnsiz, int *isabs);

/* simpler form of dns_ptodn() */
#define dns_sptodn(name,dn,dnsiz) dns_ptodn((name),0,(dn),(dnsiz),0)

UDNS_DATA_API extern dnscc_t dns_inaddr_arpa_dn[14];
#define DNS_A4RSIZE	30
UDNS_API int
dns_a4todn(const struct in_addr *addr, dnscc_t *tdn,
           dnsc_t *dn, unsigned dnsiz);
UDNS_API int
dns_a4ptodn(const struct in_addr *addr, const char *tname,
            dnsc_t *dn, unsigned dnsiz);
UDNS_API dnsc_t *
dns_a4todn_(const struct in_addr *addr, dnsc_t *dn, dnsc_t *dne);

UDNS_DATA_API extern dnscc_t dns_ip6_arpa_dn[10];
#define DNS_A6RSIZE	74
UDNS_API int
dns_a6todn(const struct in6_addr *addr, dnscc_t *tdn,
           dnsc_t *dn, unsigned dnsiz);
UDNS_API int
dns_a6ptodn(const struct in6_addr *addr, const char *tname,
            dnsc_t *dn, unsigned dnsiz);
UDNS_API dnsc_t *
dns_a6todn_(const struct in6_addr *addr, dnsc_t *dn, dnsc_t *dne);

/* convert DN into asciiz string */
UDNS_API int
dns_dntop(dnscc_t *dn, char *name, unsigned namesiz);

/* convert DN into asciiz string, using static buffer (NOT thread-safe!) */
UDNS_API const char *
dns_dntosp(dnscc_t *dn);

/* return buffer size (incl. null byte) required for asciiz form of a DN */
UDNS_API unsigned
dns_dntop_size(dnscc_t *dn);

/* either wrappers or reimplementations for inet_ntop() and inet_pton() */
UDNS_API const char *dns_ntop(int af, const void *src, char *dst, int size);
UDNS_API int dns_pton(int af, const char *src, void *dst);

/**************************************************************************/
/**************** DNS raw packet layout ***********************************/

enum dns_rcode {	/* reply codes */
  DNS_R_NOERROR		= 0,	/* ok, no error */
  DNS_R_FORMERR		= 1,	/* format error */
  DNS_R_SERVFAIL	= 2,	/* server failed */
  DNS_R_NXDOMAIN	= 3,	/* domain does not exists */
  DNS_R_NOTIMPL		= 4,	/* not implemented */
  DNS_R_REFUSED		= 5,	/* query refused */
  /* these are for BIND_UPDATE */
  DNS_R_YXDOMAIN	= 6,	/* Name exists */
  DNS_R_YXRRSET		= 7,	/* RRset exists */
  DNS_R_NXRRSET		= 8,	/* RRset does not exist */
  DNS_R_NOTAUTH		= 9,	/* Not authoritative for zone */
  DNS_R_NOTZONE		= 10,	/* Zone of record different from zone section */
  /*ns_r_max = 11,*/
  /* The following are TSIG extended errors */
  DNS_R_BADSIG		= 16,
  DNS_R_BADKEY		= 17,
  DNS_R_BADTIME		= 18
};

static __inline unsigned dns_get16(dnscc_t *s) {
  return ((unsigned)s[0]<<8) | s[1];
}
static __inline unsigned dns_get32(dnscc_t *s) {
  return ((unsigned)s[0]<<24) | ((unsigned)s[1]<<16)
        | ((unsigned)s[2]<<8) | s[3];
}
static __inline dnsc_t *dns_put16(dnsc_t *d, unsigned n) {
  *d++ = (dnsc_t)((n >> 8) & 255); *d++ = (dnsc_t)(n & 255); return d;
}
static __inline dnsc_t *dns_put32(dnsc_t *d, unsigned n) {
  *d++ = (dnsc_t)((n >> 24) & 255); *d++ = (dnsc_t)((n >> 16) & 255);
  *d++ = (dnsc_t)((n >>  8) & 255); *d++ = (dnsc_t)(n & 255);
  return d;
}

/* return pseudo-random 16bits number */
UDNS_API unsigned dns_random16(void);

/* DNS Header layout */
enum {
 /* bytes 0:1 - query ID */
  DNS_H_QID1	= 0,
  DNS_H_QID2	= 1,
  DNS_H_QID	= DNS_H_QID1,
#define dns_qid(pkt)	dns_get16((pkt)+DNS_H_QID)
 /* byte 2: flags1 */
  DNS_H_F1	= 2,
  DNS_HF1_QR	= 0x80,	/* query response flag */
#define dns_qr(pkt)	((pkt)[DNS_H_F1]&DNS_HF1_QR)
  DNS_HF1_OPCODE = 0x78, /* opcode, 0 = query */
#define dns_opcode(pkt)	(((pkt)[DNS_H_F1]&DNS_HF1_OPCODE)>>3)
  DNS_HF1_AA	= 0x04,	/* auth answer */
#define dns_aa(pkt)	((pkt)[DNS_H_F1]&DNS_HF1_AA)
  DNS_HF1_TC	= 0x02,	/* truncation flag */
#define dns_tc(pkt)	((pkt)[DNS_H_F1]&DNS_HF1_TC)
  DNS_HF1_RD	= 0x01,	/* recursion desired (may be set in query) */
#define dns_rd(pkt)	((pkt)[DNS_H_F1]&DNS_HF1_RD)
 /* byte 3: flags2 */
  DNS_H_F2	= 3,
  DNS_HF2_RA	= 0x80,	/* recursion available */
#define dns_ra(pkt)	((pkt)[DNS_H_F2]&DNS_HF2_RA)
  DNS_HF2_Z	= 0x70,	/* reserved */
  DNS_HF2_RCODE	= 0x0f,	/* response code, DNS_R_XXX above */
#define dns_rcode(pkt)	((pkt)[DNS_H_F2]&DNS_HF2_RCODE)
 /* bytes 4:5: qdcount, numqueries */
  DNS_H_QDCNT1	= 4,
  DNS_H_QDCNT2	= 5,
  DNS_H_QDCNT	= DNS_H_QDCNT1,
#define dns_numqd(pkt)	dns_get16((pkt)+4)
 /* bytes 6:7: ancount, numanswers */
  DNS_H_ANCNT1	= 6,
  DNS_H_ANCNT2	= 7,
  DNS_H_ANCNT	= DNS_H_ANCNT1,
#define dns_numan(pkt)	dns_get16((pkt)+6)
 /* bytes 8:9: nscount, numauthority */
  DNS_H_NSCNT1	= 8,
  DNS_H_NSCNT2	= 9,
  DNS_H_NSCNT	= DNS_H_NSCNT1,
#define dns_numns(pkt)	dns_get16((pkt)+8)
 /* bytes 10:11: arcount, numadditional */
  DNS_H_ARCNT1	= 10,
  DNS_H_ARCNT2	= 11,
  DNS_H_ARCNT	= DNS_H_ARCNT1,
#define dns_numar(pkt)	dns_get16((pkt)+10)
#define dns_payload(pkt) ((pkt)+DNS_HSIZE)
};

/* packet buffer: start at pkt, end before pkte, current pos *curp.
 * extract a DN and set *curp to the next byte after DN in packet.
 * return -1 on error, 0 if dnsiz is too small, or dnlen on ok.
 */
UDNS_API int
dns_getdn(dnscc_t *pkt, dnscc_t **curp, dnscc_t *end,
          dnsc_t *dn, unsigned dnsiz);

/* skip the DN at position cur in packet ending before pkte,
 * return pointer to the next byte after the DN or NULL on error */
UDNS_API dnscc_t *
dns_skipdn(dnscc_t *end, dnscc_t *cur);

struct dns_rr {		/* DNS Resource Record */
  dnsc_t dnsrr_dn[DNS_MAXDN];	/* the DN of the RR */
  enum dns_class dnsrr_cls;	/* Class */
  enum dns_type  dnsrr_typ;	/* Type */
  unsigned dnsrr_ttl;		/* Time-To-Live (TTL) */
  unsigned dnsrr_dsz;		/* data size */
  dnscc_t *dnsrr_dptr;		/* pointer to start of data */
  dnscc_t *dnsrr_dend;		/* past end of data */
};

struct dns_parse {	/* RR/packet parsing state */
  dnscc_t *dnsp_pkt;		/* start of the packet */
  dnscc_t *dnsp_end;		/* end of the packet */
  dnscc_t *dnsp_cur;		/* current packet position */
  dnscc_t *dnsp_ans;		/* start of answer section */
  int dnsp_rrl;			/* number of RRs left to go */
  int dnsp_nrr;			/* RR count so far */
  unsigned dnsp_ttl;		/* TTL value so far */
  dnscc_t *dnsp_qdn;		/* the RR DN we're looking for */
  enum dns_class dnsp_qcls;	/* RR class we're looking for or 0 */
  enum dns_type  dnsp_qtyp;	/* RR type we're looking for or 0 */
  dnsc_t dnsp_dnbuf[DNS_MAXDN];	/* domain buffer */
};

/* initialize the parse structure */
UDNS_API void
dns_initparse(struct dns_parse *p, dnscc_t *qdn,
              dnscc_t *pkt, dnscc_t *cur, dnscc_t *end);

/* search next RR, <0=error, 0=no more RRs, >0 = found. */
UDNS_API int
dns_nextrr(struct dns_parse *p, struct dns_rr *rr);

UDNS_API void
dns_rewind(struct dns_parse *p, dnscc_t *qdn);


/**************************************************************************/
/**************** Resolver Context ****************************************/

/* default resolver context */
UDNS_DATA_API extern struct dns_ctx dns_defctx;

/* reset resolver context to default state, close it if open, drop queries */
UDNS_API void
dns_reset(struct dns_ctx *ctx);

/* reset resolver context and read in system configuration */
UDNS_API int
dns_init(struct dns_ctx *ctx, int do_open);

/* return new resolver context with the same settings as copy */
UDNS_API struct dns_ctx *
dns_new(const struct dns_ctx *copy);

/* free resolver context returned by dns_new(); all queries are dropped */
UDNS_API void
dns_free(struct dns_ctx *ctx);

/* add nameserver for a resolver context (or reset nslist if serv==NULL) */
UDNS_API int
dns_add_serv(struct dns_ctx *ctx, const char *serv);

/* add nameserver using struct sockaddr structure (with ports) */
UDNS_API int
dns_add_serv_s(struct dns_ctx *ctx, const struct sockaddr *sa);

/* add search list element for a resolver context (or reset it if srch==NULL) */
UDNS_API int
dns_add_srch(struct dns_ctx *ctx, const char *srch);

/* set options for a resolver context */
UDNS_API int
dns_set_opts(struct dns_ctx *ctx, const char *opts);

enum dns_opt {		/* options */
  DNS_OPT_FLAGS,	/* flags, DNS_F_XXX */
  DNS_OPT_TIMEOUT,	/* timeout in secounds */
  DNS_OPT_NTRIES,	/* number of retries */
  DNS_OPT_NDOTS,	/* ndots */
  DNS_OPT_UDPSIZE,	/* EDNS0 UDP size */
  DNS_OPT_PORT,		/* port to use */
};

/* set or get (if val<0) an option */
UDNS_API int
dns_set_opt(struct dns_ctx *ctx, enum dns_opt opt, int val);

enum dns_flags {
  DNS_NOSRCH	= 0x00010000,	/* do not perform search */
  DNS_NORD	= 0x00020000,	/* request no recursion */
  DNS_AAONLY	= 0x00040000,	/* set AA flag in queries */
};

/* set the debug function pointer */
typedef void
(dns_dbgfn)(int code, const struct sockaddr *sa, unsigned salen,
            dnscc_t *pkt, int plen,
            const struct dns_query *q, void *data);
UDNS_API void
dns_set_dbgfn(struct dns_ctx *ctx, dns_dbgfn *dbgfn);

/* open and return UDP socket */
UDNS_API int
dns_open(struct dns_ctx *ctx);

/* return UDP socket or -1 if not open */
UDNS_API int
dns_sock(const struct dns_ctx *ctx);

/* close the UDP socket */
UDNS_API void
dns_close(struct dns_ctx *ctx);

/* return number of requests queued */
UDNS_API int
dns_active(const struct dns_ctx *ctx);

/* return status of the last operation */
UDNS_API int
dns_status(const struct dns_ctx *ctx);
UDNS_API void
dns_setstatus(struct dns_ctx *ctx, int status);

/* handle I/O event on UDP socket */
UDNS_API void
dns_ioevent(struct dns_ctx *ctx, time_t now);

/* process any timeouts, return time in secounds to the
 * next timeout (or -1 if none) but not greather than maxwait */
UDNS_API int
dns_timeouts(struct dns_ctx *ctx, int maxwait, time_t now);

/* define timer requesting routine to use */
typedef void dns_utm_fn(struct dns_ctx *ctx, int timeout, void *data);
UDNS_API void
dns_set_tmcbck(struct dns_ctx *ctx, dns_utm_fn *fn, void *data);

/**************************************************************************/
/**************** Making Queries ******************************************/

/* query callback routine */
typedef void dns_query_fn(struct dns_ctx *ctx, void *result, void *data);

/* query parse routine: raw DNS => application structure */
typedef int
dns_parse_fn(dnscc_t *qdn, dnscc_t *pkt, dnscc_t *cur, dnscc_t *end,
             void **res);

enum dns_status {
  DNS_E_NOERROR		= 0,	/* ok, not an error */
  DNS_E_TEMPFAIL	= -1,	/* timeout, SERVFAIL or similar */
  DNS_E_PROTOCOL	= -2,	/* got garbled reply */
  DNS_E_NXDOMAIN	= -3,	/* domain does not exists */
  DNS_E_NODATA		= -4,	/* domain exists but no data of reqd type */
  DNS_E_NOMEM		= -5,	/* out of memory while processing */
  DNS_E_BADQUERY	= -6	/* the query is malformed */
};

/* submit generic DN query */
UDNS_API struct dns_query *
dns_submit_dn(struct dns_ctx *ctx,
              dnscc_t *dn, int qcls, int qtyp, int flags,
              dns_parse_fn *parse, dns_query_fn *cbck, void *data);
/* submit generic name query */
UDNS_API struct dns_query *
dns_submit_p(struct dns_ctx *ctx,
             const char *name, int qcls, int qtyp, int flags,
             dns_parse_fn *parse, dns_query_fn *cbck, void *data);

/* cancel the given async query in progress */
UDNS_API int
dns_cancel(struct dns_ctx *ctx, struct dns_query *q);

/* resolve a generic query, return the answer */
UDNS_API void *
dns_resolve_dn(struct dns_ctx *ctx,
               dnscc_t *qdn, int qcls, int qtyp, int flags,
               dns_parse_fn *parse);
UDNS_API void *
dns_resolve_p(struct dns_ctx *ctx,
              const char *qname, int qcls, int qtyp, int flags,
              dns_parse_fn *parse);
UDNS_API void *
dns_resolve(struct dns_ctx *ctx, struct dns_query *q);


/* Specific RR handlers */

#define dns_rr_common(prefix)						\
  char *prefix##_cname;		/* canonical name */			\
  char *prefix##_qname;		/* original query name */		\
  unsigned prefix##_ttl;	/* TTL value */				\
  int prefix##_nrr		/* number of records */

struct dns_rr_null {		/* NULL RRset, aka RRset template */
  dns_rr_common(dnsn);
};

UDNS_API int
dns_stdrr_size(const struct dns_parse *p);
UDNS_API void *
dns_stdrr_finish(struct dns_rr_null *ret, char *cp, const struct dns_parse *p);

struct dns_rr_a4 {		/* the A RRset */
  dns_rr_common(dnsa4);
  struct in_addr *dnsa4_addr;	/* array of addresses, naddr elements */
};

UDNS_API dns_parse_fn dns_parse_a4;	/* A RR parsing routine */
typedef void				/* A query callback routine */
dns_query_a4_fn(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data);

/* submit A IN query */
UDNS_API struct dns_query *
dns_submit_a4(struct dns_ctx *ctx, const char *name, int flags,
              dns_query_a4_fn *cbck, void *data);

/* resolve A IN query */
UDNS_API struct dns_rr_a4 *
dns_resolve_a4(struct dns_ctx *ctx, const char *name, int flags);


struct dns_rr_a6 {		/* the AAAA RRset */
  dns_rr_common(dnsa6);
  struct in6_addr *dnsa6_addr;	/* array of addresses, naddr elements */
};

UDNS_API dns_parse_fn dns_parse_a6;	/* A RR parsing routine */
typedef void				/* A query callback routine */
dns_query_a6_fn(struct dns_ctx *ctx, struct dns_rr_a6 *result, void *data);

/* submit AAAA IN query */
UDNS_API struct dns_query *
dns_submit_a6(struct dns_ctx *ctx, const char *name, int flags,
              dns_query_a6_fn *cbck, void *data);

/* resolve AAAA IN query */
UDNS_API struct dns_rr_a6 *
dns_resolve_a6(struct dns_ctx *ctx, const char *name, int flags);


struct dns_rr_ptr {		/* the PTR RRset */
  dns_rr_common(dnsptr);
  char **dnsptr_ptr;		/* array of PTRs */
};

UDNS_API dns_parse_fn dns_parse_ptr;	/* PTR RR parsing routine */
typedef void				/* PTR query callback */
dns_query_ptr_fn(struct dns_ctx *ctx, struct dns_rr_ptr *result, void *data);
/* submit PTR IN in-addr.arpa query */
UDNS_API struct dns_query *
dns_submit_a4ptr(struct dns_ctx *ctx, const struct in_addr *addr,
                 dns_query_ptr_fn *cbck, void *data);
/* resolve PTR IN in-addr.arpa query */
UDNS_API struct dns_rr_ptr *
dns_resolve_a4ptr(struct dns_ctx *ctx, const struct in_addr *addr);

/* the same as above, but for ip6.arpa */
UDNS_API struct dns_query *
dns_submit_a6ptr(struct dns_ctx *ctx, const struct in6_addr *addr,
                 dns_query_ptr_fn *cbck, void *data);
UDNS_API struct dns_rr_ptr *
dns_resolve_a6ptr(struct dns_ctx *ctx, const struct in6_addr *addr);


struct dns_mx {		/* single MX RR */
  int priority;		/* MX priority */
  char *name;		/* MX name */
};
struct dns_rr_mx {		/* the MX RRset */
  dns_rr_common(dnsmx);
  struct dns_mx *dnsmx_mx;	/* array of MXes */
};
UDNS_API dns_parse_fn dns_parse_mx;	/* MX RR parsing routine */
typedef void				/* MX RR callback */
dns_query_mx_fn(struct dns_ctx *ctx, struct dns_rr_mx *result, void *data);
/* submit MX IN query */
UDNS_API struct dns_query *
dns_submit_mx(struct dns_ctx *ctx, const char *name, int flags,
              dns_query_mx_fn *cbck, void *data);
/* resolve MX IN query */
UDNS_API struct dns_rr_mx *
dns_resolve_mx(struct dns_ctx *ctx, const char *name, int flags);


struct dns_txt {	/* single TXT record */
  int len;		/* length of the text */
  dnsc_t *txt;	/* pointer to text buffer. May contain nulls. */
};
struct dns_rr_txt {		/* the TXT RRset */
  dns_rr_common(dnstxt);
  struct dns_txt *dnstxt_txt;	/* array of TXT records */
};
UDNS_API dns_parse_fn dns_parse_txt;	/* TXT RR parsing routine */
typedef void				/* TXT RR callback */
dns_query_txt_fn(struct dns_ctx *ctx, struct dns_rr_txt *result, void *data);
/* submit TXT query */
UDNS_API struct dns_query *
dns_submit_txt(struct dns_ctx *ctx, const char *name, int qcls, int flags,
               dns_query_txt_fn *cbck, void *data);
/* resolve TXT query */
UDNS_API struct dns_rr_txt *
dns_resolve_txt(struct dns_ctx *ctx, const char *name, int qcls, int flags);


struct dns_srv {	/* single SRV RR */
  int priority;		/* SRV priority */
  int weight;		/* SRV weight */
  int port;		/* SRV port */
  char *name;		/* SRV name */
};
struct dns_rr_srv {		/* the SRV RRset */
  dns_rr_common(dnssrv);
  struct dns_srv *dnssrv_srv;	/* array of SRVes */
};
UDNS_API dns_parse_fn dns_parse_srv;	/* SRV RR parsing routine */
typedef void				/* SRV RR callback */
dns_query_srv_fn(struct dns_ctx *ctx, struct dns_rr_srv *result, void *data);
/* submit SRV IN query */
UDNS_API struct dns_query *
dns_submit_srv(struct dns_ctx *ctx,
               const char *name, const char *srv, const char *proto,
               int flags, dns_query_srv_fn *cbck, void *data);
/* resolve SRV IN query */
UDNS_API struct dns_rr_srv *
dns_resolve_srv(struct dns_ctx *ctx,
                const char *name, const char *srv, const char *proto,
                int flags);

/* NAPTR (RFC3403) RR type */
struct dns_naptr {	/* single NAPTR RR */
  int order;		/* NAPTR order */
  int preference;	/* NAPTR preference */
  char *flags;		/* NAPTR flags */
  char *service;	/* NAPTR service */
  char *regexp;		/* NAPTR regexp */
  char *replacement;	/* NAPTR replacement */
};

struct dns_rr_naptr {		/* the NAPTR RRset */
  dns_rr_common(dnsnaptr);
  struct dns_naptr *dnsnaptr_naptr;	/* array of NAPTRes */
};
UDNS_API dns_parse_fn dns_parse_naptr;	/* NAPTR RR parsing routine */
typedef void				/* NAPTR RR callback */
dns_query_naptr_fn(struct dns_ctx *ctx,
                   struct dns_rr_naptr *result, void *data);
/* submit NAPTR IN query */
UDNS_API struct dns_query *
dns_submit_naptr(struct dns_ctx *ctx, const char *name, int flags,
                 dns_query_naptr_fn *cbck, void *data);
/* resolve NAPTR IN query */
UDNS_API struct dns_rr_naptr *
dns_resolve_naptr(struct dns_ctx *ctx, const char *name, int flags);


UDNS_API struct dns_query *
dns_submit_a4dnsbl(struct dns_ctx *ctx,
                   const struct in_addr *addr, const char *dnsbl,
                   dns_query_a4_fn *cbck, void *data);
UDNS_API struct dns_query *
dns_submit_a4dnsbl_txt(struct dns_ctx *ctx,
                       const struct in_addr *addr, const char *dnsbl,
                       dns_query_txt_fn *cbck, void *data);
UDNS_API struct dns_rr_a4 *
dns_resolve_a4dnsbl(struct dns_ctx *ctx,
                    const struct in_addr *addr, const char *dnsbl);
UDNS_API struct dns_rr_txt *
dns_resolve_a4dnsbl_txt(struct dns_ctx *ctx,
                        const struct in_addr *addr, const char *dnsbl);

UDNS_API struct dns_query *
dns_submit_a6dnsbl(struct dns_ctx *ctx,
                   const struct in6_addr *addr, const char *dnsbl,
                   dns_query_a4_fn *cbck, void *data);
UDNS_API struct dns_query *
dns_submit_a6dnsbl_txt(struct dns_ctx *ctx,
                       const struct in6_addr *addr, const char *dnsbl,
                       dns_query_txt_fn *cbck, void *data);
UDNS_API struct dns_rr_a4 *
dns_resolve_a6dnsbl(struct dns_ctx *ctx,
                    const struct in6_addr *addr, const char *dnsbl);
UDNS_API struct dns_rr_txt *
dns_resolve_a6dnsbl_txt(struct dns_ctx *ctx,
                        const struct in6_addr *addr, const char *dnsbl);

UDNS_API struct dns_query *
dns_submit_rhsbl(struct dns_ctx *ctx,
                 const char *name, const char *rhsbl,
                 dns_query_a4_fn *cbck, void *data);
UDNS_API struct dns_query *
dns_submit_rhsbl_txt(struct dns_ctx *ctx,
                     const char *name, const char *rhsbl,
                     dns_query_txt_fn *cbck, void *data);
UDNS_API struct dns_rr_a4 *
dns_resolve_rhsbl(struct dns_ctx *ctx, const char *name, const char *rhsbl);
UDNS_API struct dns_rr_txt *
dns_resolve_rhsbl_txt(struct dns_ctx *ctx, const char *name, const char *rhsbl);

/**************************************************************************/
/**************** Names, Names ********************************************/

struct dns_nameval {
  int val;
  const char *name;
};

UDNS_DATA_API extern const struct dns_nameval dns_classtab[];
UDNS_DATA_API extern const struct dns_nameval dns_typetab[];
UDNS_DATA_API extern const struct dns_nameval dns_rcodetab[];
UDNS_API int
dns_findname(const struct dns_nameval *nv, const char *name);
#define dns_findclassname(cls) dns_findname(dns_classtab, (cls))
#define dns_findtypename(type) dns_findname(dns_typetab, (type))
#define dns_findrcodename(rcode) dns_findname(dns_rcodetab, (rcode))

UDNS_API const char *dns_classname(enum dns_class cls);
UDNS_API const char *dns_typename(enum dns_type type);
UDNS_API const char *dns_rcodename(enum dns_rcode rcode);
const char *_dns_format_code(char *buf, const char *prefix, int code);

UDNS_API const char *dns_strerror(int errnum);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif	/* include guard */
