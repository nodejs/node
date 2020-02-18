'use strict';

const {
  ObjectCreate,
  ObjectDefineProperty,
  Promise,
} = primordials;

const {
  bindDefaultResolver,
  Resolver: CallbackResolver,
  validateHints,
  emitInvalidHostnameWarning,
} = require('internal/dns/utils');
const { codes, dnsException } = require('internal/errors');
const { toASCII } = require('internal/idna');
const { isIP } = require('internal/net');
const {
  getaddrinfo,
  getnameinfo,
  ChannelWrap,
  GetAddrInfoReqWrap,
  GetNameInfoReqWrap,
  QueryReqWrap
} = internalBinding('cares_wrap');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_OPT_VALUE,
  ERR_MISSING_ARGS,
} = codes;
const {
  validatePort,
  validateString
} = require('internal/validators');

function onlookup(err, addresses) {
  if (err) {
    this.reject(dnsException(err, 'getaddrinfo', this.hostname));
    return;
  }

  const family = this.family ? this.family : isIP(addresses[0]);
  this.resolve({ address: addresses[0], family });
}

function onlookupall(err, addresses) {
  if (err) {
    this.reject(dnsException(err, 'getaddrinfo', this.hostname));
    return;
  }

  const family = this.family;

  for (var i = 0; i < addresses.length; i++) {
    const address = addresses[i];

    addresses[i] = {
      address,
      family: family ? family : isIP(addresses[i])
    };
  }

  this.resolve(addresses);
}

function createLookupPromise(family, hostname, all, hints, verbatim) {
  return new Promise((resolve, reject) => {
    if (!hostname) {
      emitInvalidHostnameWarning(hostname);
      if (all)
        resolve([]);
      else
        resolve({ address: null, family: family === 6 ? 6 : 4 });

      return;
    }

    const matchedFamily = isIP(hostname);

    if (matchedFamily !== 0) {
      const result = { address: hostname, family: matchedFamily };
      if (all)
        resolve([result]);
      else
        resolve(result);

      return;
    }

    const req = new GetAddrInfoReqWrap();

    req.family = family;
    req.hostname = hostname;
    req.oncomplete = all ? onlookupall : onlookup;
    req.resolve = resolve;
    req.reject = reject;

    const err = getaddrinfo(req, toASCII(hostname), family, hints, verbatim);

    if (err) {
      reject(dnsException(err, 'getaddrinfo', hostname));
    }
  });
}

function lookup(hostname, options) {
  var hints = 0;
  var family = -1;
  var all = false;
  var verbatim = false;

  // Parse arguments
  if (hostname && typeof hostname !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('hostname', 'string', hostname);
  } else if (options !== null && typeof options === 'object') {
    hints = options.hints >>> 0;
    family = options.family >>> 0;
    all = options.all === true;
    verbatim = options.verbatim === true;

    validateHints(hints);
  } else {
    family = options >>> 0;
  }

  if (family !== 0 && family !== 4 && family !== 6)
    throw new ERR_INVALID_OPT_VALUE('family', family);

  return createLookupPromise(family, hostname, all, hints, verbatim);
}


function onlookupservice(err, hostname, service) {
  if (err) {
    this.reject(dnsException(err, 'getnameinfo', this.host));
    return;
  }

  this.resolve({ hostname, service });
}

function createLookupServicePromise(hostname, port) {
  return new Promise((resolve, reject) => {
    const req = new GetNameInfoReqWrap();

    req.hostname = hostname;
    req.port = port;
    req.oncomplete = onlookupservice;
    req.resolve = resolve;
    req.reject = reject;

    const err = getnameinfo(req, hostname, port);

    if (err)
      reject(dnsException(err, 'getnameinfo', hostname));
  });
}

function lookupService(address, port) {
  if (arguments.length !== 2)
    throw new ERR_MISSING_ARGS('address', 'port');

  if (isIP(address) === 0)
    throw new ERR_INVALID_OPT_VALUE('address', address);

  validatePort(port);

  return createLookupServicePromise(address, +port);
}


function onresolve(err, result, ttls) {
  if (err) {
    this.reject(dnsException(err, this.bindingName, this.hostname));
    return;
  }

  if (ttls && this.ttl)
    result = result.map((address, index) => ({ address, ttl: ttls[index] }));

  this.resolve(result);
}

function createResolverPromise(resolver, bindingName, hostname, ttl) {
  return new Promise((resolve, reject) => {
    const req = new QueryReqWrap();

    req.bindingName = bindingName;
    req.hostname = hostname;
    req.oncomplete = onresolve;
    req.resolve = resolve;
    req.reject = reject;
    req.ttl = ttl;

    const err = resolver._handle[bindingName](req, toASCII(hostname));

    if (err)
      reject(dnsException(err, bindingName, hostname));
  });
}

function resolver(bindingName) {
  function query(name, options) {
    validateString(name, 'name');

    const ttl = !!(options && options.ttl);
    return createResolverPromise(this, bindingName, name, ttl);
  }

  ObjectDefineProperty(query, 'name', { value: bindingName });
  return query;
}


const resolveMap = ObjectCreate(null);

// Resolver instances correspond 1:1 to c-ares channels.
class Resolver {
  constructor() {
    this._handle = new ChannelWrap();
  }
}

Resolver.prototype.getServers = CallbackResolver.prototype.getServers;
Resolver.prototype.setServers = CallbackResolver.prototype.setServers;
Resolver.prototype.resolveAny = resolveMap.ANY = resolver('queryAny');
Resolver.prototype.resolve4 = resolveMap.A = resolver('queryA');
Resolver.prototype.resolve6 = resolveMap.AAAA = resolver('queryAaaa');
Resolver.prototype.resolveCname = resolveMap.CNAME = resolver('queryCname');
Resolver.prototype.resolveMx = resolveMap.MX = resolver('queryMx');
Resolver.prototype.resolveNs = resolveMap.NS = resolver('queryNs');
Resolver.prototype.resolveTxt = resolveMap.TXT = resolver('queryTxt');
Resolver.prototype.resolveSrv = resolveMap.SRV = resolver('querySrv');
Resolver.prototype.resolvePtr = resolveMap.PTR = resolver('queryPtr');
Resolver.prototype.resolveNaptr = resolveMap.NAPTR = resolver('queryNaptr');
Resolver.prototype.resolveSoa = resolveMap.SOA = resolver('querySoa');
Resolver.prototype.reverse = resolver('getHostByAddr');
Resolver.prototype.resolve = function resolve(hostname, rrtype) {
  var resolver;

  if (typeof rrtype === 'string') {
    resolver = resolveMap[rrtype];

    if (typeof resolver !== 'function')
      throw new ERR_INVALID_OPT_VALUE('rrtype', rrtype);
  } else if (rrtype === undefined) {
    resolver = resolveMap.A;
  } else {
    throw new ERR_INVALID_ARG_TYPE('rrtype', 'string', rrtype);
  }

  return resolver.call(this, hostname);
};


module.exports = { lookup, lookupService, Resolver };
bindDefaultResolver(module.exports, Resolver.prototype);
