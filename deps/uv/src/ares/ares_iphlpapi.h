#ifndef HEADER_CARES_IPHLPAPI_H
#define HEADER_CARES_IPHLPAPI_H

/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2004 - 2011 by Daniel Stenberg et al
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#if defined(USE_WINSOCK)

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN  22
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 65
#endif

/* ---------------------------------- */
#if !defined(_WS2DEF_)           && \
    !defined(__CSADDR_DEFINED__) && \
    !defined(__CSADDR_T_DEFINED)
/* ---------------------------------- */

typedef struct _SOCKET_ADDRESS {
  LPSOCKADDR lpSockaddr;
  INT iSockaddrLength;
} SOCKET_ADDRESS, *PSOCKET_ADDRESS;

typedef struct _CSADDR_INFO {
  SOCKET_ADDRESS LocalAddr;
  SOCKET_ADDRESS RemoteAddr;
  INT iSocketType;
  INT iProtocol;
} CSADDR_INFO, *PCSADDR_INFO;

/* --------------------------------- */
#endif /* ! _WS2DEF_           && \  */
/*        ! __CSADDR_DEFINED__ && \  */
/*        ! __CSADDR_T_DEFINED       */
/* --------------------------------- */

/* ------------------------------- */
#if !defined(IP_ADAPTER_DDNS_ENABLED)
/* ------------------------------- */

#define IP_ADAPTER_ADDRESS_DNS_ELIGIBLE  0x0001
#define IP_ADAPTER_ADDRESS_TRANSIENT     0x0002

#define IP_ADAPTER_DDNS_ENABLED                0x0001
#define IP_ADAPTER_REGISTER_ADAPTER_SUFFIX     0x0002
#define IP_ADAPTER_DHCP_ENABLED                0x0004
#define IP_ADAPTER_RECEIVE_ONLY                0x0008
#define IP_ADAPTER_NO_MULTICAST                0x0010
#define IP_ADAPTER_IPV6_OTHER_STATEFUL_CONFIG  0x0020

#define GAA_FLAG_SKIP_UNICAST        0x0001
#define GAA_FLAG_SKIP_ANYCAST        0x0002
#define GAA_FLAG_SKIP_MULTICAST      0x0004
#define GAA_FLAG_SKIP_DNS_SERVER     0x0008
#define GAA_FLAG_INCLUDE_PREFIX      0x0010
#define GAA_FLAG_SKIP_FRIENDLY_NAME  0x0020

typedef enum {
  IpPrefixOriginOther = 0,
  IpPrefixOriginManual,
  IpPrefixOriginWellKnown,
  IpPrefixOriginDhcp,
  IpPrefixOriginRouterAdvertisement
} IP_PREFIX_ORIGIN;

typedef enum {
  IpSuffixOriginOther = 0,
  IpSuffixOriginManual,
  IpSuffixOriginWellKnown,
  IpSuffixOriginDhcp,
  IpSuffixOriginLinkLayerAddress,
  IpSuffixOriginRandom
} IP_SUFFIX_ORIGIN;

typedef enum {
  IpDadStateInvalid = 0,
  IpDadStateTentative,
  IpDadStateDuplicate,
  IpDadStateDeprecated,
  IpDadStatePreferred
} IP_DAD_STATE;

typedef enum {
  IfOperStatusUp = 1,
  IfOperStatusDown,
  IfOperStatusTesting,
  IfOperStatusUnknown,
  IfOperStatusDormant,
  IfOperStatusNotPresent,
  IfOperStatusLowerLayerDown
} IF_OPER_STATUS;

typedef enum {
  ScopeLevelInterface    = 0x0001,
  ScopeLevelLink         = 0x0002,
  ScopeLevelSubnet       = 0x0003,
  ScopeLevelAdmin        = 0x0004,
  ScopeLevelSite         = 0x0005,
  ScopeLevelOrganization = 0x0008,
  ScopeLevelGlobal       = 0x000E
} SCOPE_LEVEL;

typedef struct _IP_ADAPTER_UNICAST_ADDRESS {
  union {
    ULONGLONG Alignment;
    struct {
      ULONG Length;
      DWORD Flags;
    } s;
  } u;
  struct _IP_ADAPTER_UNICAST_ADDRESS *Next;
  SOCKET_ADDRESS Address;
  IP_PREFIX_ORIGIN PrefixOrigin;
  IP_SUFFIX_ORIGIN SuffixOrigin;
  IP_DAD_STATE DadState;
  ULONG ValidLifetime;
  ULONG PreferredLifetime;
  ULONG LeaseLifetime;
} IP_ADAPTER_UNICAST_ADDRESS, *PIP_ADAPTER_UNICAST_ADDRESS;

typedef struct _IP_ADAPTER_ANYCAST_ADDRESS {
  union {
    ULONGLONG Alignment;
    struct {
      ULONG Length;
      DWORD Flags;
    } s;
  } u;
  struct _IP_ADAPTER_ANYCAST_ADDRESS *Next;
  SOCKET_ADDRESS Address;
} IP_ADAPTER_ANYCAST_ADDRESS, *PIP_ADAPTER_ANYCAST_ADDRESS;

typedef struct _IP_ADAPTER_MULTICAST_ADDRESS {
  union {
    ULONGLONG Alignment;
    struct {
      ULONG Length;
      DWORD Flags;
    } s;
  } u;
  struct _IP_ADAPTER_MULTICAST_ADDRESS *Next;
  SOCKET_ADDRESS Address;
} IP_ADAPTER_MULTICAST_ADDRESS, *PIP_ADAPTER_MULTICAST_ADDRESS;

typedef struct _IP_ADAPTER_DNS_SERVER_ADDRESS {
  union {
    ULONGLONG Alignment;
    struct {
      ULONG Length;
      DWORD Reserved;
    } s;
  } u;
  struct _IP_ADAPTER_DNS_SERVER_ADDRESS *Next;
  SOCKET_ADDRESS Address;
} IP_ADAPTER_DNS_SERVER_ADDRESS, *PIP_ADAPTER_DNS_SERVER_ADDRESS;

typedef struct _IP_ADAPTER_PREFIX {
  union {
    ULONGLONG Alignment;
    struct {
      ULONG Length;
      DWORD Flags;
    } s;
  } u;
  struct _IP_ADAPTER_PREFIX *Next;
  SOCKET_ADDRESS Address;
  ULONG PrefixLength;
} IP_ADAPTER_PREFIX, *PIP_ADAPTER_PREFIX;

typedef struct _IP_ADAPTER_ADDRESSES {
  union {
    ULONGLONG Alignment;
    struct {
      ULONG Length;
      DWORD IfIndex;
    } s;
  } u;
  struct _IP_ADAPTER_ADDRESSES *Next;
  PCHAR AdapterName;
  PIP_ADAPTER_UNICAST_ADDRESS FirstUnicastAddress;
  PIP_ADAPTER_ANYCAST_ADDRESS FirstAnycastAddress;
  PIP_ADAPTER_MULTICAST_ADDRESS FirstMulticastAddress;
  PIP_ADAPTER_DNS_SERVER_ADDRESS FirstDnsServerAddress;
  PWCHAR DnsSuffix;
  PWCHAR Description;
  PWCHAR FriendlyName;
  BYTE PhysicalAddress[MAX_ADAPTER_ADDRESS_LENGTH];
  DWORD PhysicalAddressLength;
  DWORD Flags;
  DWORD Mtu;
  DWORD IfType;
  IF_OPER_STATUS OperStatus;
  DWORD Ipv6IfIndex;
  DWORD ZoneIndices[16];
  PIP_ADAPTER_PREFIX FirstPrefix;
} IP_ADAPTER_ADDRESSES, *PIP_ADAPTER_ADDRESSES;

/* -------------------------------- */
#endif /* ! IP_ADAPTER_DDNS_ENABLED */
/* -------------------------------- */

#endif /* USE_WINSOCK */

#endif /* HEADER_CARES_IPHLPAPI_H */
