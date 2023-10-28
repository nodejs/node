/* MIT License
 *
 * Copyright (c) 2023 Brad House
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __ARES_DNS_RECORD_H
#define __ARES_DNS_RECORD_H

/* ----- LIKELY MAKE THESE PUBLIC ----- */

/*! \addtogroup ares_dns_record DNS Record Handling
 *
 * This is a set of functions to create and manipulate DNS records.
 *
 * @{
 */

/*! DNS Record types handled by c-ares.  Some record types may only be valid
 *  on requests (e.g. ARES_REC_TYPE_ANY), and some may only be valid on
 *  responses */
typedef enum {
  ARES_REC_TYPE_A     = 1,  /*!< Host address. */
  ARES_REC_TYPE_NS    = 2,  /*!< Authoritative server. */
  ARES_REC_TYPE_CNAME = 5,  /*!< Canonical name. */
  ARES_REC_TYPE_SOA   = 6,  /*!< Start of authority zone. */
  ARES_REC_TYPE_PTR   = 12, /*!< Domain name pointer. */
  ARES_REC_TYPE_HINFO = 13, /*!< Host information. */
  ARES_REC_TYPE_MX    = 15, /*!< Mail routing information. */
  ARES_REC_TYPE_TXT   = 16, /*!< Text strings. */
  ARES_REC_TYPE_AAAA  = 28, /*!< RFC 3596. Ip6 Address. */
  ARES_REC_TYPE_SRV   = 33, /*!< RFC 2782. Server Selection. */
  ARES_REC_TYPE_NAPTR = 35, /*!< RFC 3403. Naming Authority Pointer */
  ARES_REC_TYPE_OPT   = 41, /*!< RFC 6891. EDNS0 option (meta-RR) */
#if 0
  ARES_REC_TYPE_TLSA     = 52,    /*!< DNS-Based Authentication of Named
                                   *   Entities (DANE) Transport Layer Security
                                   *   (TLS) Protocol: TLSA */
  ARES_REC_TYPE_SVBC     = 64,    /*!< General Purpose Service Binding */
  ARES_REC_TYPE_HTTPS    = 65,    /*!< Service Binding type for use with HTTP */
#endif
  ARES_REC_TYPE_ANY = 255,     /*!< Wildcard match.  Not response RR. */
  ARES_REC_TYPE_URI = 256,     /*!< RFC 7553. Uniform Resource Identifier */
  ARES_REC_TYPE_CAA = 257,     /*!< RFC 6844. Certification Authority
                                *   Authorization. */
  ARES_REC_TYPE_RAW_RR = 65536 /*!< Used as an indicator that the RR record
                                *   is not parsed, but provided in wire
                                *   format */
} ares_dns_rec_type_t;

/*! DNS Classes for requests and responses.  */
typedef enum {
  ARES_CLASS_IN     = 1,   /*!< Internet */
  ARES_CLASS_CHAOS  = 3,   /*!< CHAOS */
  ARES_CLASS_HESOID = 4,   /*!< Hesoid [Dyer 87] */
  ARES_CLASS_NONE   = 254, /*!< RFC 2136 */
  ARES_CLASS_ANY    = 255  /*!< Any class (requests only) */
} ares_dns_class_t;

/*! DNS RR Section type */
typedef enum {
  ARES_SECTION_ANSWER     = 1, /*!< Answer section */
  ARES_SECTION_AUTHORITY  = 2, /*!< Authority section */
  ARES_SECTION_ADDITIONAL = 3  /*!< Additional information section */
} ares_dns_section_t;

/*! DNS Header opcodes */
typedef enum {
  ARES_OPCODE_QUERY  = 0, /*!< Standard query */
  ARES_OPCODE_IQUERY = 1, /*!< Inverse query. Obsolete. */
  ARES_OPCODE_STATUS = 2, /*!< Name server status query */
  ARES_OPCODE_NOTIFY = 4, /*!< Zone change notification (RFC 1996) */
  ARES_OPCODE_UPDATE = 5, /*!< Zone update message (RFC2136) */
} ares_dns_opcode_t;

/*! DNS Header flags */
typedef enum {
  ARES_FLAG_QR = 1 << 0, /*!< QR. If set, is a response */
  ARES_FLAG_AA = 1 << 1, /*!< Authoritative Answer. If set, is authoritative */
  ARES_FLAG_TC = 1 << 2, /*!< Truncation. If set, is truncated response */
  ARES_FLAG_RD = 1 << 3, /*!< Recursion Desired. If set, recursion is desired */
  ARES_FLAG_RA = 1 << 4, /*!< Recursion Available. If set, server supports
                          *   recursion */
  ARES_FLAG_AD = 1 << 5, /*!< RFC 2065. Authentic Data bit indicates in a
                          * response that the data included has been verified by
                          * the server providing it */
  ARES_FLAG_CD = 1 << 6, /*!< RFC 2065. Checking Disabled bit indicates in a
                          * query that non-verified data is acceptable to the
                          * resolver sending the query. */
} ares_dns_flags_t;

/*! DNS Response Codes from server */
typedef enum {
  ARES_RCODE_NOERROR      = 0,    /*!< Success */
  ARES_RCODE_FORMAT_ERROR = 1,    /*!< Format error. The name server was unable
                                   *   to interpret the query. */
  ARES_RCODE_SERVER_FAILURE = 2,  /*!< Server Failure. The name server was
                                   *   unable to process this query due to a
                                   *   problem with the nameserver */
  ARES_RCODE_NAME_ERROR = 3,      /*!< Name Error.  Meaningful only for
                                   *   responses from an authoritative name
                                   *   server, this code signifies that the
                                   *   domain name referenced in the query does
                                   *   not exist. */
  ARES_RCODE_NOT_IMPLEMENTED = 4, /*!< Not implemented.  The name server does
                                   *   not support the requested kind of
                                   *   query */
  ARES_RCODE_REFUSED = 5,         /*!< Refused. The name server refuses to
                                   *   perform the speciied operation for
                                   *   policy reasons. */
  ARES_RCODE_YXDOMAIN = 6,        /*!< RFC 2136. Some name that ought not to
                                   *   exist, does exist. */
  ARES_RCODE_YXRRSET = 7,         /*!< RFC 2136. Some RRset that ought to not
                                   *   exist, does exist. */
  ARES_RCODE_NXRRSET = 8,         /*!< RFC 2136. Some RRset that ought to exist,
                                   *   does not exist. */
  ARES_RCODE_NOTAUTH = 9,         /*!< RFC 2136. The server is not authoritative
                                   *   for the zone named in the Zone section.
                                   */
  ARES_RCODE_NOTZONE = 10,        /*!< RFC 2136. A name used in the Prerequisite
                                   *   or Update Section is not within the zone
                                   *   denoted by the Zone Section. */
} ares_dns_rcode_t;

/*! Data types used */
typedef enum {
  ARES_DATATYPE_INADDR  = 1, /*!< struct in_addr * type */
  ARES_DATATYPE_INADDR6 = 2, /*!< struct ares_in6_addr * type */
  ARES_DATATYPE_U8      = 3, /*!< 8bit unsigned integer */
  ARES_DATATYPE_U16     = 4, /*!< 16bit unsigned integer */
  ARES_DATATYPE_U32     = 5, /*!< 32bit unsigned integer */
  ARES_DATATYPE_STR     = 6, /*!< Null-terminated string */
  ARES_DATATYPE_BIN     = 7  /*!< Binary data */
} ares_dns_datatype_t;

/*! Keys used for all RR Types.  We take the record type and multiply by 100
 *  to ensure we have a proper offset between keys so we can keep these sorted
 */
typedef enum {
  /*! A Record. Address. Datatype: INADDR */
  ARES_RR_A_ADDR = (ARES_REC_TYPE_A * 100) + 1,
  /*! NS Record. Name. Datatype: String */
  ARES_RR_NS_NSDNAME = (ARES_REC_TYPE_NS * 100) + 1,
  /*! CNAME Record. CName. Datatype: String */
  ARES_RR_CNAME_CNAME = (ARES_REC_TYPE_CNAME * 100) + 1,
  /*! SOA Record. MNAME, Primary Source of Data. Datatype: String */
  ARES_RR_SOA_MNAME = (ARES_REC_TYPE_SOA * 100) + 1,
  /*! SOA Record. RNAME, Mailbox of person responsible. Datatype: String */
  ARES_RR_SOA_RNAME = (ARES_REC_TYPE_SOA * 100) + 2,
  /*! SOA Record. Serial, version. Datatype: u32 */
  ARES_RR_SOA_SERIAL = (ARES_REC_TYPE_SOA * 100) + 3,
  /*! SOA Record. Refresh, zone refersh interval. Datatype: u32 */
  ARES_RR_SOA_REFRESH = (ARES_REC_TYPE_SOA * 100) + 4,
  /*! SOA Record. Retry, failed refresh retry interval. Datatype: u32 */
  ARES_RR_SOA_RETRY = (ARES_REC_TYPE_SOA * 100) + 5,
  /*! SOA Record. Expire, upper limit on authority. Datatype: u32 */
  ARES_RR_SOA_EXPIRE = (ARES_REC_TYPE_SOA * 100) + 6,
  /*! SOA Record. Minimum, RR TTL. Datatype: u32 */
  ARES_RR_SOA_MINIMUM = (ARES_REC_TYPE_SOA * 100) + 7,
  /*! PTR Record. DNAME, pointer domain. Datatype: string */
  ARES_RR_PTR_DNAME = (ARES_REC_TYPE_PTR * 100) + 1,
  /*! HINFO Record. CPU. Datatype: string */
  ARES_RR_HINFO_CPU = (ARES_REC_TYPE_HINFO * 100) + 1,
  /*! HINFO Record. OS. Datatype: string */
  ARES_RR_HINFO_OS = (ARES_REC_TYPE_HINFO * 100) + 2,
  /*! MX Record. Preference. Datatype: u16 */
  ARES_RR_MX_PREFERENCE = (ARES_REC_TYPE_MX * 100) + 1,
  /*! MX Record. Exchange, domain. Datatype: string */
  ARES_RR_MX_EXCHANGE = (ARES_REC_TYPE_MX * 100) + 2,
  /*! TXT Record. Data. Datatype: binary */
  ARES_RR_TXT_DATA = (ARES_REC_TYPE_TXT * 100) + 1,
  /*! AAAA Record. Address. Datatype: INADDR6 */
  ARES_RR_AAAA_ADDR = (ARES_REC_TYPE_AAAA * 100) + 1,
  /*! SRV Record. Priority. Datatype: u16 */
  ARES_RR_SRV_PRIORITY = (ARES_REC_TYPE_SRV * 100) + 2,
  /*! SRV Record. Weight. Datatype: u16 */
  ARES_RR_SRV_WEIGHT = (ARES_REC_TYPE_SRV * 100) + 3,
  /*! SRV Record. Port. Datatype: u16 */
  ARES_RR_SRV_PORT = (ARES_REC_TYPE_SRV * 100) + 4,
  /*! SRV Record. Target domain. Datatype: string */
  ARES_RR_SRV_TARGET = (ARES_REC_TYPE_SRV * 100) + 5,
  /*! NAPTR Record. Order. Datatype: u16 */
  ARES_RR_NAPTR_ORDER = (ARES_REC_TYPE_NAPTR * 100) + 1,
  /*! NAPTR Record. Preference. Datatype: u16 */
  ARES_RR_NAPTR_PREFERENCE = (ARES_REC_TYPE_NAPTR * 100) + 2,
  /*! NAPTR Record. Flags. Datatype: string */
  ARES_RR_NAPTR_FLAGS = (ARES_REC_TYPE_NAPTR * 100) + 3,
  /*! NAPTR Record. Services. Datatype: string */
  ARES_RR_NAPTR_SERVICES = (ARES_REC_TYPE_NAPTR * 100) + 4,
  /*! NAPTR Record. Regexp. Datatype: string */
  ARES_RR_NAPTR_REGEXP = (ARES_REC_TYPE_NAPTR * 100) + 5,
  /*! NAPTR Record. Replacement. Datatype: string */
  ARES_RR_NAPTR_REPLACEMENT = (ARES_REC_TYPE_NAPTR * 100) + 6,
  /*! OPT Record. UDP Size. Datatype: u16 */
  ARES_RR_OPT_UDP_SIZE = (ARES_REC_TYPE_OPT * 100) + 1,
  /*! OPT Record. Extended RCode. Datatype: u8 */
  ARES_RR_OPT_EXT_RCODE = (ARES_REC_TYPE_OPT * 100) + 2,
  /*! OPT Record. Version. Datatype: u8 */
  ARES_RR_OPT_VERSION = (ARES_REC_TYPE_OPT * 100) + 3,
  /*! OPT Record. Flags. Datatype: u16 */
  ARES_RR_OPT_FLAGS = (ARES_REC_TYPE_OPT * 100) + 4,
  /*! URI Record. Priority. Datatype: u16 */
  ARES_RR_URI_PRIORITY = (ARES_REC_TYPE_URI * 100) + 1,
  /*! URI Record. Weight. Datatype: u16 */
  ARES_RR_URI_WEIGHT = (ARES_REC_TYPE_URI * 100) + 2,
  /*! URI Record. Target domain. Datatype: string */
  ARES_RR_URI_TARGET = (ARES_REC_TYPE_URI * 100) + 3,
  /*! CAA Record. Critical flag. Datatype: u8 */
  ARES_RR_CAA_CRITICAL = (ARES_REC_TYPE_CAA * 100) + 1,
  /*! CAA Record. Tag/Property. Datatype: string */
  ARES_RR_CAA_TAG = (ARES_REC_TYPE_CAA * 100) + 2,
  /*! CAA Record. Value. Datatype: binary */
  ARES_RR_CAA_VALUE = (ARES_REC_TYPE_CAA * 100) + 3,
  /*! RAW Record. RR Type. Datatype: u16 */
  ARES_RR_RAW_RR_TYPE = (ARES_REC_TYPE_RAW_RR * 100) + 1,
  /*! RAW Record. RR Data. Datatype: binary */
  ARES_RR_RAW_RR_DATA = (ARES_REC_TYPE_RAW_RR * 100) + 2,
} ares_dns_rr_key_t;

/*! String representation of DNS Record Type
 *
 *  \param[in] type  DNS Record Type
 *  \return string
 */
const char *ares_dns_rec_type_tostr(ares_dns_rec_type_t type);

/*! String representation of DNS Class
 *
 *  \param[in] qclass  DNS Class
 *  \return string
 */
const char *ares_dns_class_tostr(ares_dns_class_t qclass);

/*! String representation of DNS OpCode
 *
 *  \param[in] opcode  DNS OpCode
 *  \return string
 */
const char *ares_dns_opcode_tostr(ares_dns_opcode_t opcode);

/*! String representation of DNS Resource Record Parameter
 *
 *  \param[in] key  DNS Resource Record parameter
 *  \return string
 */
const char *ares_dns_rr_key_tostr(ares_dns_rr_key_t key);


/*! Opaque data type representing a DNS RR (Resource Record) */
struct ares_dns_rr;

/*! Typedef for opaque data type representing a DNS RR (Resource Record) */
typedef struct ares_dns_rr ares_dns_rr_t;

/*! Opaque data type representing a DNS Query Data QD Packet */
struct ares_dns_qd;

/*! Typedef for opaque data type representing a DNS Query Data QD Packet */
typedef struct ares_dns_qd ares_dns_qd_t;

/*! Opaque data type representing a DNS Packet */
struct ares_dns_record;

/*! Typedef for opaque data type representing a DNS Packet */
typedef struct ares_dns_record ares_dns_record_t;


/*! Create a new DNS record object
 *
 *  \param[out] dnsrec  Pointer passed by reference for a newly allocated
 *                      record object.  Must be ares_dns_record_destroy()'d by
 *                      caller.
 *  \param[in]  id      DNS Query ID
 *  \param[in]  flags   DNS Flags from \ares_dns_flags_t
 *  \param[in]  opcode  DNS OpCode (typically ARES_OPCODE_QUERY)
 *  \param[in]  rcode   DNS RCode
 *  \return ARES_SUCCESS on success
 */
ares_status_t     ares_dns_record_create(ares_dns_record_t **dnsrec,
                                         unsigned short id, unsigned short flags,
                                         ares_dns_opcode_t opcode,
                                         ares_dns_rcode_t  rcode);

/*! Destroy a DNS record object
 *
 *  \param[in] dnsrec  Initialized record object
 */
void              ares_dns_record_destroy(ares_dns_record_t *dnsrec);

/*! Get the DNS Query ID
 *
 *  \param[in] dnsrec  Initialized record object
 *  \return DNS query id
 */
unsigned short    ares_dns_record_get_id(const ares_dns_record_t *dnsrec);

/*! Get the DNS Record Flags
 *
 *  \param[in] dnsrec  Initialized record object
 *  \return One or more \ares_dns_flags_t
 */
unsigned short    ares_dns_record_get_flags(const ares_dns_record_t *dnsrec);

/*! Get the DNS Record OpCode
 *
 *  \param[in] dnsrec  Initialized record object
 *  \return opcode
 */
ares_dns_opcode_t ares_dns_record_get_opcode(const ares_dns_record_t *dnsrec);

/*! Get the DNS Record RCode
 *
 *  \param[in] dnsrec  Initialized record object
 *  \return rcode
 */
ares_dns_rcode_t  ares_dns_record_get_rcode(const ares_dns_record_t *dnsrec);

/*! Add a query to the DNS Record.  Typically a record will have only 1
 *  query. Most DNS servers will reject queries with more than 1 question.
 *
 * \param[in] dnsrec  Initialized record object
 * \param[in] name    Name/Hostname of request
 * \param[in] qtype   Type of query
 * \param[in] qclass  Class of query (typically ARES_CLASS_IN)
 * \return ARES_SUCCESS on success
 */
ares_status_t     ares_dns_record_query_add(ares_dns_record_t  *dnsrec,
                                            const char         *name,
                                            ares_dns_rec_type_t qtype,
                                            ares_dns_class_t    qclass);

/*! Get the count of queries in the DNS Record
 *
 * \param[in] dnsrec  Initialized record object
 * \return count of queries
 */
size_t            ares_dns_record_query_cnt(const ares_dns_record_t *dnsrec);

/*! Get the data about the query at the provided index.
 *
 * \param[in]  dnsrec  Initialized record object
 * \param[in]  idx     Index of query
 * \param[out] name    Optional.  Returns name, may pass NULL if not desired.
 * \param[out] qtype   Optional.  Returns record type, may pass NULL.
 * \param[out] qclass  Optional.  Returns class, may pass NULL.
 * \return ARES_SUCCESS on success
 */
ares_status_t     ares_dns_record_query_get(const ares_dns_record_t *dnsrec,
                                            size_t idx, const char **name,
                                            ares_dns_rec_type_t *qtype,
                                            ares_dns_class_t    *qclass);

/*! Get the count of Resource Records in the provided section
 *
 * \param[in] dnsrec  Initialized record object
 * \param[in] sect    Section.  ARES_SECTION_ANSWER is most used.
 * \return count of resource records.
 */
size_t            ares_dns_record_rr_cnt(const ares_dns_record_t *dnsrec,
                                         ares_dns_section_t       sect);


/*! Add a Resource Record to the DNS Record.
 *
 *  \param[out] rr_out   Pointer to created resource record.  This pointer
 *                       is owned by the DNS record itself, this is just made
 *                       available to facilitate adding RR-specific fields.
 *  \param[in]  dnsrec   Initialized record object
 *  \param[in]  sect     Section to add resource record to
 *  \param[in]  name     Resource Record name/hostname
 *  \param[in]  type     Record Type
 *  \param[in]  rclass   Class
 *  \param[in]  ttl      TTL
 *  \return ARES_SUCCESS on success
 */
ares_status_t     ares_dns_record_rr_add(ares_dns_rr_t    **rr_out,
                                         ares_dns_record_t *dnsrec,
                                         ares_dns_section_t sect, const char *name,
                                         ares_dns_rec_type_t type,
                                         ares_dns_class_t rclass, unsigned int ttl);

/*! Fetch a resource record based on the section and index.
 *
 *  \param[in]  dnsrec   Initialized record object
 *  \param[in]  sect     Section for resource record
 *  \param[in]  idx      Index of resource record in section
 *  \param NULL on misuse, otherwise a pointer to the resource record
 */
ares_dns_rr_t    *ares_dns_record_rr_get(ares_dns_record_t *dnsrec,
                                         ares_dns_section_t sect, size_t idx);


/*! Retrieve a list of Resource Record keys that can be set or retrieved for
 *  the Resource record type.
 *
 *  \param[in]  type  Record Type
 *  \param[out] cnt   Number of keys returned
 *  \return array of keys associated with Resource Record
 */
const ares_dns_rr_key_t *ares_dns_rr_get_keys(ares_dns_rec_type_t type,
                                              size_t             *cnt);

/*! Retrieve the datatype associated with a Resource Record key.
 *
 *  \param[in] key   Resource Record Key
 *  \return datatype
 */
ares_dns_datatype_t      ares_dns_rr_key_datatype(ares_dns_rr_key_t key);

/*! Retrieve the DNS Resource Record type associated with a Resource Record key.
 *
 *  \param[in] key   Resource Record Key
 *  \return DNS Resource Record Type
 */
ares_dns_rec_type_t      ares_dns_rr_key_to_rec_type(ares_dns_rr_key_t key);

/*! Retrieve the resource record Name/Hostname
 *
 *  \param[in] rr  Pointer to resource record
 *  \return Name
 */
const char              *ares_dns_rr_get_name(const ares_dns_rr_t *rr);

/*! Retrieve the resource record type
 *
 *  \param[in] rr  Pointer to resource record
 *  \return type
 */
ares_dns_rec_type_t      ares_dns_rr_get_type(const ares_dns_rr_t *rr);

/*! Retrieve the resource record class
 *
 *  \param[in] rr  Pointer to resource record
 *  \return class
 */
ares_dns_class_t         ares_dns_rr_get_class(const ares_dns_rr_t *rr);

/*! Retrieve the resource record TTL
 *
 *  \param[in] rr  Pointer to resource record
 *  \return TTL
 */
unsigned int             ares_dns_rr_get_ttl(const ares_dns_rr_t *rr);

/*! Set ipv4 address data type for specified resource record and key.  Can
 *  only be used on keys with datatype ARES_DATATYPE_INADDR
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \param[in] addr   Pointer to ipv4 address to use.
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares_dns_rr_set_addr(ares_dns_rr_t *dns_rr, ares_dns_rr_key_t key,
                                   const struct in_addr *addr);

/*! Set ipv6 address data type for specified resource record and key.  Can
 *  only be used on keys with datatype ARES_DATATYPE_INADDR6
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \param[in] addr   Pointer to ipv6 address to use.
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares_dns_rr_set_addr6(ares_dns_rr_t              *dns_rr,
                                    ares_dns_rr_key_t           key,
                                    const struct ares_in6_addr *addr);

/*! Set string data for specified resource record and key.  Can
 *  only be used on keys with datatype ARES_DATATYPE_STR
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \param[in] val    Pointer to string to set.
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares_dns_rr_set_str(ares_dns_rr_t *dns_rr, ares_dns_rr_key_t key,
                                  const char *val);

/*! Set 8bit unsigned integer for specified resource record and key.  Can
 *  only be used on keys with datatype ARES_DATATYPE_U8
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \param[in] val    8bit unsigned integer
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares_dns_rr_set_u8(ares_dns_rr_t *dns_rr, ares_dns_rr_key_t key,
                                 unsigned char val);

/*! Set 16bit unsigned integer for specified resource record and key.  Can
 *  only be used on keys with datatype ARES_DATATYPE_U16
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \param[in] val    16bit unsigned integer
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares_dns_rr_set_u16(ares_dns_rr_t *dns_rr, ares_dns_rr_key_t key,
                                  unsigned short val);

/*! Set 32bit unsigned integer for specified resource record and key.  Can
 *  only be used on keys with datatype ARES_DATATYPE_U32
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \param[in] val    32bit unsigned integer
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares_dns_rr_set_u32(ares_dns_rr_t *dns_rr, ares_dns_rr_key_t key,
                                  unsigned int val);

/*! Set binary data for specified resource record and key.  Can
 *  only be used on keys with datatype ARES_DATATYPE_BIN
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \param[in] val    Pointer to binary data.
 *  \param[in] len    Length of binary data
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares_dns_rr_set_bin(ares_dns_rr_t *dns_rr, ares_dns_rr_key_t key,
                                  const unsigned char *val, size_t len);


/*! Retrieve a pointer to the ipv4 address.  Can only be used on keys with
 *  datatype ARES_DATATYPE_INADDR.
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \return pointer to ipv4 address or NULL on error
 */
const struct in_addr       *ares_dns_rr_get_addr(const ares_dns_rr_t *dns_rr,
                                                 ares_dns_rr_key_t    key);

/*! Retrieve a pointer to the ipv6 address.  Can only be used on keys with
 *  datatype ARES_DATATYPE_INADDR6.
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \return pointer to ipv6 address or NULL on error
 */
const struct ares_in6_addr *ares_dns_rr_get_addr6(const ares_dns_rr_t *dns_rr,
                                                  ares_dns_rr_key_t    key);

/*! Retrieve a pointer to the string.  Can only be used on keys with
 *  datatype ARES_DATATYPE_STR.
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \return pointer string or NULL on error
 */
const char                 *ares_dns_rr_get_str(const ares_dns_rr_t *dns_rr,
                                                ares_dns_rr_key_t    key);

/*! Retrieve an 8bit unsigned integer.  Can only be used on keys with
 *  datatype ARES_DATATYPE_U8.
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \return 8bit unsigned integer
 */
unsigned char               ares_dns_rr_get_u8(const ares_dns_rr_t *dns_rr,
                                               ares_dns_rr_key_t    key);

/*! Retrieve an 16bit unsigned integer.  Can only be used on keys with
 *  datatype ARES_DATATYPE_U16.
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \return 16bit unsigned integer
 */
unsigned short              ares_dns_rr_get_u16(const ares_dns_rr_t *dns_rr,
                                                ares_dns_rr_key_t    key);

/*! Retrieve an 32bit unsigned integer.  Can only be used on keys with
 *  datatype ARES_DATATYPE_U32.
 *
 *  \param[in] dns_rr Pointer to resource record
 *  \param[in] key    DNS Resource Record Key
 *  \return 32bit unsigned integer
 */
unsigned int                ares_dns_rr_get_u32(const ares_dns_rr_t *dns_rr,
                                                ares_dns_rr_key_t    key);

/*! Retrieve a pointer to the binary data.  Can only be used on keys with
 *  datatype ARES_DATATYPE_BIN.
 *
 *  \param[in]  dns_rr Pointer to resource record
 *  \param[in]  key    DNS Resource Record Key
 *  \param[out] len    Length of binary data returned
 *  \return pointer binary data or NULL on error
 */
const unsigned char        *ares_dns_rr_get_bin(const ares_dns_rr_t *dns_rr,
                                                ares_dns_rr_key_t key, size_t *len);


/*! Parse a complete DNS message.
 *
 *  \param[in]  buf      pointer to bytes to be parsed
 *  \param[in]  buf_len  Length of buf provided
 *  \param[in]  flags    Flags dictating how the message should be parsed. TBD.
 *  \param[out] dnsrec   Pointer passed by reference for a new DNS record object
 *                       that must be ares_dns_record_destroy()'d by caller.
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares_dns_parse(const unsigned char *buf, size_t buf_len,
                             unsigned int flags, ares_dns_record_t **dnsrec);


/*! @} */

/* ---- PRIVATE BELOW ----- */
ares_bool_t   ares_dns_opcode_isvalid(ares_dns_opcode_t opcode);
ares_bool_t   ares_dns_rcode_isvalid(ares_dns_rcode_t rcode);
ares_bool_t   ares_dns_flags_arevalid(unsigned short flags);
ares_bool_t   ares_dns_rec_type_isvalid(ares_dns_rec_type_t type,
                                        ares_bool_t         is_query);
ares_bool_t   ares_dns_class_isvalid(ares_dns_class_t qclass,
                                     ares_bool_t      is_query);
ares_bool_t   ares_dns_section_isvalid(ares_dns_section_t sect);
ares_status_t ares_dns_rr_set_str_own(ares_dns_rr_t    *dns_rr,
                                      ares_dns_rr_key_t key, char *val);
ares_status_t ares_dns_rr_set_bin_own(ares_dns_rr_t    *dns_rr,
                                      ares_dns_rr_key_t key, unsigned char *val,
                                      size_t len);
ares_status_t ares_dns_record_rr_prealloc(ares_dns_record_t *dnsrec,
                                          ares_dns_section_t sect, size_t cnt);

struct ares_dns_qd {
  char               *name;
  ares_dns_rec_type_t qtype;
  ares_dns_class_t    qclass;
};

typedef struct {
  struct in_addr addr;
} ares__dns_a_t;

typedef struct {
  char *nsdname;
} ares__dns_ns_t;

typedef struct {
  char *cname;
} ares__dns_cname_t;

typedef struct {
  char        *mname;
  char        *rname;
  unsigned int serial;
  unsigned int refresh;
  unsigned int retry;
  unsigned int expire;
  unsigned int minimum;
} ares__dns_soa_t;

typedef struct {
  char *dname;
} ares__dns_ptr_t;

typedef struct {
  char *cpu;
  char *os;
} ares__dns_hinfo_t;

typedef struct {
  unsigned short preference;
  char          *exchange;
} ares__dns_mx_t;

typedef struct {
  char  *data;
  size_t data_len;
} ares__dns_txt_t;

typedef struct {
  struct ares_in6_addr addr;
} ares__dns_aaaa_t;

typedef struct {
  unsigned short priority;
  unsigned short weight;
  unsigned short port;
  char          *target;
} ares__dns_srv_t;

typedef struct {
  unsigned short order;
  unsigned short preference;
  char          *flags;
  char          *services;
  char          *regexp;
  char          *replacement;
} ares__dns_naptr_t;

typedef struct {
  unsigned short udp_size;  /*!< taken from class */
  unsigned char  ext_rcode; /*!< Taken from first 8 bits of ttl */
  unsigned char  version;   /*!< taken from bits 8-16 of ttl */
  unsigned short flags;     /*!< Flags, remaining 16 bits, though only 1
                             *   currently defined */
  /* Remaining data can be multiple:
   *   16bit attribute/code, 16bit length, data
   * not currently supported */
} ares__dns_opt_t;

typedef struct {
  unsigned short priority;
  unsigned short weight;
  char          *target;
} ares__dns_uri_t;

typedef struct {
  unsigned char  critical;
  char          *tag;
  unsigned char *value;
  size_t         value_len;
} ares__dns_caa_t;

/*! Raw, unparsed RR data */
typedef struct {
  unsigned short type;   /*!< Not ares_rec_type_t because it likely isn't one
                          *   of those values since it wasn't parsed */
  unsigned char *data;   /*!< Raw RR data */
  size_t         length; /*!< Length of raw RR data */
} ares__dns_raw_rr_t;

/*! DNS RR data structure */
struct ares_dns_rr {
  char               *name;
  ares_dns_rec_type_t type;
  ares_dns_class_t    rclass;
  unsigned int        ttl;

  union {
    ares__dns_a_t      a;
    ares__dns_ns_t     ns;
    ares__dns_cname_t  cname;
    ares__dns_soa_t    soa;
    ares__dns_ptr_t    ptr;
    ares__dns_hinfo_t  hinfo;
    ares__dns_mx_t     mx;
    ares__dns_txt_t    txt;
    ares__dns_aaaa_t   aaaa;
    ares__dns_srv_t    srv;
    ares__dns_naptr_t  naptr;
    ares__dns_opt_t    opt;
    ares__dns_uri_t    uri;
    ares__dns_caa_t    caa;
    ares__dns_raw_rr_t raw_rr;
  } r;
};

/*! DNS data structure */
struct ares_dns_record {
  unsigned short    id;     /*!< DNS query id */
  unsigned short    flags;  /*!< One or more ares_dns_flags_t */
  ares_dns_opcode_t opcode; /*!< DNS Opcode */
  ares_dns_rcode_t  rcode;  /*!< DNS RCODE */

  ares_dns_qd_t    *qd;
  size_t            qdcount;
  size_t            qdalloc;

  ares_dns_rr_t    *an;
  size_t            ancount;
  size_t            analloc;

  ares_dns_rr_t    *ns;
  size_t            nscount;
  size_t            nsalloc;

  ares_dns_rr_t    *ar;
  size_t            arcount;
  size_t            aralloc;
};


#endif /* __ARES_DNS_RECORD_H */
