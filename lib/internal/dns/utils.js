'use strict';

const errors = require('internal/errors');
const { isIP } = require('internal/net');
const { ModernResolver } = require('internal/dns/modern');
const IANA_DNS_PORT = 53;
const {
  ERR_INVALID_OPT_VALUE
} = errors.codes;

class Resolver {
  constructor() {
    this._handle = new ModernResolver();
  }

  cancel() {
    this._handle.cancelAllTransactions();
  }

  getServers() {
    return this._handle.getUpstreamRecursiveServers().map((val) => {
      if (!val[1] || val[1] === IANA_DNS_PORT)
        return val[0];

      const host = isIP(val[0]) === 6 ? `[${val[0]}]` : val[0];
      return `${host}:${val[1]}`;
    });
  }

  setServers(servers) {
    this._handle.setUpstreamRecursiveServers(servers);
  }
}

let defaultResolver = new Resolver();
const resolverKeys = [
  'getServers',
  'resolve',
  'resolve4',
  'resolve6',
  'resolveAny',
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
  resolverKeys.forEach((key) => {
    target[key] = source[key].bind(defaultResolver);
  });
}

const AI_ADDRCONFIG = 1 << 0;
const AI_ALL = 1 << 1;
const AI_V4MAPPED = 1 << 2;
function validateHints(hints) {
  if ((hints & ~(AI_ADDRCONFIG | AI_ALL | AI_V4MAPPED)) !== 0) {
    throw new ERR_INVALID_OPT_VALUE('hints', hints);
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

module.exports = {
  bindDefaultResolver,
  getDefaultResolver,
  setDefaultResolver,
  validateHints,
  Resolver,
  emitInvalidHostnameWarning,
  AI_ADDRCONFIG,
  AI_ALL,
  AI_V4MAPPED,
};
