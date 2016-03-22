'use strict';

const net = require('net');
const util = require('util');

const cares = process.binding('cares_wrap');
const uv = process.binding('uv');

const GetAddrInfoReqWrap = cares.GetAddrInfoReqWrap;
const GetNameInfoReqWrap = cares.GetNameInfoReqWrap;
const QueryReqWrap = cares.QueryReqWrap;

const isIp = net.isIP;


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
  if (typeof callback !== 'function') {
    return callback;
  }
  return function asyncCallback() {
    if (asyncCallback.immediately) {
      // The API already returned, we can invoke the callback immediately.
      callback.apply(null, arguments);
    } else {
      var args = new Array(arguments.length + 1);
      args[0] = callback;
      for (var i = 1, a = 0; a < arguments.length; ++i, ++a)
        args[i] = arguments[a];
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
exports.lookup = function lookup(hostname, options, callback) {
  var hints = 0;
  var family = -1;
  var all = false;

  // Parse arguments
  if (hostname && typeof hostname !== 'string') {
    throw new TypeError('invalid arguments: ' +
                        'hostname must be a string or falsey');
  } else if (typeof options === 'function') {
    callback = options;
    family = 0;
  } else if (typeof callback !== 'function') {
    throw new TypeError('invalid arguments: callback must be passed');
  } else if (options !== null && typeof options === 'object') {
    hints = options.hints >>> 0;
    family = options.family >>> 0;
    all = options.all === true;

    if (hints !== 0 &&
        hints !== exports.ADDRCONFIG &&
        hints !== exports.V4MAPPED &&
        hints !== (exports.ADDRCONFIG | exports.V4MAPPED)) {
      throw new TypeError('invalid argument: hints must use valid flags');
    }
  } else {
    family = options >>> 0;
  }

  if (family !== 0 && family !== 4 && family !== 6)
    throw new TypeError('invalid argument: family must be 4 or 6');

  callback = makeAsync(callback);

  if (!hostname) {
    if (all) {
      callback(null, []);
    } else {
      callback(null, null, family === 6 ? 6 : 4);
    }
    return {};
  }

  var matchedFamily = net.isIP(hostname);
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
};


function onlookupservice(err, host, service) {
  if (err)
    return this.callback(errnoException(err, 'getnameinfo', this.host));

  this.callback(null, host, service);
}


// lookupService(address, port, callback)
exports.lookupService = function(host, port, callback) {
  if (arguments.length !== 3)
    throw new Error('invalid arguments');

  if (cares.isIP(host) === 0)
    throw new TypeError('host needs to be a valid IP address');

  if (typeof port !== 'number')
    throw new TypeError(`port argument must be a number, got "${port}"`);

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
};


function onresolve(err, result) {
  if (err)
    this.callback(errnoException(err, this.bindingName, this.hostname));
  else
    this.callback(null, result);
}


function resolver(bindingName) {
  var binding = cares[bindingName];

  return function query(name, callback) {
    if (typeof name !== 'string') {
      throw new Error('Name must be a string');
    } else if (typeof callback !== 'function') {
      throw new Error('Callback must be a function');
    }

    callback = makeAsync(callback);
    var req = new QueryReqWrap();
    req.bindingName = bindingName;
    req.callback = callback;
    req.hostname = name;
    req.oncomplete = onresolve;
    var err = binding(req, name);
    if (err) throw errnoException(err, bindingName);
    callback.immediately = true;
    return req;
  };
}


var resolveMap = Object.create(null);
exports.resolve4 = resolveMap.A = resolver('queryA');
exports.resolve6 = resolveMap.AAAA = resolver('queryAaaa');
exports.resolveCname = resolveMap.CNAME = resolver('queryCname');
exports.resolveMx = resolveMap.MX = resolver('queryMx');
exports.resolveNs = resolveMap.NS = resolver('queryNs');
exports.resolveTxt = resolveMap.TXT = resolver('queryTxt');
exports.resolveSrv = resolveMap.SRV = resolver('querySrv');
exports.resolveNaptr = resolveMap.NAPTR = resolver('queryNaptr');
exports.resolveSoa = resolveMap.SOA = resolver('querySoa');
exports.reverse = resolveMap.PTR = resolver('getHostByAddr');


exports.resolve = function(hostname, type_, callback_) {
  var resolver, callback;
  if (typeof type_ === 'string') {
    resolver = resolveMap[type_];
    callback = callback_;
  } else if (typeof type_ === 'function') {
    resolver = exports.resolve4;
    callback = type_;
  } else {
    throw new Error('Type must be a string');
  }

  if (typeof resolver === 'function') {
    return resolver(hostname, callback);
  } else {
    throw new Error(`Unknown type "${type_}"`);
  }
};


exports.getServers = function() {
  return cares.getServers();
};


exports.setServers = function(servers) {
  // cache the original servers because in the event of an error setting the
  // servers cares won't have any servers available for resolution
  var orig = cares.getServers();

  var newSet = [];

  servers.forEach(function(serv) {
    var ver = isIp(serv);

    if (ver)
      return newSet.push([ver, serv]);

    var match = serv.match(/\[(.*)\](:\d+)?/);

    // we have an IPv6 in brackets
    if (match) {
      ver = isIp(match[1]);
      if (ver)
        return newSet.push([ver, match[1]]);
    }

    var s = serv.split(/:\d+$/)[0];
    ver = isIp(s);

    if (ver)
      return newSet.push([ver, s]);

    throw new Error(`IP address is not properly formatted: ${serv}`);
  });

  var r = cares.setServers(newSet);

  if (r) {
    // reset the servers to the old servers, because ares probably unset them
    cares.setServers(orig.join(','));

    var err = cares.strerror(r);
    throw new Error(`c-ares failed to set servers: "${err}" [${servers}]`);
  }
};

// uv_getaddrinfo flags
exports.ADDRCONFIG = cares.AI_ADDRCONFIG;
exports.V4MAPPED = cares.AI_V4MAPPED;

// ERROR CODES
exports.NODATA = 'ENODATA';
exports.FORMERR = 'EFORMERR';
exports.SERVFAIL = 'ESERVFAIL';
exports.NOTFOUND = 'ENOTFOUND';
exports.NOTIMP = 'ENOTIMP';
exports.REFUSED = 'EREFUSED';
exports.BADQUERY = 'EBADQUERY';
exports.BADNAME = 'EBADNAME';
exports.BADFAMILY = 'EBADFAMILY';
exports.BADRESP = 'EBADRESP';
exports.CONNREFUSED = 'ECONNREFUSED';
exports.TIMEOUT = 'ETIMEOUT';
exports.EOF = 'EOF';
exports.FILE = 'EFILE';
exports.NOMEM = 'ENOMEM';
exports.DESTRUCTION = 'EDESTRUCTION';
exports.BADSTR = 'EBADSTR';
exports.BADFLAGS = 'EBADFLAGS';
exports.NONAME = 'ENONAME';
exports.BADHINTS = 'EBADHINTS';
exports.NOTINITIALIZED = 'ENOTINITIALIZED';
exports.LOADIPHLPAPI = 'ELOADIPHLPAPI';
exports.ADDRGETNETWORKPARAMS = 'EADDRGETNETWORKPARAMS';
exports.CANCELLED = 'ECANCELLED';
