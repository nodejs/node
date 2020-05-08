/*
 * pkthdr.h - packet header from wire conversion routines
 *
 * a Net::DNS like library for C
 *
 * (c) NLnet Labs, 2005-2006
 *
 * See the file LICENSE for the license
 */

/**
 * \file
 *
 * Contains functions that translate dns data from the wire format (as sent
 * by servers and clients) to the internal structures for the packet header.
 */
 
#ifndef GLDNS_PKTHDR_H
#define GLDNS_PKTHDR_H

#ifdef __cplusplus
extern "C" {
#endif

/* The length of the header */
#define	GLDNS_HEADER_SIZE	12

/* First octet of flags */
#define	GLDNS_RD_MASK		0x01U
#define	GLDNS_RD_SHIFT	0
#define	GLDNS_RD_WIRE(wirebuf)	(*(wirebuf+2) & GLDNS_RD_MASK)
#define	GLDNS_RD_SET(wirebuf)	(*(wirebuf+2) |= GLDNS_RD_MASK)
#define	GLDNS_RD_CLR(wirebuf)	(*(wirebuf+2) &= ~GLDNS_RD_MASK)

#define GLDNS_TC_MASK		0x02U
#define GLDNS_TC_SHIFT	1
#define	GLDNS_TC_WIRE(wirebuf)	(*(wirebuf+2) & GLDNS_TC_MASK)
#define	GLDNS_TC_SET(wirebuf)	(*(wirebuf+2) |= GLDNS_TC_MASK)
#define	GLDNS_TC_CLR(wirebuf)	(*(wirebuf+2) &= ~GLDNS_TC_MASK)

#define	GLDNS_AA_MASK		0x04U
#define	GLDNS_AA_SHIFT	2
#define	GLDNS_AA_WIRE(wirebuf)	(*(wirebuf+2) & GLDNS_AA_MASK)
#define	GLDNS_AA_SET(wirebuf)	(*(wirebuf+2) |= GLDNS_AA_MASK)
#define	GLDNS_AA_CLR(wirebuf)	(*(wirebuf+2) &= ~GLDNS_AA_MASK)

#define	GLDNS_OPCODE_MASK	0x78U
#define	GLDNS_OPCODE_SHIFT	3
#define	GLDNS_OPCODE_WIRE(wirebuf)	((*(wirebuf+2) & GLDNS_OPCODE_MASK) >> GLDNS_OPCODE_SHIFT)
#define	GLDNS_OPCODE_SET(wirebuf, opcode) \
	(*(wirebuf+2) = ((*(wirebuf+2)) & ~GLDNS_OPCODE_MASK) | ((opcode) << GLDNS_OPCODE_SHIFT))

#define	GLDNS_QR_MASK		0x80U
#define	GLDNS_QR_SHIFT	7
#define	GLDNS_QR_WIRE(wirebuf)	(*(wirebuf+2) & GLDNS_QR_MASK)
#define	GLDNS_QR_SET(wirebuf)	(*(wirebuf+2) |= GLDNS_QR_MASK)
#define	GLDNS_QR_CLR(wirebuf)	(*(wirebuf+2) &= ~GLDNS_QR_MASK)

/* Second octet of flags */
#define	GLDNS_RCODE_MASK	0x0fU
#define	GLDNS_RCODE_SHIFT	0
#define	GLDNS_RCODE_WIRE(wirebuf)	(*(wirebuf+3) & GLDNS_RCODE_MASK)
#define	GLDNS_RCODE_SET(wirebuf, rcode) \
	(*(wirebuf+3) = ((*(wirebuf+3)) & ~GLDNS_RCODE_MASK) | (rcode))

#define	GLDNS_CD_MASK		0x10U
#define	GLDNS_CD_SHIFT	4
#define	GLDNS_CD_WIRE(wirebuf)	(*(wirebuf+3) & GLDNS_CD_MASK)
#define	GLDNS_CD_SET(wirebuf)	(*(wirebuf+3) |= GLDNS_CD_MASK)
#define	GLDNS_CD_CLR(wirebuf)	(*(wirebuf+3) &= ~GLDNS_CD_MASK)

#define	GLDNS_AD_MASK		0x20U
#define	GLDNS_AD_SHIFT	5
#define	GLDNS_AD_WIRE(wirebuf)	(*(wirebuf+3) & GLDNS_AD_MASK)
#define	GLDNS_AD_SET(wirebuf)	(*(wirebuf+3) |= GLDNS_AD_MASK)
#define	GLDNS_AD_CLR(wirebuf)	(*(wirebuf+3) &= ~GLDNS_AD_MASK)

#define	GLDNS_Z_MASK		0x40U
#define	GLDNS_Z_SHIFT		6
#define	GLDNS_Z_WIRE(wirebuf)	(*(wirebuf+3) & GLDNS_Z_MASK)
#define	GLDNS_Z_SET(wirebuf)	(*(wirebuf+3) |= GLDNS_Z_MASK)
#define	GLDNS_Z_CLR(wirebuf)	(*(wirebuf+3) &= ~GLDNS_Z_MASK)

#define	GLDNS_RA_MASK		0x80U
#define	GLDNS_RA_SHIFT	7
#define	GLDNS_RA_WIRE(wirebuf)	(*(wirebuf+3) & GLDNS_RA_MASK)
#define	GLDNS_RA_SET(wirebuf)	(*(wirebuf+3) |= GLDNS_RA_MASK)
#define	GLDNS_RA_CLR(wirebuf)	(*(wirebuf+3) &= ~GLDNS_RA_MASK)

/* Query ID */
#define	GLDNS_ID_WIRE(wirebuf)		(gldns_read_uint16(wirebuf))
#define	GLDNS_ID_SET(wirebuf, id)	(gldns_write_uint16(wirebuf, id))

/* Counter of the question section */
#define GLDNS_QDCOUNT_OFF		4
/*
#define	QDCOUNT(wirebuf)		(ntohs(*(uint16_t *)(wirebuf+QDCOUNT_OFF)))
*/
#define	GLDNS_QDCOUNT(wirebuf)		(gldns_read_uint16(wirebuf+GLDNS_QDCOUNT_OFF))

/* Counter of the answer section */
#define GLDNS_ANCOUNT_OFF		6
#define	GLDNS_ANCOUNT(wirebuf)		(gldns_read_uint16(wirebuf+GLDNS_ANCOUNT_OFF))

/* Counter of the authority section */
#define GLDNS_NSCOUNT_OFF		8
#define	GLDNS_NSCOUNT(wirebuf)		(gldns_read_uint16(wirebuf+GLDNS_NSCOUNT_OFF))

/* Counter of the additional section */
#define GLDNS_ARCOUNT_OFF		10
#define	GLDNS_ARCOUNT(wirebuf)		(gldns_read_uint16(wirebuf+GLDNS_ARCOUNT_OFF))

/**
 * The sections of a packet
 */
enum gldns_enum_pkt_section {
        GLDNS_SECTION_QUESTION = 0,
        GLDNS_SECTION_ANSWER = 1,
        GLDNS_SECTION_AUTHORITY = 2,
        GLDNS_SECTION_ADDITIONAL = 3,
        /** bogus section, if not interested */
        GLDNS_SECTION_ANY = 4,
        /** used to get all non-question rrs from a packet */
        GLDNS_SECTION_ANY_NOQUESTION = 5
};
typedef enum gldns_enum_pkt_section gldns_pkt_section;

/* opcodes for pkt's */
enum gldns_enum_pkt_opcode {
        GLDNS_PACKET_QUERY = 0,
        GLDNS_PACKET_IQUERY = 1,
        GLDNS_PACKET_STATUS = 2, /* there is no 3?? DNS is weird */
        GLDNS_PACKET_NOTIFY = 4,
        GLDNS_PACKET_UPDATE = 5
};
typedef enum gldns_enum_pkt_opcode gldns_pkt_opcode;

/* rcodes for pkts */
enum gldns_enum_pkt_rcode {
        GLDNS_RCODE_NOERROR = 0,
        GLDNS_RCODE_FORMERR = 1,
        GLDNS_RCODE_SERVFAIL = 2,
        GLDNS_RCODE_NXDOMAIN = 3,
        GLDNS_RCODE_NOTIMPL = 4,
        GLDNS_RCODE_REFUSED = 5,
        GLDNS_RCODE_YXDOMAIN = 6,
        GLDNS_RCODE_YXRRSET = 7,
        GLDNS_RCODE_NXRRSET = 8,
        GLDNS_RCODE_NOTAUTH = 9,
        GLDNS_RCODE_NOTZONE = 10
};
typedef enum gldns_enum_pkt_rcode gldns_pkt_rcode;

#ifdef __cplusplus
}
#endif

#endif /* GLDNS_PKTHDR_H */
