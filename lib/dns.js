'use strict';

const util = require('util');

const cares = process.binding('cares_wrap');
const uv = process.binding('uv');
const internalNet = require('internal/net');
const errors = require('internal/errors');

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


// c-ares invokes a callback either synchronously or asynchronously,
// but the dns API should always invoke a callback asynchronously.
//
// This function makes sure that the callback is invoked asynchronously.
// It returns a function that invokes the callback within nextTick().
//
// To avoid invoking unnecessary nextTick(), `immediately` property of
// returned function should be set to true after c-ares returned.
//
// Usage:
//
// function someAPI(callback) {
//   callback = makeAsync(callback);
//   channel.someAPI(..., callback);
//   callback.immediately = true;
// }
function makeAsync(callback) {
  return function asyncCallback(...args) {
    if (asyncCallback.immediately) {
      // The API already returned, we can invoke the callback immediately.
      callback.apply(null, args);
    } else {
      args.unshift(callback);
      process.nextTick.apply(null, args);
    }
  };
}


function onlookup(err, addresses) {
  if (err) {
    return this.callback(errnoException(err, 'getaddrinfo', this.hostname));
  }
  if (this.family) {
    this.callback(null, addresses[0], this.family);
  } else {
    this.callback(null, addresses[0], addresses[0].indexOf(':') >= 0 ? 6 : 4);
  }
}


function onlookupall(err, addresses) {
  var results = [];
  if (err) {
    return this.callback(errnoException(err, 'getaddrinfo', this.hostname));
  }

  for (var i = 0; i < addresses.length; i++) {
    results.push({
      address: addresses[i],
      family: this.family || (addresses[i].indexOf(':') >= 0 ? 6 : 4)
    });
  }

  this.callback(null, results);
}


// Easy DNS A/AAAA look up
// lookup(hostname, [options,] callback)
function lookup(hostname, options, callback) {
  var hints = 0;
  var family = -1;
  var all = false;

  // Parse arguments
  if (hostname && typeof hostname !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
      'hostname', ['string', 'buffer']);
  } else if (typeof options === 'function') {
    callback = options;
    family = 0;
  } else if (typeof callback !== 'function') {
    // throw new TypeError('Invalid arguments: callback must be passed');
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  } else if (options !== null && typeof options === 'object') {
    hints = options.hints >>> 0;
    family = options.family >>> 0;
    all = options.all === true;

    if (hints !== 0 &&
      hints !== cares.AI_ADDRCONFIG &&
      hints !== cares.AI_V4MAPPED &&
      hints !== (cares.AI_ADDRCONFIG | cares.AI_V4MAPPED)) {
      throw new errors.Error('ERR_INVALID_FLAG', 'hints');
    }
  } else {
    family = options >>> 0;
  }

  if (family !== 0 && family !== 4 && family !== 6)
    throw new errors.Error('ERR_INVALID_ARG_VALUE',
      'family', ['4', '6']);

  callback = makeAsync(callback);

  if (!hostname) {
    if (all) {
      callback(null, []);
    } else {
      callback(null, null, family === 6 ? 6 : 4);
    }
    return {};
  }

  var matchedFamily = isIP(hostname);
  if (matchedFamily) {
    if (all) {
      callback(null, [{address: hostname, family: matchedFamily}]);
    } else {
      callback(null, hostname, matchedFamily);
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
    callback(errnoException(err, 'getaddrinfo', hostname));
    return {};
  }

  callback.immediately = true;
  return req;
}


function onlookupservice(err, host, service) {
  if (err)
    return this.callback(errnoException(err, 'getnameinfo', this.host));

  this.callback(null, host, service);
}


// lookupService(address, port, callback)
function lookupService(host, port, callback) {
  if (arguments.length !== 3)
    throw new errors.Error('ERR_INVALID_ARGS');

  if (isIP(host) === 0)
    throw new errors.Error('ERR_INVALID_IP', 'host', host);

  if (!isLegalPort(port))
    throw new errors.RangeError('ERR_INVALID_PORT', 'port', port);

  if (typeof callback !== 'function')
    throw new errors.TypeError('ERR_INVALID_CALLBACK');

  port = +port;
  callback = makeAsync(callback);

  var req = new GetNameInfoReqWrap();
  req.callback = callback;
  req.host = host;
  req.port = port;
  req.oncomplete = onlookupservice;

  var err = cares.getnameinfo(req, host, port);
  if (err) throw errnoException(err, 'getnameinfo', host);

  callback.immediately = true;
  return req;
}


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
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'name', 'string');
    } else if (typeof callback !== 'function') {
      throw new errors.TypeError('ERR_INVALID_CALLBACK');
    }

    callback = makeAsync(callback);
    var req = new QueryReqWrap();
    req.bindingName = bindingName;
    req.callback = callback;
    req.hostname = name;
    req.oncomplete = onresolve;
    req.ttl = !!(options && options.ttl);
    var err = binding(req, name);
    if (err) throw errnoException(err, bindingName);
    callback.immediately = true;
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


function resolve(hostname, type_, callback_) {
  var resolver, callback;
  if (typeof type_ === 'string') {
    resolver = resolveMap[type_];
    callback = callback_;
  } else if (typeof type_ === 'function') {
    resolver = resolveMap.A;
    callback = type_;
  } else {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
      'type', ['string', 'function']);
  }

  if (typeof resolver === 'function') {
    return resolver(hostname, callback);
  } else {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
      'resolver', 'function');
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

    throw new errors.Error('ERR_INVALID_IP', 'serv', serv);
  });

  const errorNumber = cares.setServers(newSet);

  if (errorNumber !== 0) {
    // reset the servers to the old servers, because ares probably unset them
    cares.setServers(orig.join(','));

    var err = cares.strerror(errorNumber);
    throw new errors.Error('ERR_ASSERTION',
      `c-ares failed to set servers: "${err}" [${servers}]`);
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
