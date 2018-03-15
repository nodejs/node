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

const cares = process.binding('cares_wrap');
const { isIP, isIPv4, isLegalPort } = require('internal/net');
const { customPromisifyArgs } = require('internal/util');
const errors = require('internal/errors');
const {
  ERR_DNS_SET_SERVERS_FAILED,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_INVALID_IP_ADDRESS,
  ERR_INVALID_OPT_VALUE,
  ERR_MISSING_ARGS,
  ERR_SOCKET_BAD_PORT
} = errors.codes;

const {
  GetAddrInfoReqWrap,
  GetNameInfoReqWrap,
  QueryReqWrap,
  ChannelWrap,
} = cares;

const IANA_DNS_PORT = 53;
const dnsException = errors.dnsException;

function onlookup(err, addresses) {
  if (err) {
    return this.callback(dnsException(err, 'getaddrinfo', this.hostname));
  }
  if (this.family) {
    this.callback(null, addresses[0], this.family);
  } else {
    this.callback(null, addresses[0], isIPv4(addresses[0]) ? 4 : 6);
  }
}


function onlookupall(err, addresses) {
  if (err) {
    return this.callback(dnsException(err, 'getaddrinfo', this.hostname));
  }

  const family = this.family;
  for (var i = 0; i < addresses.length; i++) {
    const addr = addresses[i];
    addresses[i] = {
      address: addr,
      family: family || (isIPv4(addr) ? 4 : 6)
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
    throw new ERR_INVALID_ARG_TYPE('hostname', ['string', 'falsy'], hostname);
  } else if (typeof options === 'function') {
    callback = options;
    family = 0;
  } else if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK();
  } else if (options !== null && typeof options === 'object') {
    hints = options.hints >>> 0;
    family = options.family >>> 0;
    all = options.all === true;
    verbatim = options.verbatim === true;

    if (hints !== 0 &&
        hints !== cares.AI_ADDRCONFIG &&
        hints !== cares.AI_V4MAPPED &&
        hints !== (cares.AI_ADDRCONFIG | cares.AI_V4MAPPED)) {
      throw new ERR_INVALID_OPT_VALUE('hints', hints);
    }
  } else {
    family = options >>> 0;
  }

  if (family !== 0 && family !== 4 && family !== 6)
    throw new ERR_INVALID_OPT_VALUE('family', family);

  if (!hostname) {
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

  const err = cares.getaddrinfo(req, hostname, family, hints, verbatim);
  if (err) {
    process.nextTick(callback, dnsException(err, 'getaddrinfo', hostname));
    return {};
  }
  return req;
}

Object.defineProperty(lookup, customPromisifyArgs,
                      { value: ['address', 'family'], enumerable: false });


function onlookupservice(err, host, service) {
  if (err)
    return this.callback(dnsException(err, 'getnameinfo', this.host));

  this.callback(null, host, service);
}


// lookupService(address, port, callback)
function lookupService(host, port, callback) {
  if (arguments.length !== 3)
    throw new ERR_MISSING_ARGS('host', 'port', 'callback');

  if (isIP(host) === 0)
    throw new ERR_INVALID_OPT_VALUE('host', host);

  if (!isLegalPort(port))
    throw new ERR_SOCKET_BAD_PORT(port);

  if (typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK();

  port = +port;

  const req = new GetNameInfoReqWrap();
  req.callback = callback;
  req.host = host;
  req.port = port;
  req.oncomplete = onlookupservice;

  const err = cares.getnameinfo(req, host, port);
  if (err) throw dnsException(err, 'getnameinfo', host);
  return req;
}

Object.defineProperty(lookupService, customPromisifyArgs,
                      { value: ['hostname', 'service'], enumerable: false });


function onresolve(err, result, ttls) {
  if (ttls && this.ttl)
    result = result.map((address, index) => ({ address, ttl: ttls[index] }));

  if (err)
    this.callback(dnsException(err, this.bindingName, this.hostname));
  else
    this.callback(null, result);
}

// Resolver instances correspond 1:1 to c-ares channels.
class Resolver {
  constructor() {
    this._handle = new ChannelWrap();
  }

  cancel() {
    this._handle.cancel();
  }
}

function resolver(bindingName) {
  function query(name, /* options, */ callback) {
    let options;
    if (arguments.length > 2) {
      options = callback;
      callback = arguments[2];
    }

    if (typeof name !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('name', 'string', name);
    } else if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK();
    }

    const req = new QueryReqWrap();
    req.bindingName = bindingName;
    req.callback = callback;
    req.hostname = name;
    req.oncomplete = onresolve;
    req.ttl = !!(options && options.ttl);
    const err = this._handle[bindingName](req, name);
    if (err) throw dnsException(err, bindingName);
    return req;
  }
  Object.defineProperty(query, 'name', { value: bindingName });
  return query;
}

const resolveMap = Object.create(null);
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


Resolver.prototype.getServers = getServers;
function getServers() {
  const ret = this._handle.getServers();
  return ret.map((val) => {
    if (!val[1] || val[1] === IANA_DNS_PORT) return val[0];

    const host = isIP(val[0]) === 6 ? `[${val[0]}]` : val[0];
    return `${host}:${val[1]}`;
  });
}


Resolver.prototype.setServers = setServers;
function setServers(servers) {
  // cache the original servers because in the event of an error setting the
  // servers cares won't have any servers available for resolution
  const orig = this._handle.getServers();
  const newSet = [];
  const IPv6RE = /\[(.*)\]/;
  const addrSplitRE = /(^.+?)(?::(\d+))?$/;

  servers.forEach((serv) => {
    const ipVersion = isIP(serv);
    if (ipVersion !== 0)
      return newSet.push([ipVersion, serv, IANA_DNS_PORT]);

    const match = serv.match(IPv6RE);
    // we have an IPv6 in brackets
    if (match) {
      ipVersion = isIP(match[1]);
      if (ipVersion !== 0) {
        const port =
          parseInt(serv.replace(addrSplitRE, '$2')) ||
          IANA_DNS_PORT;
        return newSet.push([ipVersion, match[1], port]);
      }
    }

    const [, s, p] = serv.match(addrSplitRE);
    ipVersion = isIP(s);

    if (ipVersion !== 0) {
      return newSet.push([ipVersion, s, parseInt(p)]);
    }

    throw new ERR_INVALID_IP_ADDRESS.Error(serv);
  });

  const errorNumber = this._handle.setServers(newSet);

  if (errorNumber !== 0) {
    // reset the servers to the old servers, because ares probably unset them
    this._handle.setServers(orig.join(','));

    const err = cares.strerror(errorNumber);
    throw new ERR_DNS_SET_SERVERS_FAILED(err, servers);
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

function setExportsFunctions() {
  resolverKeys.forEach((key) => {
    module.exports[key] = defaultResolver[key].bind(defaultResolver);
  });
}

function defaultResolverSetServers(servers) {
  const resolver = new Resolver();
  resolver.setServers(servers);
  defaultResolver = resolver;
  setExportsFunctions();
}

module.exports = {
  lookup,
  lookupService,

  Resolver,
  setServers: defaultResolverSetServers,

  // uv_getaddrinfo flags
  ADDRCONFIG: cares.AI_ADDRCONFIG,
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

setExportsFunctions();
