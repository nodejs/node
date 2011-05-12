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

var dns = process.binding('cares');
var net = process.binding('net');
var IOWatcher = process.binding('io_watcher').IOWatcher;


var watchers = {};
var activeWatchers = {};
var Timer = process.binding('timer').Timer;

var timer = new Timer();

timer.callback = function() {
  var sockets = Object.keys(activeWatchers);
  for (var i = 0, l = sockets.length; i < l; i++) {
    var socket = sockets[i];
    var s = parseInt(socket, 10);
    channel.processFD(watchers[socket].read ? s : dns.SOCKET_BAD,
                      watchers[socket].write ? s : dns.SOCKET_BAD);
  }
  updateTimer();
};


function updateTimer() {
  timer.stop();

  // Were just checking to see if activeWatchers is empty or not
  if (0 === Object.keys(activeWatchers).length) return;
  var max = 20000;
  var timeout = channel.timeout(max);
  timer.start(timeout, 0);
}


var channel = new dns.Channel({SOCK_STATE_CB: function(socket, read, write) {
  var watcher, fd;

  if (process.platform == 'win32') {
    fd = process.binding('os').openOSHandle(socket);
  } else {
    fd = socket;
  }

  if (socket in watchers) {
    watcher = watchers[socket].watcher;
  } else {
    watcher = new IOWatcher();
    watchers[socket] = { read: read,
                         write: write,
                         watcher: watcher };

    watcher.callback = function(read, write)  {
      channel.processFD(read ? socket : dns.SOCKET_BAD,
                        write ? socket : dns.SOCKET_BAD);
      updateTimer();
    };
  }

  watcher.stop();

  if (!(read || write)) {
    delete activeWatchers[socket];
    return;
  } else {
    watcher.set(fd, read == 1, write == 1);
    watcher.start();
    activeWatchers[socket] = watcher;
  }

  updateTimer();
}});

exports.resolve = function(domain, type_, callback_) {
  var type, callback;
  if (typeof(type_) == 'string') {
    type = type_;
    callback = callback_;
  } else {
    type = 'A';
    callback = arguments[1];
  }

  var resolveFunc = resolveMap[type];

  if (typeof(resolveFunc) == 'function') {
    resolveFunc(domain, callback);
  } else {
    throw new Error('Unknown type "' + type + '"');
  }
};


function familyToSym(family) {
  if (family !== dns.AF_INET && family !== dns.AF_INET6) {
    family = (family === 6) ? dns.AF_INET6 : dns.AF_INET;
  }
  return family;
}


exports.getHostByName = function(domain, family/*=4*/, callback) {
  if (typeof family === 'function') { callback = family; family = null; }
  channel.getHostByName(domain, familyToSym(family), callback);
};


exports.getHostByAddr = function(address, family/*=4*/, callback) {
  if (typeof family === 'function') { callback = family; family = null; }
  channel.getHostByAddr(address, familyToSym(family), callback);
};


// Easy DNS A/AAAA look up
// lookup(domain, [family,] callback)
exports.lookup = function(domain, family, callback) {
  // parse arguments
  if (arguments.length === 2) {
    callback = family;
    family = undefined;
  } else if (family && family !== 4 && family !== 6) {
    family = parseInt(family, 10);
    if (family === dns.AF_INET) {
      family = 4;
    } else if (family === dns.AF_INET6) {
      family = 6;
    } else if (family !== 4 && family !== 6) {
      throw new Error('invalid argument: "family" must be 4 or 6');
    }
  }

  if (!domain) {
    callback(null, null, family === 6 ? 6 : 4);
    return;
  }

  var matchedFamily = net.isIP(domain);
  if (matchedFamily) {
    callback(null, domain, matchedFamily);
    return;
  }

  if (/\w\.local\.?$/.test(domain)) {
    // ANNOYING: In the case of mDNS domains use NSS in the thread pool.
    // I wish c-ares had better support.
    process.binding('net').getaddrinfo(domain, 4, function(err, domains4) {
      callback(err, domains4[0], 4);
    });
    return;
  }

  if (family) {
    // resolve names for explicit address family
    var af = familyToSym(family);
    channel.getHostByName(domain, af, function(err, domains) {
      if (!err && domains && domains.length) {
        if (family !== net.isIP(domains[0])) {
          callback(new Error('not found'), []);
        } else {
          callback(null, domains[0], family);
        }
      } else {
        callback(err, []);
      }
    });
    return;
  }

  // first resolve names for v4 and if that fails, try v6
  channel.getHostByName(domain, dns.AF_INET, function(err, domains4) {
    if (domains4 && domains4.length) {
      callback(null, domains4[0], 4);
    } else {
      channel.getHostByName(domain, dns.AF_INET6, function(err, domains6) {
        if (domains6 && domains6.length) {
          callback(null, domains6[0], 6);
        } else {
          callback(err, []);
        }
      });
    }
  });
};


exports.resolve4 = function(domain, callback) {
  channel.query(domain, dns.A, callback);
};


exports.resolve6 = function(domain, callback) {
  channel.query(domain, dns.AAAA, callback);
};


exports.resolveMx = function(domain, callback) {
  channel.query(domain, dns.MX, callback);
};


exports.resolveTxt = function(domain, callback) {
  channel.query(domain, dns.TXT, callback);
};


exports.resolveSrv = function(domain, callback) {
  channel.query(domain, dns.SRV, callback);
};


exports.reverse = function(domain, callback) {
  channel.query(domain, dns.PTR, callback);
};


exports.resolveNs = function(domain, callback) {
  channel.query(domain, dns.NS, callback);
};


exports.resolveCname = function(domain, callback) {
  channel.query(domain, dns.CNAME, callback);
};

var resolveMap = { A: exports.resolve4,
                   AAAA: exports.resolve6,
                   MX: exports.resolveMx,
                   TXT: exports.resolveTxt,
                   SRV: exports.resolveSrv,
                   PTR: exports.reverse,
                   NS: exports.resolveNs,
                   CNAME: exports.resolveCname };

// ERROR CODES
exports.NODATA = dns.NODATA;
exports.FORMERR = dns.FORMERR;
exports.BADRESP = dns.BADRESP;
exports.NOTFOUND = dns.NOTFOUND;
exports.BADNAME = dns.BADNAME;
exports.TIMEOUT = dns.TIMEOUT;
exports.CONNREFUSED = dns.CONNREFUSED;
exports.NOMEM = dns.NOMEM;
exports.DESTRUCTION = dns.DESTRUCTION;
exports.NOTIMP = dns.NOTIMP;
exports.EREFUSED = dns.EREFUSED;
exports.SERVFAIL = dns.SERVFAIL;
