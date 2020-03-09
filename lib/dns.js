// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const {
  ObjectCreate,
  ObjectDefineProperties,
  ObjectDefineProperty,
} = primordials;

const cares = internalBinding('cares_wrap');
const { toASCII } = require('internal/idna');
const { isIP } = require('internal/net');
const { customPromisifyArgs } = require('internal/util');
const errors = require('internal/errors');
const {
  bindDefaultResolver,
  getDefaultResolver,
  setDefaultResolver,
  Resolver,
  validateHints,
  emitInvalidHostnameWarning,
} = require('internal/dns/utils');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_INVALID_OPT_VALUE,
  ERR_MISSING_ARGS,
} = errors.codes;
const {
  validatePort,
  validateString,
} = require('internal/validators');

const {
  GetAddrInfoReqWrap,
  GetNameInfoReqWrap,
  QueryReqWrap,
} = cares;

const dnsException = errors.dnsException;

let promises = null; // Lazy loaded

function onlookup(err, addresses) {
  if (err) {
    return this.callback(dnsException(err, 'getaddrinfo', this.hostname));
  }
  this.callback(null, addresses[0], this.family || isIP(addresses[0]));
}


function onlookupall(err, addresses) {
  if (err) {
    return this.callback(dnsException(err, 'getaddrinfo', this.hostname));
  }

  const family = this.family;
  for (let i = 0; i < addresses.length; i++) {
    const addr = addresses[i];
    addresses[i] = {
      address: addr,
      family: family || isIP(addr)
    };
  }

  this.callback(null, addresses);
}


// Easy DNS A/AAAA look up
// lookup(hostname, [options,] callback)
function lookup(hostname, options, callback) {
  let hints = 0;
  let family = -1;
  let all = false;
  let verbatim = false;

  // Parse arguments
  if (hostname && typeof hostname !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('hostname', 'string', hostname);
  } else if (typeof options === 'function') {
    callback = options;
    family = 0;
  } else if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK(callback);
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

  if (!hostname) {
    emitInvalidHostnameWarning(hostname);
    if (all) {
      process.nextTick(callback, null, []);
    } else {
      process.nextTick(callback, null, null, family === 6 ? 6 : 4);
    }
    return {};
  }

  const matchedFamily = isIP(hostname);
  if (matchedFamily) {
    if (all) {
      process.nextTick(
        callback, null, [{ address: hostname, family: matchedFamily }]);
    } else {
      process.nextTick(callback, null, hostname, matchedFamily);
    }
    return {};
  }

  const req = new GetAddrInfoReqWrap();
  req.callback = callback;
  req.family = family;
  req.hostname = hostname;
  req.oncomplete = all ? onlookupall : onlookup;

  const err = cares.getaddrinfo(
    req, toASCII(hostname), family, hints, verbatim
  );
  if (err) {
    process.nextTick(callback, dnsException(err, 'getaddrinfo', hostname));
    return {};
  }
  return req;
}

ObjectDefineProperty(lookup, customPromisifyArgs,
                     { value: ['address', 'family'], enumerable: false });


function onlookupservice(err, hostname, service) {
  if (err)
    return this.callback(dnsException(err, 'getnameinfo', this.hostname));

  this.callback(null, hostname, service);
}


function lookupService(address, port, callback) {
  if (arguments.length !== 3)
    throw new ERR_MISSING_ARGS('address', 'port', 'callback');

  if (isIP(address) === 0)
    throw new ERR_INVALID_OPT_VALUE('address', address);

  validatePort(port);

  if (typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  port = +port;

  const req = new GetNameInfoReqWrap();
  req.callback = callback;
  req.hostname = address;
  req.port = port;
  req.oncomplete = onlookupservice;

  const err = cares.getnameinfo(req, address, port);
  if (err) throw dnsException(err, 'getnameinfo', address);
  return req;
}

ObjectDefineProperty(lookupService, customPromisifyArgs,
                     { value: ['hostname', 'service'], enumerable: false });


function onresolve(err, result, ttls) {
  if (ttls && this.ttl)
    result = result.map((address, index) => ({ address, ttl: ttls[index] }));

  if (err)
    this.callback(dnsException(err, this.bindingName, this.hostname));
  else
    this.callback(null, result);
}

function resolver(bindingName) {
  function query(name, /* options, */ callback) {
    let options;
    if (arguments.length > 2) {
      options = callback;
      callback = arguments[2];
    }

    validateString(name, 'name');
    if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
    }

    const req = new QueryReqWrap();
    req.bindingName = bindingName;
    req.callback = callback;
    req.hostname = name;
    req.oncomplete = onresolve;
    req.ttl = !!(options && options.ttl);
    const err = this._handle[bindingName](req, toASCII(name));
    if (err) throw dnsException(err, bindingName, name);
    return req;
  }
  ObjectDefineProperty(query, 'name', { value: bindingName });
  return query;
}

const resolveMap = ObjectCreate(null);
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

Resolver.prototype.resolve = resolve;

function resolve(hostname, rrtype, callback) {
  let resolver;
  if (typeof rrtype === 'string') {
    resolver = resolveMap[rrtype];
  } else if (typeof rrtype === 'function') {
    resolver = resolveMap.A;
    callback = rrtype;
  } else {
    throw new ERR_INVALID_ARG_TYPE('rrtype', 'string', rrtype);
  }

  if (typeof resolver === 'function') {
    return resolver.call(this, hostname, callback);
  } else {
    throw new ERR_INVALID_OPT_VALUE('rrtype', rrtype);
  }
}

function defaultResolverSetServers(servers) {
  const resolver = new Resolver();

  resolver.setServers(servers);
  setDefaultResolver(resolver);
  bindDefaultResolver(module.exports, Resolver.prototype);

  if (promises !== null)
    bindDefaultResolver(promises, promises.Resolver.prototype);
}

module.exports = {
  lookup,
  lookupService,

  Resolver,
  setServers: defaultResolverSetServers,

  // uv_getaddrinfo flags
  ADDRCONFIG: cares.AI_ADDRCONFIG,
  ALL: cares.AI_ALL,
  V4MAPPED: cares.AI_V4MAPPED,

  // ERROR CODES
  NODATA: 'ENODATA',
  FORMERR: 'EFORMERR',
  SERVFAIL: 'ESERVFAIL',
  NOTFOUND: 'ENOTFOUND',
  NOTIMP: 'ENOTIMP',
  REFUSED: 'EREFUSED',
  BADQUERY: 'EBADQUERY',
  BADNAME: 'EBADNAME',
  BADFAMILY: 'EBADFAMILY',
  BADRESP: 'EBADRESP',
  CONNREFUSED: 'ECONNREFUSED',
  TIMEOUT: 'ETIMEOUT',
  EOF: 'EOF',
  FILE: 'EFILE',
  NOMEM: 'ENOMEM',
  DESTRUCTION: 'EDESTRUCTION',
  BADSTR: 'EBADSTR',
  BADFLAGS: 'EBADFLAGS',
  NONAME: 'ENONAME',
  BADHINTS: 'EBADHINTS',
  NOTINITIALIZED: 'ENOTINITIALIZED',
  LOADIPHLPAPI: 'ELOADIPHLPAPI',
  ADDRGETNETWORKPARAMS: 'EADDRGETNETWORKPARAMS',
  CANCELLED: 'ECANCELLED'
};

bindDefaultResolver(module.exports, getDefaultResolver());

ObjectDefineProperties(module.exports, {
  promises: {
    configurable: true,
    enumerable: true,
    get() {
      if (promises === null) {
        promises = require('internal/dns/promises');
        promises.setServers = defaultResolverSetServers;
      }
      return promises;
    }
  }
});
