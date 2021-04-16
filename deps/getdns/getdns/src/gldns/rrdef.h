/*
 * rrdef.h
 *
 * RR definitions
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
 * Defines resource record types and constants.
 */

#ifndef GLDNS_RRDEF_H
#define GLDNS_RRDEF_H

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of a dname label */
#define GLDNS_MAX_LABELLEN     63
/** Maximum length of a complete dname */
#define GLDNS_MAX_DOMAINLEN    255
/** Maximum number of pointers in 1 dname */
#define GLDNS_MAX_POINTERS	65535
/** The bytes TTL, CLASS and length use up in an rr */
#define GLDNS_RR_OVERHEAD	10

#define GLDNS_DNSSEC_KEYPROTO    3
#define GLDNS_KEY_ZONE_KEY   0x0100 /* set for ZSK&KSK, rfc 4034 */
#define GLDNS_KEY_SEP_KEY    0x0001 /* set for KSK, rfc 4034 */
#define GLDNS_KEY_REVOKE_KEY 0x0080 /* used to revoke KSK, rfc 5011 */

/* The first fields are contiguous and can be referenced instantly */
#define GLDNS_RDATA_FIELD_DESCRIPTORS_COMMON 260

/** lookuptable for rr classes  */
extern struct gldns_struct_lookup_table* gldns_rr_classes;

/**
 *  The different RR classes.
 */
enum gldns_enum_rr_class
{
	/** the Internet */
	GLDNS_RR_CLASS_IN 	= 1,
	/** Chaos class */
	GLDNS_RR_CLASS_CH	= 3,
	/** Hesiod (Dyer 87) */
	GLDNS_RR_CLASS_HS	= 4,
	/** None class, dynamic update */
	GLDNS_RR_CLASS_NONE      = 254,
	/** Any class */
	GLDNS_RR_CLASS_ANY	= 255,

	GLDNS_RR_CLASS_FIRST     = 0,
	GLDNS_RR_CLASS_LAST      = 65535,
	GLDNS_RR_CLASS_COUNT     = GLDNS_RR_CLASS_LAST - GLDNS_RR_CLASS_FIRST + 1
};
typedef enum gldns_enum_rr_class gldns_rr_class;

/**
 *  Used to specify whether compression is allowed.
 */
enum gldns_enum_rr_compress
{
	/** compression is allowed */
	GLDNS_RR_COMPRESS,
	GLDNS_RR_NO_COMPRESS
};
typedef enum gldns_enum_rr_compress gldns_rr_compress;

/**
 * The different RR types.
 */
enum gldns_enum_rr_type
{
	/**  a host address */
	GLDNS_RR_TYPE_A = 1,
	/**  an authoritative name server */
	GLDNS_RR_TYPE_NS = 2,
	/**  a mail destination (Obsolete - use MX) */
	GLDNS_RR_TYPE_MD = 3,
	/**  a mail forwarder (Obsolete - use MX) */
	GLDNS_RR_TYPE_MF = 4,
	/**  the canonical name for an alias */
	GLDNS_RR_TYPE_CNAME = 5,
	/**  marks the start of a zone of authority */
	GLDNS_RR_TYPE_SOA = 6,
	/**  a mailbox domain name (EXPERIMENTAL) */
	GLDNS_RR_TYPE_MB = 7,
	/**  a mail group member (EXPERIMENTAL) */
	GLDNS_RR_TYPE_MG = 8,
	/**  a mail rename domain name (EXPERIMENTAL) */
	GLDNS_RR_TYPE_MR = 9,
	/**  a null RR (EXPERIMENTAL) */
	GLDNS_RR_TYPE_NULL = 10,
	/**  a well known service description */
	GLDNS_RR_TYPE_WKS = 11,
	/**  a domain name pointer */
	GLDNS_RR_TYPE_PTR = 12,
	/**  host information */
	GLDNS_RR_TYPE_HINFO = 13,
	/**  mailbox or mail list information */
	GLDNS_RR_TYPE_MINFO = 14,
	/**  mail exchange */
	GLDNS_RR_TYPE_MX = 15,
	/**  text strings */
	GLDNS_RR_TYPE_TXT = 16,
	/**  RFC1183 */
	GLDNS_RR_TYPE_RP = 17,
	/**  RFC1183 */
	GLDNS_RR_TYPE_AFSDB = 18,
	/**  RFC1183 */
	GLDNS_RR_TYPE_X25 = 19,
	/**  RFC1183 */
	GLDNS_RR_TYPE_ISDN = 20,
	/**  RFC1183 */
	GLDNS_RR_TYPE_RT = 21,
	/**  RFC1706 */
	GLDNS_RR_TYPE_NSAP = 22,
	/**  RFC1348 */
	GLDNS_RR_TYPE_NSAP_PTR = 23,
	/**  2535typecode */
	GLDNS_RR_TYPE_SIG = 24,
	/**  2535typecode */
	GLDNS_RR_TYPE_KEY = 25,
	/**  RFC2163 */
	GLDNS_RR_TYPE_PX = 26,
	/**  RFC1712 */
	GLDNS_RR_TYPE_GPOS = 27,
	/**  ipv6 address */
	GLDNS_RR_TYPE_AAAA = 28,
	/**  LOC record  RFC1876 */
	GLDNS_RR_TYPE_LOC = 29,
	/**  2535typecode */
	GLDNS_RR_TYPE_NXT = 30,
	/**  draft-ietf-nimrod-dns-01.txt */
	GLDNS_RR_TYPE_EID = 31,
	/**  draft-ietf-nimrod-dns-01.txt */
	GLDNS_RR_TYPE_NIMLOC = 32,
	/**  SRV record RFC2782 */
	GLDNS_RR_TYPE_SRV = 33,
	/**  http://www.jhsoft.com/rfc/af-saa-0069.000.rtf */
	GLDNS_RR_TYPE_ATMA = 34,
	/**  RFC2915 */
	GLDNS_RR_TYPE_NAPTR = 35,
	/**  RFC2230 */
	GLDNS_RR_TYPE_KX = 36,
	/**  RFC2538 */
	GLDNS_RR_TYPE_CERT = 37,
	/**  RFC2874 */
	GLDNS_RR_TYPE_A6 = 38,
	/**  RFC2672 */
	GLDNS_RR_TYPE_DNAME = 39,
	/**  dnsind-kitchen-sink-02.txt */
	GLDNS_RR_TYPE_SINK = 40,
	/**  Pseudo OPT record... */
	GLDNS_RR_TYPE_OPT = 41,
	/**  RFC3123 */
	GLDNS_RR_TYPE_APL = 42,
	/**  RFC4034, RFC3658 */
	GLDNS_RR_TYPE_DS = 43,
	/**  SSH Key Fingerprint */
	GLDNS_RR_TYPE_SSHFP = 44, /* RFC 4255 */
	/**  IPsec Key */
	GLDNS_RR_TYPE_IPSECKEY = 45, /* RFC 4025 */
	/**  DNSSEC */
	GLDNS_RR_TYPE_RRSIG = 46, /* RFC 4034 */
	GLDNS_RR_TYPE_NSEC = 47, /* RFC 4034 */
	GLDNS_RR_TYPE_DNSKEY = 48, /* RFC 4034 */

	GLDNS_RR_TYPE_DHCID = 49, /* RFC 4701 */
	/* NSEC3 */
	GLDNS_RR_TYPE_NSEC3 = 50, /* RFC 5155 */
	GLDNS_RR_TYPE_NSEC3PARAM = 51, /* RFC 5155 */
	GLDNS_RR_TYPE_NSEC3PARAMS = 51,
	GLDNS_RR_TYPE_TLSA = 52, /* RFC 6698 */
	GLDNS_RR_TYPE_SMIMEA = 53, /* RFC 8162 */
	GLDNS_RR_TYPE_HIP = 55, /* RFC 5205 */

	/** draft-reid-dnsext-zs */
	GLDNS_RR_TYPE_NINFO = 56,
	/** draft-reid-dnsext-rkey */
	GLDNS_RR_TYPE_RKEY = 57,
        /** draft-ietf-dnsop-trust-history */
        GLDNS_RR_TYPE_TALINK = 58,
	GLDNS_RR_TYPE_CDS = 59, /** RFC 7344 */
	GLDNS_RR_TYPE_CDNSKEY = 60, /** RFC 7344 */
	GLDNS_RR_TYPE_OPENPGPKEY = 61, /* RFC 7929 */
	GLDNS_RR_TYPE_CSYNC = 62, /* RFC 7477 */
	GLDNS_RR_TYPE_ZONEMD = 63, /* draft-wessels-dns-zone-digest */

	GLDNS_RR_TYPE_SPF = 99, /* RFC 4408 */

	GLDNS_RR_TYPE_UINFO = 100,
	GLDNS_RR_TYPE_UID = 101,
	GLDNS_RR_TYPE_GID = 102,
	GLDNS_RR_TYPE_UNSPEC = 103,

	GLDNS_RR_TYPE_NID = 104, /* RFC 6742 */
	GLDNS_RR_TYPE_L32 = 105, /* RFC 6742 */
	GLDNS_RR_TYPE_L64 = 106, /* RFC 6742 */
	GLDNS_RR_TYPE_LP = 107, /* RFC 6742 */

	/** draft-jabley-dnsext-eui48-eui64-rrtypes */
	GLDNS_RR_TYPE_EUI48 = 108,
	GLDNS_RR_TYPE_EUI64 = 109,

	GLDNS_RR_TYPE_TKEY = 249, /* RFC 2930 */
	GLDNS_RR_TYPE_TSIG = 250,
	GLDNS_RR_TYPE_IXFR = 251,
	GLDNS_RR_TYPE_AXFR = 252,
	/**  A request for mailbox-related records (MB, MG or MR) */
	GLDNS_RR_TYPE_MAILB = 253,
	/**  A request for mail agent RRs (Obsolete - see MX) */
	GLDNS_RR_TYPE_MAILA = 254,
	/**  any type (wildcard) */
	GLDNS_RR_TYPE_ANY = 255,
	GLDNS_RR_TYPE_URI = 256, /* RFC 7553 */
	GLDNS_RR_TYPE_CAA = 257, /* RFC 6844 */
	GLDNS_RR_TYPE_AVC = 258,
	GLDNS_RR_TYPE_DOA = 259, /* draft-durand-doa-over-dns */
	GLDNS_RR_TYPE_AMTRELAY = 260, /* draft-ietf-mboned-driad-amt-discovery */

	/** DNSSEC Trust Authorities */
	GLDNS_RR_TYPE_TA = 32768,
	/* RFC 4431, 5074, DNSSEC Lookaside Validation */
	GLDNS_RR_TYPE_DLV = 32769,

	/* type codes from nsec3 experimental phase
	GLDNS_RR_TYPE_NSEC3 = 65324,
	GLDNS_RR_TYPE_NSEC3PARAMS = 65325, */
	GLDNS_RR_TYPE_FIRST = 0,
	GLDNS_RR_TYPE_LAST  = 65535,
	GLDNS_RR_TYPE_COUNT = GLDNS_RR_TYPE_LAST - GLDNS_RR_TYPE_FIRST + 1
};
typedef enum gldns_enum_rr_type gldns_rr_type;

/* RDATA */
#define GLDNS_MAX_RDFLEN	65535

#define GLDNS_RDF_SIZE_BYTE              1
#define GLDNS_RDF_SIZE_WORD              2
#define GLDNS_RDF_SIZE_DOUBLEWORD        4
#define GLDNS_RDF_SIZE_6BYTES            6
#define GLDNS_RDF_SIZE_8BYTES            8
#define GLDNS_RDF_SIZE_16BYTES           16

#define GLDNS_NSEC3_VARS_OPTOUT_MASK 0x01

#define GLDNS_APL_IP4            1
#define GLDNS_APL_IP6            2
#define GLDNS_APL_MASK           0x7f
#define GLDNS_APL_NEGATION       0x80

/**
 * The different types of RDATA fields.
 */
enum gldns_enum_rdf_type
{
	/** none */
	GLDNS_RDF_TYPE_NONE,
	/** domain name */
	GLDNS_RDF_TYPE_DNAME,
	/** 8 bits */
	GLDNS_RDF_TYPE_INT8,
	/** 16 bits */
	GLDNS_RDF_TYPE_INT16,
	/** 32 bits */
	GLDNS_RDF_TYPE_INT32,
	/** A record */
	GLDNS_RDF_TYPE_A,
	/** AAAA record */
	GLDNS_RDF_TYPE_AAAA,
	/** txt string */
	GLDNS_RDF_TYPE_STR,
	/** apl data */
	GLDNS_RDF_TYPE_APL,
	/** b32 string */
	GLDNS_RDF_TYPE_B32_EXT,
	/** b64 string */
	GLDNS_RDF_TYPE_B64,
	/** hex string */
	GLDNS_RDF_TYPE_HEX,
	/** nsec type codes */
	GLDNS_RDF_TYPE_NSEC,
	/** a RR type */
	GLDNS_RDF_TYPE_TYPE,
	/** a class */
	GLDNS_RDF_TYPE_CLASS,
	/** certificate algorithm */
	GLDNS_RDF_TYPE_CERT_ALG,
	/** a key algorithm */
        GLDNS_RDF_TYPE_ALG,
        /** unknown types */
        GLDNS_RDF_TYPE_UNKNOWN,
        /** time (32 bits) */
        GLDNS_RDF_TYPE_TIME,
        /** period */
        GLDNS_RDF_TYPE_PERIOD,
        /** tsig time 48 bits */
        GLDNS_RDF_TYPE_TSIGTIME,
	/** Represents the Public Key Algorithm, HIT and Public Key fields
	    for the HIP RR types.  A HIP specific rdf type is used because of
	    the unusual layout in wireformat (see RFC 5205 Section 5) */
	GLDNS_RDF_TYPE_HIP,
        /** variable length any type rdata where the length
            is specified by the first 2 bytes */
        GLDNS_RDF_TYPE_INT16_DATA,
        /** protocol and port bitmaps */
        GLDNS_RDF_TYPE_SERVICE,
        /** location data */
        GLDNS_RDF_TYPE_LOC,
        /** well known services */
        GLDNS_RDF_TYPE_WKS,
        /** NSAP */
        GLDNS_RDF_TYPE_NSAP,
        /** ATMA */
        GLDNS_RDF_TYPE_ATMA,
        /** IPSECKEY */
        GLDNS_RDF_TYPE_IPSECKEY,
        /** nsec3 hash salt */
        GLDNS_RDF_TYPE_NSEC3_SALT,
        /** nsec3 base32 string (with length byte on wire */
        GLDNS_RDF_TYPE_NSEC3_NEXT_OWNER,

        /** 4 shorts represented as 4 * 16 bit hex numbers
         *  separated by colons. For NID and L64.
         */
        GLDNS_RDF_TYPE_ILNP64,

        /** 6 * 8 bit hex numbers separated by dashes. For EUI48. */
        GLDNS_RDF_TYPE_EUI48,
        /** 8 * 8 bit hex numbers separated by dashes. For EUI64. */
        GLDNS_RDF_TYPE_EUI64,

        /** A non-zero sequence of US-ASCII letters and numbers in lower case.
         *  For CAA.
         */
        GLDNS_RDF_TYPE_TAG,

        /** A <character-string> encoding of the value field as specified 
         * [RFC1035], Section 5.1., encoded as remaining rdata.
         * For CAA, URI.
         */
        GLDNS_RDF_TYPE_LONG_STR,

	/* draft-ietf-mboned-driad-amt-discovery */
	GLDNS_RDF_TYPE_AMTRELAY,

	/** TSIG extended 16bit error value */
	GLDNS_RDF_TYPE_TSIGERROR,

        /* Aliases */
        GLDNS_RDF_TYPE_BITMAP = GLDNS_RDF_TYPE_NSEC
};
typedef enum gldns_enum_rdf_type gldns_rdf_type;

/**
 * Algorithms used in dns
 */
enum gldns_enum_algorithm
{
        GLDNS_RSAMD5             = 1,   /* RFC 4034,4035 */
        GLDNS_DH                 = 2,
        GLDNS_DSA                = 3,
        GLDNS_ECC                = 4,
        GLDNS_RSASHA1            = 5,
        GLDNS_DSA_NSEC3          = 6,
        GLDNS_RSASHA1_NSEC3      = 7,
        GLDNS_RSASHA256          = 8,   /* RFC 5702 */
        GLDNS_RSASHA512          = 10,  /* RFC 5702 */
        GLDNS_ECC_GOST           = 12,  /* RFC 5933 */
        GLDNS_ECDSAP256SHA256    = 13,  /* RFC 6605 */
        GLDNS_ECDSAP384SHA384    = 14,  /* RFC 6605 */
	GLDNS_ED25519		= 15,  /* RFC 8080 */
	GLDNS_ED448		= 16,  /* RFC 8080 */
        GLDNS_INDIRECT           = 252,
        GLDNS_PRIVATEDNS         = 253,
        GLDNS_PRIVATEOID         = 254
};
typedef enum gldns_enum_algorithm gldns_algorithm;

/**
 * Hashing algorithms used in the DS record
 */
enum gldns_enum_hash
{
        GLDNS_SHA1               = 1,  /* RFC 4034 */
        GLDNS_SHA256             = 2,  /* RFC 4509 */
        GLDNS_HASH_GOST          = 3,  /* RFC 5933 */
        GLDNS_SHA384             = 4   /* RFC 6605 */
};
typedef enum gldns_enum_hash gldns_hash;

/**
 * algorithms used in CERT rrs
 */
enum gldns_enum_cert_algorithm
{
        GLDNS_CERT_PKIX          = 1,
        GLDNS_CERT_SPKI          = 2,
        GLDNS_CERT_PGP           = 3,
        GLDNS_CERT_IPKIX         = 4,
        GLDNS_CERT_ISPKI         = 5,
        GLDNS_CERT_IPGP          = 6,
        GLDNS_CERT_ACPKIX        = 7,
        GLDNS_CERT_IACPKIX       = 8,
        GLDNS_CERT_URI           = 253,
        GLDNS_CERT_OID           = 254
};
typedef enum gldns_enum_cert_algorithm gldns_cert_algorithm;

/**
 * EDNS option codes
 */
enum gldns_enum_edns_option
{
	GLDNS_EDNS_LLQ = 1, /* http://files.dns-sd.org/draft-sekar-dns-llq.txt */
	GLDNS_EDNS_UL = 2, /* http://files.dns-sd.org/draft-sekar-dns-ul.txt */
	GLDNS_EDNS_NSID = 3, /* RFC5001 */
	/* 4 draft-cheshire-edns0-owner-option */
	GLDNS_EDNS_DAU = 5, /* RFC6975 */
	GLDNS_EDNS_DHU = 6, /* RFC6975 */
	GLDNS_EDNS_N3U = 7, /* RFC6975 */
	GLDNS_EDNS_CLIENT_SUBNET = 8, /* RFC7871 */
	GLDNS_EDNS_KEEPALIVE = 11, /* draft-ietf-dnsop-edns-tcp-keepalive*/
	GLDNS_EDNS_PADDING = 12 /* RFC7830 */
};
typedef enum gldns_enum_edns_option gldns_edns_option;

#define GLDNS_EDNS_MASK_DO_BIT 0x8000

/** TSIG and TKEY extended rcodes (16bit), 0-15 are the normal rcodes. */
#define GLDNS_TSIG_ERROR_NOERROR  0
#define GLDNS_TSIG_ERROR_BADSIG   16
#define GLDNS_TSIG_ERROR_BADKEY   17
#define GLDNS_TSIG_ERROR_BADTIME  18
#define GLDNS_TSIG_ERROR_BADMODE  19
#define GLDNS_TSIG_ERROR_BADNAME  20
#define GLDNS_TSIG_ERROR_BADALG   21

/**
 * Contains all information about resource record types.
 *
 * This structure contains, for all rr types, the rdata fields that are defined.
 */
struct gldns_struct_rr_descriptor
{
	/** Type of the RR that is described here */
	gldns_rr_type    _type;
	/** Textual name of the RR type.  */
	const char *_name;
	/** Minimum number of rdata fields in the RRs of this type.  */
	uint8_t     _minimum;
	/** Maximum number of rdata fields in the RRs of this type.  */
	uint8_t     _maximum;
	/** Wireformat specification for the rr, i.e. the types of rdata fields in their respective order. */
	const gldns_rdf_type *_wireformat;
	/** Special rdf types */
	gldns_rdf_type _variable;
	/** Specifies whether compression can be used for dnames in this RR type. */
	gldns_rr_compress _compress;
	/** The number of DNAMEs in the _wireformat string, for parsing. */
	uint8_t _dname_count;
};
typedef struct gldns_struct_rr_descriptor gldns_rr_descriptor;

/**
 * returns the resource record descriptor for the given rr type.
 *
 * \param[in] type the type value of the rr type
 *\return the gldns_rr_descriptor for this type
 */
const gldns_rr_descriptor *gldns_rr_descript(uint16_t type);

/**
 * returns the minimum number of rdata fields of the rr type this descriptor describes.
 *
 * \param[in]  descriptor for an rr type
 * \return the minimum number of rdata fields
 */
size_t gldns_rr_descriptor_minimum(const gldns_rr_descriptor *descriptor);

/**
 * returns the maximum number of rdata fields of the rr type this descriptor describes.
 *
 * \param[in]  descriptor for an rr type
 * \return the maximum number of rdata fields
 */
size_t gldns_rr_descriptor_maximum(const gldns_rr_descriptor *descriptor);

/**
 * returns the rdf type for the given rdata field number of the rr type for the given descriptor.
 *
 * \param[in] descriptor for an rr type
 * \param[in] field the field number
 * \return the rdf type for the field
 */
gldns_rdf_type gldns_rr_descriptor_field_type(const gldns_rr_descriptor *descriptor, size_t field);

/**
 * retrieves a rrtype by looking up its name.
 * \param[in] name a string with the name
 * \return the type which corresponds with the name
 */
gldns_rr_type gldns_get_rr_type_by_name(const char *name);

/**
 * retrieves a class by looking up its name.
 * \param[in] name string with the name
 * \return the cass which corresponds with the name
 */
gldns_rr_class gldns_get_rr_class_by_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* GLDNS_RRDEF_H */
