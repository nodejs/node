// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// 'Software'), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var net = require('net');
var util = require('util');

var cares = process.binding('cares_wrap');
var uv = process.binding('uv');

var isIp = net.isIP;


function errnoException(err, syscall) {
  // FIXME(bnoordhuis) Remove this backwards compatibility shite and pass
  // the true error to the user. ENOTFOUND is not even a proper POSIX error!
  if (err === uv.UV_EAI_MEMORY ||
      err === uv.UV_EAI_NODATA ||
      err === uv.UV_EAI_NONAME) {
    var ex = new Error(syscall + ' ENOTFOUND');
    ex.code = 'ENOTFOUND';
    ex.errno = 'ENOTFOUND';
    ex.syscall = syscall;
    return ex;
  }
  return util._errnoException(err, syscall);
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
  if (!IS_FUNCTION(callback)) {
    return callback;
  }
  return function asyncCallback() {
    if (asyncCallback.immediately) {
      // The API already returned, we can invoke the callback immediately.
      callback.apply(null, arguments);
    } else {
      var args = arguments;
      process.nextTick(function() {
        callback.apply(null, args);
      });
    }
  };
}


// Easy DNS A/AAAA look up
// lookup(domain, [family,] callback)
exports.lookup = function(domain, family, callback) {
  // parse arguments
  if (arguments.length === 2) {
    callback = family;
    family = 0;
  } else if (!family) {
    family = 0;
  } else {
    family = +family;
    if (family !== 4 && family !== 6) {
      throw new Error('invalid argument: `family` must be 4 or 6');
    }
  }
  callback = makeAsync(callback);

  if (!domain) {
    callback(null, null, family === 6 ? 6 : 4);
    return {};
  }

  // Hack required for Windows because Win7 removed the
  // localhost entry from c:\WINDOWS\system32\drivers\etc\hosts
  // See http://daniel.haxx.se/blog/2011/02/21/localhost-hack-on-windows/
  // TODO Remove this once c-ares handles this problem.
  if (process.platform == 'win32' && domain == 'localhost') {
    callback(null, '127.0.0.1', 4);
    return {};
  }

  var matchedFamily = net.isIP(domain);
  if (matchedFamily) {
    callback(null, domain, matchedFamily);
    return {};
  }

  function onanswer(err, addresses) {
    if (err) {
      return callback(errnoException(err, 'getaddrinfo'));
    }
    if (family) {
      callback(null, addresses[0], family);
    } else {
      callback(null, addresses[0], addresses[0].indexOf(':') >= 0 ? 6 : 4);
    }
  }

  var req = {};
  var err = cares.getaddrinfo(req, domain, family);

  if (err) {
    throw errnoException(err, 'getaddrinfo');
  }

  req.oncomplete = onanswer;

  callback.immediately = true;
  return req;
};


function resolver(bindingName) {
  var binding = cares[bindingName];

  return function query(name, callback) {
    function onanswer(err, result) {
      if (err)
        callback(errnoException(err, bindingName));
      else
        callback(null, result);
    }

    callback = makeAsync(callback);
    var req = {};
    var err = binding(req, name, onanswer);
    if (err) {
      throw errnoException(err, bindingName);
    }

    callback.immediately = true;
    return req;
  }
}


var resolveMap = {};
exports.resolve4 = resolveMap.A = resolver('queryA');
exports.resolve6 = resolveMap.AAAA = resolver('queryAaaa');
exports.resolveCname = resolveMap.CNAME = resolver('queryCname');
exports.resolveMx = resolveMap.MX = resolver('queryMx');
exports.resolveNs = resolveMap.NS = resolver('queryNs');
exports.resolveTxt = resolveMap.TXT = resolver('queryTxt');
exports.resolveSrv = resolveMap.SRV = resolver('querySrv');
exports.resolveNaptr = resolveMap.NAPTR = resolver('queryNaptr');
exports.reverse = resolveMap.PTR = resolver('getHostByAddr');


exports.resolve = function(domain, type_, callback_) {
  var resolver, callback;
  if (IS_STRING(type_)) {
    resolver = resolveMap[type_];
    callback = callback_;
  } else {
    resolver = exports.resolve4;
    callback = type_;
  }

  if (IS_FUNCTION(resolver)) {
    return resolver(domain, callback);
  } else {
    throw new Error('Unknown type "' + type_ + '"');
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

    throw new Error('IP address is not properly formatted: ' + serv);
  });

  var r = cares.setServers(newSet);

  if (r) {
    // reset the servers to the old servers, because ares probably unset them
    cares.setServers(orig.join(','));

    var err = cares.strerror(r);
    throw new Error('c-ares failed to set servers: "' + err +
                    '" [' + servers + ']');
  }
};


// ERROR CODES
exports.NODATA = 'ENODATA';
exports.FORMERR = 'EFORMERR';
exports.SERVFAIL = 'ESERVFAIL';
exports.NOTFOUND = 'ENOTFOUND';
exports.NOTIMP = 'ENOTIMP';
exports.REFUSED = 'EREFUSED';
exports.BADQUERY = 'EBADQUERY';
exports.ADNAME = 'EADNAME';
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
