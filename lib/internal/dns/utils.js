'use strict';
const errors = require('internal/errors');
const { isIP } = require('internal/net');
const {
  ChannelWrap,
  strerror,
  AI_ADDRCONFIG,
  AI_V4MAPPED
} = process.binding('cares_wrap');
const IANA_DNS_PORT = 53;
const IPv6RE = /^\[([^[\]]*)\]/;
const addrSplitRE = /(^.+?)(?::(\d+))?$/;
const {
  ERR_DNS_SET_SERVERS_FAILED,
  ERR_INVALID_IP_ADDRESS,
  ERR_INVALID_OPT_VALUE
} = errors.codes;

// Resolver instances correspond 1:1 to c-ares channels.
class Resolver {
  constructor() {
    this._handle = new ChannelWrap();
  }

  cancel() {
    this._handle.cancel();
  }

  getServers() {
    return this._handle.getServers().map((val) => {
      if (!val[1] || val[1] === IANA_DNS_PORT)
        return val[0];

      const host = isIP(val[0]) === 6 ? `[${val[0]}]` : val[0];
      return `${host}:${val[1]}`;
    });
  }

  setServers(servers) {
    // Cache the original servers because in the event of an error while
    // setting the servers, c-ares won't have any servers available for
    // resolution.
    const orig = this._handle.getServers();
    const newSet = [];

    servers.forEach((serv) => {
      var ipVersion = isIP(serv);

      if (ipVersion !== 0)
        return newSet.push([ipVersion, serv, IANA_DNS_PORT]);

      const match = serv.match(IPv6RE);

      // Check for an IPv6 in brackets.
      if (match) {
        ipVersion = isIP(match[1]);

        if (ipVersion !== 0) {
          const port =
            parseInt(serv.replace(addrSplitRE, '$2')) ||
            IANA_DNS_PORT;
          return newSet.push([ipVersion, match[1], port]);
        }
      }

      // addr::port
      const addrSplitMatch = serv.match(addrSplitRE);

      if (addrSplitMatch) {
        const hostIP = addrSplitMatch[1];
        const port = addrSplitMatch[2] || IANA_DNS_PORT;

        ipVersion = isIP(hostIP);

        if (ipVersion !== 0) {
          return newSet.push([ipVersion, hostIP, parseInt(port)]);
        }
      }

      throw new ERR_INVALID_IP_ADDRESS(serv);
    });

    const errorNumber = this._handle.setServers(newSet);

    if (errorNumber !== 0) {
      // Reset the servers to the old servers, because ares probably unset them.
      this._handle.setServers(orig.join(','));
      const err = strerror(errorNumber);
      throw new ERR_DNS_SET_SERVERS_FAILED(err, servers);
    }
  }
}

let defaultResolver = new Resolver();
const resolverKeys = [
  'getServers',
  'resolve',
  'resolveAny',
  'resolve4',
  'resolve6',
  'resolveCname',
  'resolveMx',
  'resolveNs',
  'resolveTxt',
  'resolveSrv',
  'resolvePtr',
  'resolveNaptr',
  'resolveSoa',
  'reverse'
];

function getDefaultResolver() {
  return defaultResolver;
}

function setDefaultResolver(resolver) {
  defaultResolver = resolver;
}

function bindDefaultResolver(target, source) {
  resolverKeys.forEach((key) => {
    target[key] = source[key].bind(defaultResolver);
  });
}

function validateHints(hints) {
  if (hints !== 0 &&
      hints !== AI_ADDRCONFIG &&
      hints !== AI_V4MAPPED &&
      hints !== (AI_ADDRCONFIG | AI_V4MAPPED)) {
    throw new ERR_INVALID_OPT_VALUE('hints', hints);
  }
}

module.exports = {
  bindDefaultResolver,
  getDefaultResolver,
  setDefaultResolver,
  validateHints,
  Resolver
};
