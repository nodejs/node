'use strict';

const { ArrayIsArray } = primordials;

const {
  DNSWrap,
  GETDNS_TRANSPORT_UDP,
  GETDNS_TRANSPORT_TCP,
  GETDNS_TRANSPORT_TLS,
} = internalBinding('dns');
const { isIP } = require('internal/net');
const { toASCII } = require('internal/idna');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_IP_ADDRESS,
  },
} = require('internal/errors');
const { validateString } = require('internal/validators');

const RRTYPE = {
  __proto__: null,
  A: 1,
  NS: 2,
  MD: 3,
  MF: 4,
  CNAME: 5,
  SOA: 6,
  MB: 7,
  MG: 8,
  MR: 9,
  NULL: 10,
  WKS: 11,
  PTR: 12,
  HINFO: 13,
  MINFO: 14,
  MX: 15,
  TXT: 16,
  RP: 17,
  AFSDB: 18,
  X25: 19,
  ISDN: 20,
  RT: 21,
  NSAP: 22,
  NSAP_PTR: 23,
  SIG: 24,
  KEY: 25,
  PX: 26,
  GPOS: 27,
  AAAA: 28,
  LOC: 29,
  NXT: 30,
  EID: 31,
  NIMLOC: 32,
  SRV: 33,
  ATMA: 34,
  NAPTR: 35,
  KX: 36,
  CERT: 37,
  A6: 38,
  DNAME: 39,
  SINK: 40,
  OPT: 41,
  APL: 42,
  DS: 43,
  SSHFP: 44,
  IPSECKEY: 45,
  RRSIG: 46,
  NSEC: 47,
  DNSKEY: 48,
  DHCID: 49,
  NSEC3: 50,
  NSEC3PARAM: 51,
  TLSA: 52,
  SMIMEA: 53,
  HIP: 55,
  NINFO: 56,
  RKEY: 57,
  TALINK: 58,
  CDS: 59,
  CDNSKEY: 60,
  OPENPGPKEY: 61,
  CSYNC: 62,
  ZONEMD: 63,
  SPF: 99,
  UINFO: 100,
  UID: 101,
  GID: 102,
  UNSPEC: 103,
  NID: 104,
  L32: 105,
  L64: 106,
  LP: 107,
  EUI48: 108,
  EUI64: 109,
  TKEY: 249,
  TSIG: 250,
  IXFR: 251,
  AXFR: 252,
  MAILB: 253,
  MAILA: 254,
  ANY: 255,
  URI: 256,
  CAA: 257,
  AVC: 258,
  DOA: 259,
  AMTRELAY: 260,
  TA: 32768,
  DLV: 32769,
};

const IPv6RE = /^\[([^[\]]*)\]/;
const addrSplitRE = /(^.+?)(?::(\d+))?$/;

class ModernResolver {
  #wrap;
  constructor() {
    this.#wrap = new DNSWrap();
  }

  setUpstreamRecursiveServers(servers) {
    const newSet = [];

    let tlsEnabled = false;
    servers.forEach((serv, index) => {
      let tlsHostname;
      if (ArrayIsArray(serv)) {
        tlsHostname = serv[1];
        serv = serv[0];
        tlsEnabled = true;
        if (typeof serv !== 'string') {
          throw new ERR_INVALID_ARG_TYPE(`servers[${index}][0]`,
                                         'string', serv);
        }
        if (typeof tlsHostname !== 'string') {
          throw new ERR_INVALID_ARG_TYPE(`servers[${index}][1]`,
                                         'string', serv);
        }
      } else if (typeof serv !== 'string') {
        throw new ERR_INVALID_ARG_TYPE(`servers[${index}]`, 'string', serv);
      }

      const PORT = tlsHostname === undefined ? 53 : 853;
      let ipVersion = isIP(serv);

      if (ipVersion !== 0)
        return newSet.push([serv, PORT, tlsHostname]);

      const match = serv.match(IPv6RE);

      // Check for an IPv6 in brackets.
      if (match) {
        ipVersion = isIP(match[1]);

        if (ipVersion !== 0) {
          const port =
            parseInt(serv.replace(addrSplitRE, '$2')) || PORT;
          return newSet.push([match[1], port, tlsHostname]);
        }
      }

      // addr::port
      const addrSplitMatch = serv.match(addrSplitRE);

      if (addrSplitMatch) {
        const hostIP = addrSplitMatch[1];
        const port = addrSplitMatch[2] || PORT;

        ipVersion = isIP(hostIP);

        if (ipVersion !== 0) {
          return newSet.push([hostIP, parseInt(port), tlsHostname]);
        }
      }

      throw new ERR_INVALID_IP_ADDRESS(serv);
    });

    this.#wrap.setUpstreamRecursiveServers(newSet);
    if (tlsEnabled) {
      this.#wrap.setDNSTransportList([
        GETDNS_TRANSPORT_TLS,
      ]);
    } else {
      this.#wrap.setDNSTransportList([
        GETDNS_TRANSPORT_UDP,
        GETDNS_TRANSPORT_TCP,
      ]);
    }
  }

  getUpstreamRecursiveServers() {
    return this.#wrap.getUpstreamRecursiveServers();
  }

  async getAddresses(name) {
    validateString(name, 'name');
    const { addresses } = await this.#wrap.getAddresses(toASCII(name));
    return addresses;
  }

  async getServices(name) {
    validateString(name, 'name');
    return this.#wrap.getServices(toASCII(name));
  }

  async getHostnames(address) {
    validateString(address, 'address');
    return this.#wrap.getHostnames(address);
  }

  async resolve(rr, name) {
    validateString(rr, 'rr');
    if (!RRTYPE[rr]) {
      throw ERR_INVALID_ARG_TYPE('rr', 'resource record type', rr);
    }
    validateString(name, 'name');
    return this.#wrap.getGeneral(toASCII(name), RRTYPE[rr]);
  }
}

let defaultResolver;
function getDefaultModernResolver() {
  if (defaultResolver === undefined) {
    defaultResolver = new ModernResolver();
  }
  return defaultResolver;
}

module.exports = {
  RRTYPE,
  ModernResolver,
  getDefaultModernResolver,
};
