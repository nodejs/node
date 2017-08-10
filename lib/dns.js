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

const util = require('util');

const cares = process.binding('cares_wrap');
const uv = process.binding('uv');
const internalNet = require('internal/net');
const { customPromisifyArgs } = require('internal/util');

const {
  GetAddrInfoReqWrap,
  GetNameInfoReqWrap,
  QueryReqWrap,
  ChannelWrap,
  isIP
} = cares;

const isLegalPort = internalNet.isLegalPort;


function errnoException(err, syscall, hostname) {
  // FIXME(bnoordhuis) Remove this backwards compatibility nonsense and pass
  // the true error to the user. ENOTFOUND is not even a proper POSIX error!
  if (err === uv.UV_EAI_MEMORY ||
      err === uv.UV_EAI_NODATA ||
      err === uv.UV_EAI_NONAME) {
    err = 'ENOTFOUND';
  }
  var ex = null;
  if (typeof err === 'string') {  // c-ares error code.
    const errHost = hostname ? ' ' + hostname : '';
    ex = new Error(`${syscall} ${err}${errHost}`);
    ex.code = err;
    ex.errno = err;
    ex.syscall = syscall;
  } else {
    ex = util._errnoException(err, syscall);
  }
  if (hostname) {
    ex.hostname = hostname;
  }
  return ex;
}

const IANA_DNS_PORT = 53;
const digits = [
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0-15
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 16-31
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 32-47
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, // 48-63
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 64-79
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 80-95
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 96-111
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 112-127
];
function isIPv4(str) {
  if (!digits[str.charCodeAt(0)]) return false;
  if (str.length === 1) return false;
  if (str.charCodeAt(1) === 46/*'.'*/)
    return true;
  else if (!digits[str.charCodeAt(1)])
    return false;
  if (str.length === 2) return false;
  if (str.charCodeAt(2) === 46/*'.'*/)
    return true;
  else if (!digits[str.charCodeAt(2)])
    return false;
  return (str.length > 3 && str.charCodeAt(3) === 46/*'.'*/);
}


function onlookup(err, addresses) {
  if (err) {
    return this.callback(errnoException(err, 'getaddrinfo', this.hostname));
  }
  if (this.family) {
    this.callback(null, addresses[0], this.family);
  } else {
    this.callback(null, addresses[0], isIPv4(addresses[0]) ? 4 : 6);
  }
}


function onlookupall(err, addresses) {
  if (err) {
    return this.callback(errnoException(err, 'getaddrinfo', this.hostname));
  }

  var family = this.family;
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
  var hints = 0;
  var family = -1;
  var all = false;
  var verbatim = false;

  // Parse arguments
  if (hostname && typeof hostname !== 'string') {
    throw new TypeError('Invalid arguments: ' +
                        'hostname must be a string or falsey');
  } else if (typeof options === 'function') {
    callback = options;
    family = 0;
  } else if (typeof callback !== 'function') {
    throw new TypeError('Invalid arguments: callback must be passed');
  } else if (options !== null && typeof options === 'object') {
    hints = options.hints >>> 0;
    family = options.family >>> 0;
    all = options.all === true;
    verbatim = options.verbatim === true;

    if (hints !== 0 &&
        hints !== cares.AI_ADDRCONFIG &&
        hints !== cares.AI_V4MAPPED &&
        hints !== (cares.AI_ADDRCONFIG | cares.AI_V4MAPPED)) {
      throw new TypeError('Invalid argument: hints must use valid flags');
    }
  } else {
    family = options >>> 0;
  }

  if (family !== 0 && family !== 4 && family !== 6)
    throw new TypeError('Invalid argument: family must be 4 or 6');

  if (!hostname) {
    if (all) {
      process.nextTick(callback, null, []);
    } else {
      process.nextTick(callback, null, null, family === 6 ? 6 : 4);
    }
    return {};
  }

  var matchedFamily = isIP(hostname);
  if (matchedFamily) {
    if (all) {
      process.nextTick(
        callback, null, [{address: hostname, family: matchedFamily}]);
    } else {
      process.nextTick(callback, null, hostname, matchedFamily);
    }
    return {};
  }

  var req = new GetAddrInfoReqWrap();
  req.callback = callback;
  req.family = family;
  req.hostname = hostname;
  req.oncomplete = all ? onlookupall : onlookup;

  var err = cares.getaddrinfo(req, hostname, family, hints, verbatim);
  if (err) {
    process.nextTick(callback, errnoException(err, 'getaddrinfo', hostname));
    return {};
  }
  return req;
}

Object.defineProperty(lookup, customPromisifyArgs,
                      { value: ['address', 'family'], enumerable: false });


function onlookupservice(err, host, service) {
  if (err)
    return this.callback(errnoException(err, 'getnameinfo', this.host));

  this.callback(null, host, service);
}


// lookupService(address, port, callback)
function lookupService(host, port, callback) {
  if (arguments.length !== 3)
    throw new Error('Invalid arguments');

  if (isIP(host) === 0)
    throw new TypeError('"host" argument needs to be a valid IP address');

  if (!isLegalPort(port))
    throw new TypeError(`"port" should be >= 0 and < 65536, got "${port}"`);

  if (typeof callback !== 'function')
    throw new TypeError('"callback" argument must be a function');

  port = +port;

  var req = new GetNameInfoReqWrap();
  req.callback = callback;
  req.host = host;
  req.port = port;
  req.oncomplete = onlookupservice;

  var err = cares.getnameinfo(req, host, port);
  if (err) throw errnoException(err, 'getnameinfo', host);
  return req;
}

Object.defineProperty(lookupService, customPromisifyArgs,
                      { value: ['hostname', 'service'], enumerable: false });


function onresolve(err, result, ttls) {
  if (ttls && this.ttl)
    result = result.map((address, index) => ({ address, ttl: ttls[index] }));

  if (err)
    this.callback(errnoException(err, this.bindingName, this.hostname));
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
    var options;
    if (arguments.length > 2) {
      options = callback;
      callback = arguments[2];
    }

    if (typeof name !== 'string') {
      throw new Error('"name" argument must be a string');
    } else if (typeof callback !== 'function') {
      throw new Error('"callback" argument must be a function');
    }

    var req = new QueryReqWrap();
    req.bindingName = bindingName;
    req.callback = callback;
    req.hostname = name;
    req.oncomplete = onresolve;
    req.ttl = !!(options && options.ttl);
    var err = this._handle[bindingName](req, name);
    if (err) throw errnoException(err, bindingName);
    return req;
  }
  Object.defineProperty(query, 'name', { value: bindingName });
  return query;
}

var resolveMap = Object.create(null);
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

function resolve(hostname, type_, callback_) {
  var resolver, callback;
  if (typeof type_ === 'string') {
    resolver = resolveMap[type_];
    callback = callback_;
  } else if (typeof type_ === 'function') {
    resolver = resolveMap.A;
    callback = type_;
  } else {
    throw new Error('"type" argument must be a string');
  }

  if (typeof resolver === 'function') {
    return resolver.call(this, hostname, callback);
  } else {
    throw new Error(`Unknown type "${type_}"`);
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
    var ipVersion = isIP(serv);
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

    throw new Error(`IP address is not properly formatted: ${serv}`);
  });

  const errorNumber = this._handle.setServers(newSet);

  if (errorNumber !== 0) {
    // reset the servers to the old servers, because ares probably unset them
    this._handle.setServers(orig.join(','));

    var err = cares.strerror(errorNumber);
    throw new Error(`c-ares failed to set servers: "${err}" [${servers}]`);
  }
}

const defaultResolver = new Resolver();

module.exports = {
  lookup,
  lookupService,

  Resolver,
  getServers: defaultResolver.getServers.bind(defaultResolver),
  setServers: defaultResolver.setServers.bind(defaultResolver),
  resolve: defaultResolver.resolve.bind(defaultResolver),
  resolveAny: defaultResolver.resolveAny.bind(defaultResolver),
  resolve4: defaultResolver.resolve4.bind(defaultResolver),
  resolve6: defaultResolver.resolve6.bind(defaultResolver),
  resolveCname: defaultResolver.resolveCname.bind(defaultResolver),
  resolveMx: defaultResolver.resolveMx.bind(defaultResolver),
  resolveNs: defaultResolver.resolveNs.bind(defaultResolver),
  resolveTxt: defaultResolver.resolveTxt.bind(defaultResolver),
  resolveSrv: defaultResolver.resolveSrv.bind(defaultResolver),
  resolvePtr: defaultResolver.resolvePtr.bind(defaultResolver),
  resolveNaptr: defaultResolver.resolveNaptr.bind(defaultResolver),
  resolveSoa: defaultResolver.resolveSoa.bind(defaultResolver),
  reverse: defaultResolver.reverse.bind(defaultResolver),

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
