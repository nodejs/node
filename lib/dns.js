var dns = process.binding('cares');


var watchers = {};
var activeWatchers = {};


var timer = new process.Timer();

timer.callback = function () {
  var sockets = Object.keys(activeWatchers);
  for (var i = 0, l = sockets.length; i < l; i++) {
    var socket = sockets[i];
    var s = parseInt(socket);
    channel.processFD( watchers[socket].read  ? s : dns.SOCKET_BAD
                     , watchers[socket].write ? s : dns.SOCKET_BAD
                     );
  }
  updateTimer();
}


function updateTimer() {
  timer.stop();

  // Were just checking to see if activeWatchers is empty or not
  for (var socket in activeWatchers) {
    if (activeWatchers.hasOwnProperty(socket)) {
      var max = 20000;
      var timeout = channel.timeout(max);

      timer.start(timeout, 0);
      // Short circuit the loop on first find.
      return;
    }
  }
}


var channel = new dns.Channel({SOCK_STATE_CB: function (socket, read, write) {
  var watcher;

  if (socket in watchers) {
    watcher = watchers[socket].watcher;
  } else {
    watcher = new process.IOWatcher();
    watchers[socket] = { read: read
                       , write: write
                       , watcher: watcher
                       };

    watcher.callback = function(read, write)  {
      channel.processFD( read  ? socket : dns.SOCKET_BAD
                       , write ? socket : dns.SOCKET_BAD
                       );
      updateTimer();
    }
  }

  watcher.set(socket, read == 1, write == 1);

  if (!(read || write)) {
    watcher.stop();
    delete activeWatchers[socket];
    return;
  } else {
    watcher.start();
    activeWatchers[socket] = watcher;
  }

  updateTimer();
}});



exports.resolve = function (domain, type_, callback_) {
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
}


exports.getHostByName = function (domain, callback) {
  channel.getHostByName(domain, dns.AF_INET, callback);
};

var net;

// Easy DNS A/AAAA look up
exports.lookup = function (domain, callback) {
  var addressType = dns.isIP(domain);
  if (addressType) {
    process.nextTick(function () {
      callback(null, domain, addressType);
    });
  } else {
    if (/\w\.local\.?$/.test(domain) ) {
      // ANNOYING: In the case of mDNS domains use NSS in the thread pool.
      // I wish c-ares had better support.
      process.binding('net').getaddrinfo(domain, 4, function (err, domains4) {
        callback(err, domains4[0], 4);
      });
    } else {
      channel.getHostByName(domain, dns.AF_INET, function (err, domains4) {
        if (domains4 && domains4.length) {
          callback(null, domains4[0], 4);
        } else {
          channel.getHostByName(domain, dns.AF_INET6, function (err, domains6) {
            if (domains6 && domains6.length) {
              callback(null, domains6[0], 6);
            } else {
              callback(err, []);
            }
          });
        }
      });
    }
  }
};


exports.resolve4    = function(domain, callback) { channel.query(domain, dns.A, callback) };
exports.resolve6    = function(domain, callback) { channel.query(domain, dns.AAAA, callback) };
exports.resolveMx   = function(domain, callback) { channel.query(domain, dns.MX, callback) };
exports.resolveTxt  = function(domain, callback) { channel.query(domain, dns.TXT, callback) };
exports.resolveSrv  = function(domain, callback) { channel.query(domain, dns.SRV, callback) };
exports.reverse     = function(domain, callback) { channel.query(domain, dns.PTR, callback) };
exports.resolveNs   = function(domain, callback) { channel.query(domain, dns.NS, callback) };


var resolveMap = {
  'A'   : exports.resolve4,
  'AAAA': exports.resolve6,
  'MX'  : exports.resolveMx,
  'TXT' : exports.resolveTxt,
  'SRV' : exports.resolveSrv,
  'PTR' : exports.resolvePtr,
  'NS'  : exports.resolveNs,
};

// ERROR CODES
exports.NODATA      = dns.NODATA;
exports.FORMERR     = dns.FORMERR;
exports.BADRESP     = dns.BADRESP;
exports.NOTFOUND    = dns.NOTFOUND;
exports.BADNAME     = dns.BADNAME;
exports.TIMEOUT     = dns.TIMEOUT;
exports.CONNREFUSED = dns.CONNREFUSED;
exports.NOMEM       = dns.NOMEM;
exports.DESTRUCTION = dns.DESTRUCTION;

exports.NOTIMP      = dns.NOTIMP;
exports.EREFUSED    = dns.EREFUSED;
exports.SERVFAIL    = dns.SERVFAIL;
