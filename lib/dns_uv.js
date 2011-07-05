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

var cares = process.binding('cares_wrap'),
    net = require('net'),
    isIp = net.isIP;


function errnoException(errorno, syscall) {
  // TODO make this more compatible with ErrnoException from src/node.cc
  // Once all of Node is using this function the ErrnoException from
  // src/node.cc should be removed.
  var e = new Error(syscall + ' ' + errorno);
  e.errno = e.code = errorno;
  e.syscall = syscall;
  return e;
}


function familyToSym(family) {
  switch (family) {
    case 4: return cares.AF_INET;
    case 6: return cares.AF_INET6;
    default: return cares.AF_UNSPEC;
  }
}


function symToFamily(family) {
  switch (family) {
    case cares.AF_INET: return 4;
    case cares.AF_INET6: return 6;
    default: return undefined;
  }
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

  if (!domain) {
    callback(null, null, family === 6 ? 6 : 4);
    return {};
  }

  var matchedFamily = net.isIP(domain);
  if (matchedFamily) {
    callback(null, domain, matchedFamily);
    return {};
  }

  /* TODO
  if (/\w\.local\.?$/.test(domain)) {
    // ANNOYING: In the case of mDNS domains use NSS in the thread pool.
    // I wish c-ares had better support.
    process.binding('net').getaddrinfo(domain, 4, function(err, domains4) {
      callback(err, domains4[0], 4);
    });
    return {};
  } */

  function onanswer(status, addresses, familySym) {
    if (!status) {
      callback(null, addresses[0], symToFamily(familySym));
    } else {
      callback(errnoException(errno, 'getHostByName'));
    }
  }

  var wrap = cares.getHostByName(domain, familyToSym(family), onanswer);
  if (!wrap) {
    throw errnoException(errno, 'getHostByName');
  }

  return wrap;
};


function resolver(bindingName) {
  var binding = cares[bindingName];

  return function query(name, callback) {
    function onanswer(status, result) {
      if (!status) {
        callback(null, result);
      } else {
        callback(errnoException(errno, bindingName));
      }
    }

    var wrap = binding(name, onanswer);
    if (!wrap) {
      throw errnoException(errno, bindingName);
    }

    return wrap;
  }
}


var resolveMap = {};
exports.resolve4     = resolveMap.A     = resolver('queryA');
exports.resolve6     = resolveMap.AAAA  = resolver('queryAaaa');
exports.resolveCname = resolveMap.CNAME = resolver('queryCname');
exports.resolveMx    = resolveMap.MX    = resolver('queryMx');
exports.resolveNs    = resolveMap.NS    = resolver('queryNs');
exports.resolveTxt   = resolveMap.TXT   = resolver('queryTxt');
exports.resolveSrv   = resolveMap.SRV   = resolver('querySrv');
exports.reverse      = resolveMap.PTR   = resolver('getHostByAddr');


exports.resolve = function(domain, type_, callback_) {
  var resolver, callback;
  if (typeof type_ == 'string') {
    resolver = resolveMap[type_];
    callback = callback_;
  } else {
    resolver = exports.resolve4;
    callback = type_;
  }

  if (typeof resolver === 'function') {
    return resolver(domain, callback);
  } else {
    throw new Error('Unknown type "' + type + '"');
  }
};


// ERROR CODES
exports.BADNAME = 'EBADNAME';
exports.BADRESP = 'EBADRESP';
exports.CONNREFUSED = 'ECONNREFUSED';
exports.DESTRUCTION = 'EDESTRUCTION';
exports.REFUSED = 'EREFUSED';
exports.FORMERR = 'EFORMERR';
exports.NODATA = 'ENODATA';
exports.NOMEM = 'ENOMEM';
exports.NOTFOUND = 'ENOTFOUND';
exports.NOTIMP = 'ENOTIMP';
exports.SERVFAIL = 'ESERVFAIL';
exports.TIMEOUT = 'ETIMEOUT';
