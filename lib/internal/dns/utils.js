'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  FunctionPrototypeBind,
  NumberParseInt,
  StringPrototypeMatch,
  StringPrototypeReplace,
} = primordials;

const errors = require('internal/errors');
const { isIP } = require('internal/net');
const { getOptionValue } = require('internal/options');
const {
  validateArray,
  validateInt32,
  validateOneOf,
  validateString,
} = require('internal/validators');
const {
  ChannelWrap,
  strerror,
  AI_ADDRCONFIG,
  AI_ALL,
  AI_V4MAPPED,
} = internalBinding('cares_wrap');
const IANA_DNS_PORT = 53;
const IPv6RE = /^\[([^[\]]*)\]/;
const addrSplitRE = /(^.+?)(?::(\d+))?$/;
const {
  ERR_DNS_SET_SERVERS_FAILED,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_IP_ADDRESS,
} = errors.codes;

function validateTimeout(options) {
  const { timeout = -1 } = { ...options };
  validateInt32(timeout, 'options.timeout', -1, 2 ** 31 - 1);
  return timeout;
}

// Resolver instances correspond 1:1 to c-ares channels.
class Resolver {
  constructor(options = undefined) {
    const timeout = validateTimeout(options);
    this._handle = new ChannelWrap(timeout);
  }

  cancel() {
    this._handle.cancel();
  }

  getServers() {
    return ArrayPrototypeMap(this._handle.getServers(), (val) => {
      if (!val[1] || val[1] === IANA_DNS_PORT)
        return val[0];

      const host = isIP(val[0]) === 6 ? `[${val[0]}]` : val[0];
      return `${host}:${val[1]}`;
    });
  }

  setServers(servers) {
    validateArray(servers, 'servers');

    // Cache the original servers because in the event of an error while
    // setting the servers, c-ares won't have any servers available for
    // resolution.
    const orig = this._handle.getServers();
    const newSet = [];

    ArrayPrototypeForEach(servers, (serv, index) => {
      validateString(serv, `servers[${index}]`);
      let ipVersion = isIP(serv);

      if (ipVersion !== 0)
        return ArrayPrototypePush(newSet, [ipVersion, serv, IANA_DNS_PORT]);

      const match = StringPrototypeMatch(serv, IPv6RE);

      // Check for an IPv6 in brackets.
      if (match) {
        ipVersion = isIP(match[1]);

        if (ipVersion !== 0) {
          const port = NumberParseInt(
            StringPrototypeReplace(serv, addrSplitRE, '$2')) || IANA_DNS_PORT;
          return ArrayPrototypePush(newSet, [ipVersion, match[1], port]);
        }
      }

      // addr::port
      const addrSplitMatch = StringPrototypeMatch(serv, addrSplitRE);

      if (addrSplitMatch) {
        const hostIP = addrSplitMatch[1];
        const port = addrSplitMatch[2] || IANA_DNS_PORT;

        ipVersion = isIP(hostIP);

        if (ipVersion !== 0) {
          return ArrayPrototypePush(
            newSet, [ipVersion, hostIP, NumberParseInt(port)]);
        }
      }

      throw new ERR_INVALID_IP_ADDRESS(serv);
    });

    const errorNumber = this._handle.setServers(newSet);

    if (errorNumber !== 0) {
      // Reset the servers to the old servers, because ares probably unset them.
      this._handle.setServers(ArrayPrototypeJoin(orig, ','));
      const err = strerror(errorNumber);
      throw new ERR_DNS_SET_SERVERS_FAILED(err, servers);
    }
  }

  setLocalAddress(ipv4, ipv6) {
    validateString(ipv4, 'ipv4');

    if (typeof ipv6 !== 'string' && ipv6 !== undefined) {
      throw new ERR_INVALID_ARG_TYPE('ipv6', ['String', 'undefined'], ipv6);
    }

    this._handle.setLocalAddress(ipv4, ipv6);
  }
}

let defaultResolver = new Resolver();
const resolverKeys = [
  'getServers',
  'resolve',
  'resolve4',
  'resolve6',
  'resolveAny',
  'resolveCaa',
  'resolveCname',
  'resolveMx',
  'resolveNaptr',
  'resolveNs',
  'resolvePtr',
  'resolveSoa',
  'resolveSrv',
  'resolveTxt',
  'reverse',
];

function getDefaultResolver() {
  return defaultResolver;
}

function setDefaultResolver(resolver) {
  defaultResolver = resolver;
}

function bindDefaultResolver(target, source) {
  ArrayPrototypeForEach(resolverKeys, (key) => {
    target[key] = FunctionPrototypeBind(source[key], defaultResolver);
  });
}

function validateHints(hints) {
  if ((hints & ~(AI_ADDRCONFIG | AI_ALL | AI_V4MAPPED)) !== 0) {
    throw new ERR_INVALID_ARG_VALUE('hints', hints);
  }
}

let invalidHostnameWarningEmitted = false;

function emitInvalidHostnameWarning(hostname) {
  if (invalidHostnameWarningEmitted) {
    return;
  }
  invalidHostnameWarningEmitted = true;
  process.emitWarning(
    `The provided hostname "${hostname}" is not a valid ` +
    'hostname, and is supported in the dns module solely for compatibility.',
    'DeprecationWarning',
    'DEP0118'
  );
}

let dnsOrder = getOptionValue('--dns-result-order') || 'ipv4first';

function getDefaultVerbatim() {
  switch (dnsOrder) {
    case 'verbatim':
      return true;
    case 'ipv4first':
    default:
      return false;
  }
}

function setDefaultResultOrder(value) {
  validateOneOf(value, 'dnsOrder', ['verbatim', 'ipv4first']);
  dnsOrder = value;
}

module.exports = {
  bindDefaultResolver,
  getDefaultResolver,
  setDefaultResolver,
  validateHints,
  validateTimeout,
  Resolver,
  emitInvalidHostnameWarning,
  getDefaultVerbatim,
  setDefaultResultOrder,
};
