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

const GetAddrInfoReqWrap = cares.GetAddrInfoReqWrap;
const GetNameInfoReqWrap = cares.GetNameInfoReqWrap;
const QueryReqWrap = cares.QueryReqWrap;

const isIP = cares.isIP;
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

  var err = cares.getaddrinfo(req, hostname, family, hints);
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


function resolver(bindingName) {
  var binding = cares[bindingName];

  return function query(name, /* options, */ callback) {
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
    var err = binding(req, name);
    if (err) throw errnoException(err, bindingName);
    return req;
  };
}


var resolveMap = Object.create(null);
resolveMap.A = resolver('queryA');
resolveMap.AAAA = resolver('queryAaaa');
resolveMap.CNAME = resolver('queryCname');
resolveMap.MX = resolver('queryMx');
resolveMap.NS = resolver('queryNs');
resolveMap.TXT = resolver('queryTxt');
resolveMap.SRV = resolver('querySrv');
resolveMap.PTR = resolver('queryPtr');
resolveMap.NAPTR = resolver('queryNaptr');
resolveMap.SOA = resolver('querySoa');


function resolve(hostname, rrtype, callback) {
  var resolver;
  if (typeof rrtype === 'string') {
    resolver = resolveMap[rrtype];
  } else if (typeof rrtype === 'function') {
    resolver = resolveMap.A;
    callback = rrtype;
  } else {
    throw new TypeError('"rrtype" argument must be a string');
  }

  if (typeof resolver === 'function') {
    return resolver(hostname, callback);
  } else {
    throw new Error(`Unknown type "${rrtype}"`);
  }
}


function getServers() {
  return cares.getServers();
}


function setServers(servers) {
  // cache the original servers because in the event of an error setting the
  // servers cares won't have any servers available for resolution
  const orig = cares.getServers();
  const newSet = [];

  servers.forEach((serv) => {
    var ipVersion = isIP(serv);
    if (ipVersion !== 0)
      return newSet.push([ipVersion, serv]);

    const match = serv.match(/\[(.*)\](?::\d+)?/);
    // we have an IPv6 in brackets
    if (match) {
      ipVersion = isIP(match[1]);
      if (ipVersion !== 0)
        return newSet.push([ipVersion, match[1]]);
    }

    const s = serv.split(/:\d+$/)[0];
    ipVersion = isIP(s);

    if (ipVersion !== 0)
      return newSet.push([ipVersion, s]);

    throw new Error(`IP address is not properly formatted: ${serv}`);
  });

  const errorNumber = cares.setServers(newSet);

  if (errorNumber !== 0) {
    // reset the servers to the old servers, because ares probably unset them
    cares.setServers(orig.join(','));

    var err = cares.strerror(errorNumber);
    throw new Error(`c-ares failed to set servers: "${err}" [${servers}]`);
  }
}

module.exports = {
  lookup,
  lookupService,
  getServers,
  setServers,
  resolve,
  resolve4: resolveMap.A,
  resolve6: resolveMap.AAAA,
  resolveCname: resolveMap.CNAME,
  resolveMx: resolveMap.MX,
  resolveNs: resolveMap.NS,
  resolveTxt: resolveMap.TXT,
  resolveSrv: resolveMap.SRV,
  resolvePtr: resolveMap.PTR,
  resolveNaptr: resolveMap.NAPTR,
  resolveSoa: resolveMap.SOA,
  reverse: resolver('getHostByAddr'),

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
