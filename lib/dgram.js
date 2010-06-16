var sys = require("sys");
var fs = require("fs");
var events = require("events");
var dns = require('dns');

var Buffer = require('buffer').Buffer;
var IOWatcher    = process.IOWatcher;
var binding      = process.binding('net');
var socket       = binding.socket;
var bind         = binding.bind;
var recvfrom     = binding.recvfrom;
var sendto       = binding.sendto;
var close        = binding.close;
var ENOENT       = binding.ENOENT;
var setBroadcast = binding.setBroadcast;

function isPort (x) { return parseInt(x) >= 0; }
var pool = null;

function getPool() {
  /* TODO: this effectively limits you to 8kb maximum packet sizes */
  var minPoolAvail = 1024 * 8;

  var poolSize = 1024 * 64;

  if (pool === null || (pool.used + minPoolAvail  > pool.length)) {
    pool = new Buffer(poolSize);
    pool.used = 0;
  }

  return pool;
}

function Socket (broadcast, listener) {
  events.EventEmitter.call(this);
  var self = this;

  if (typeof(broadcast) != 'boolean') {
    listener = broadcast;
    broadcast = false;
  }
  self.broadcast = broadcast;

  if (listener) {
    self.addListener('message', listener);
  }

  self.watcher = new IOWatcher();
  self.watcher.host = self;
  self.watcher.callback = function () {
    while (self.fd) {
      var p = getPool();
      var rinfo = recvfrom(self.fd, p, p.used, p.length - p.used, 0);

      if (!rinfo) return;

      self.emit('message', p.slice(p.used, p.used + rinfo.size), rinfo);

      p.used += rinfo.size;
    }
  };
}

sys.inherits(Socket, events.EventEmitter);
exports.Socket = Socket;

exports.createSocket = function (broadcast, listener) {
  return new Socket(broadcast, listener);
};

Socket.prototype.bind = function () {
  var self = this;
  if (self.fd) throw new Error('Server already opened');

  if (!isPort(arguments[0])) {
    /* TODO: unix path dgram */
    self.fd = socket('unix_dgram');
    self.type = 'unix_dgram';
    var path = arguments[0];
    self.path = path;
    // unlink sockfile if it exists
    fs.stat(path, function (err, r) {
      if (err) {
        if (err.errno == ENOENT) {
          bind(self.fd, path);
          process.nextTick(function() {
            self._startWatcher();
          });
        } else {
          throw r;
        }
      } else {
        if (!r.isFile()) {
          throw new Error("Non-file exists at  " + path);
        } else {
          fs.unlink(path, function (err) {
            if (err) {
              throw err;
            } else {
              bind(self.fd, path);
              process.nextTick(function() {
                self._startWatcher();
              });
            }
          });
        }
      }
    });
  } else if (!arguments[1]) {
    // Don't bind(). OS will assign a port with INADDR_ANY.
    // The port can be found with server.address()
    self.type = 'udp4';
    self.fd = socket(self.type);
    bind(self.fd, arguments[0]);
    process.nextTick(function() {
      self._startWatcher();
    });
  } else {
    // the first argument is the port, the second an IP
    var port = arguments[0];
    dns.lookup(arguments[1], function (err, ip, addressType) {
      if (err) {
        self.emit('error', err);
      } else {
        self.type = addressType == 4 ? 'udp4' : 'udp6';
        self.fd = socket(self.type);
        bind(self.fd, port, ip);
        process.nextTick(function() {
          self._startWatcher();
        });
      }
    });
  }
};

Socket.prototype._startWatcher = function () {
  this.watcher.set(this.fd, true, false);
  this.watcher.start();
  this.emit("listening");
};

Socket.prototype.address = function () {
  return getsockname(this.fd);
};

Socket.prototype.send = function(port, addr, buffer, offset, length) {
  var self = this;

  var lastArg = arguments[arguments.length - 1];
  var callback = typeof lastArg === 'function' ? lastArg : null;

  if (!isPort(arguments[0])) {
    try {
      if (!self.fd) {
        self.type = 'unix_dgram';
        self.fd = socket(self.type);
      }
      var bytes = sendto(self.fd, buffer, offset, length, 0, port, addr);
    } catch (e) {
      if (callback) callback(e);
      return;
    }

    if (callback) callback(null, bytes);

  } else {
    dns.lookup(arguments[1], function (err, ip, addressType) {
      // DNS error
      if (err) {
        if (callback) callback(err);
        self.emit('error', err);
        return;
      }

      try {
        if (!self.fd) {
          self.type = addressType == 4 ? 'udp4' : 'udp6';
          self.fd = socket(self.type);
          setBroadcast(self.fd, self.broadcast);
          process.nextTick(function() {
            self._startWatcher();
          });
        }
        var bytes = sendto(self.fd, buffer, offset, length, 0, port, ip);
      } catch (err) {
        // socket creation, or sendto error.
        if (callback) callback(err);
        return;
      }

      if (callback) callback(null, bytes);
    });
  }
};

Socket.prototype.close = function () {
  var self = this;

  if (!self.fd) throw new Error('Not running');

  self.watcher.stop();

  close(self.fd);
  self.fd = null;

  if (self.type === "unix_dgram") {
    fs.unlink(self.path, function () {
      self.emit("close");
    });
  } else {
    self.emit("close");
  }
};

