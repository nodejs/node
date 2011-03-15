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

var util = require('util');
var fs = require('fs');
var events = require('events');
var dns = require('dns');

var IOWatcher = process.binding('io_watcher').IOWatcher;
var binding = process.binding('net');
var constants = process.binding('constants');

var socket = binding.socket;
var recvfrom = binding.recvfrom;
var close = binding.close;

var ENOENT = constants.ENOENT;

function isPort(x) { return parseInt(x) >= 0; }
var pool = null;

function getPool() {
  /* TODO: this effectively limits you to 8kb maximum packet sizes */
  var minPoolAvail = 1024 * 8;

  var poolSize = 1024 * 64;

  if (pool === null || (pool.used + minPoolAvail > pool.length)) {
    pool = new Buffer(poolSize);
    pool.used = 0;
  }

  return pool;
}

function dnsLookup(type, hostname, callback) {
  var family = (type ? ((type === 'udp6') ? 6 : 4) : null);
  dns.lookup(hostname, family, function(err, ip, addressFamily) {
    if (!err && family && addressFamily !== family) {
      err = new Error('no address found in family ' + type +
                      ' for ' + hostname);
    }
    callback(err, ip, addressFamily);
  });
}

function Socket(type, listener) {
  events.EventEmitter.call(this);
  var self = this;

  self.type = type;
  if (type === 'unix_dgram' || type === 'udp4' || type === 'udp6') {
    self.fd = socket(self.type);
  } else {
    throw new Error('Bad socket type specified.  Valid types are: ' +
                    'unix_dgram, udp4, udp6');
  }

  if (typeof listener === 'function') {
    self.on('message', listener);
  }

  self.watcher = new IOWatcher();
  self.watcher.host = self;
  self.watcher.callback = function() {
    while (self.fd) {
      var p = getPool();
      var rinfo = recvfrom(self.fd, p, p.used, p.length - p.used, 0);

      if (!rinfo) return;

      self.emit('message', p.slice(p.used, p.used + rinfo.size), rinfo);

      p.used += rinfo.size;
    }
  };

  if (self.type === 'udp4' || self.type === 'udp6') {
    self._startWatcher();
  }
}

util.inherits(Socket, events.EventEmitter);
exports.Socket = Socket;

exports.createSocket = function(type, listener) {
  return new Socket(type, listener);
};

Socket.prototype.bind = function() {
  var self = this;

  if (this.type === 'unix_dgram') {
    // bind(path)
    if (typeof arguments[0] !== 'string') {
      throw new Error('unix_dgram sockets must be bound to a path in ' +
                      'the filesystem');
    }
    this.path = arguments[0];

    // unlink old file, OK if it doesn't exist
    fs.unlink(this.path, function(err) {
      if (err && err.errno !== ENOENT) {
        throw err;
      } else {
        try {
          binding.bind(self.fd, self.path);
          self._startWatcher();
          self.emit('listening');
        } catch (err) {
          console.log('Error in unix_dgram bind of ' + self.path);
          console.log(err.stack);
          throw err;
        }
      }
    });
  } else if (this.type === 'udp4' || this.type === 'udp6') {
    // bind(port, [address])
    if (arguments[1] === undefined) {
      // Not bind()ing a specific address. Use INADDR_ANY and OS will pick one.
      // The address can be found with server.address()
      binding.bind(self.fd, arguments[0]);
      this.emit('listening');
    } else {
      // the first argument is the port, the second an address
      this.port = arguments[0];
      dnsLookup(this.type, arguments[1], function(err, ip, addressFamily) {
        if (err) {
          self.emit('error', err);
        } else {
          self.ip = ip;
          binding.bind(self.fd, self.port, ip);
          self.emit('listening');
        }
      });
    }
  }
};

Socket.prototype._startWatcher = function() {
  if (! this._watcherStarted) {
    // listen for read ready, not write ready
    this.watcher.set(this.fd, true, false);
    this.watcher.start();
    this._watcherStarted = true;
  }
};

Socket.prototype.address = function() {
  return binding.getsockname(this.fd);
};

Socket.prototype.setBroadcast = function(arg) {
  if (arg) {
    return binding.setBroadcast(this.fd, 1);
  } else {
    return binding.setBroadcast(this.fd, 0);
  }
};

Socket.prototype.setTTL = function(arg) {
  var newttl = parseInt(arg);

  if (newttl > 0 && newttl < 256) {
    return binding.setTTL(this.fd, newttl);
  } else {
    throw new Error('New TTL must be between 1 and 255');
  }
};

Socket.prototype.setMulticastTTL = function(arg) {
  var newttl = parseInt(arg);

  if (newttl >= 0 && newttl < 256) {
    return binding.setMulticastTTL(this.fd, newttl);
  } else {
    throw new Error('New MulticastTTL must be between 0 and 255');
  }
};

Socket.prototype.setMulticastLoopback = function(arg) {
  if (arg) {
    return binding.setMulticastLoopback(this.fd, 1);
  } else {
    return binding.setMulticastLoopback(this.fd, 0);
  }
};

Socket.prototype.addMembership = function(multicastAddress,
                                          multicastInterface) {
  var self = this;
  dnsLookup(this.type, multicastAddress, function(err, ip, addressFamily) {
    if (err) {  // DNS error
      self.emit('error', err);
      return;
    }
    binding.addMembership(self.fd, multicastAddress, multicastInterface);
  });
};

Socket.prototype.dropMembership = function(multicastAddress,
                                           multicastInterface) {
  var self = this;
  dnsLookup(this.type, multicastAddress, function(err, ip, addressFamily) {
    if (err) {  // DNS error
      self.emit('error', err);
      return;
    }
    binding.dropMembership(self.fd, multicastAddress, multicastInterface);
  });
};

// translate arguments from JS API into C++ API, possibly after DNS lookup
Socket.prototype.send = function(buffer, offset, length) {
  var self = this;

  if (typeof offset !== 'number' || typeof length !== 'number') {
    throw new Error('send takes offset and length as args 2 and 3');
  }

  if (this.type === 'unix_dgram') {
    // send(buffer, offset, length, path [, callback])
    if (typeof arguments[3] !== 'string') {
      throw new Error('unix_dgram sockets must send to a path ' +
                      'in the filesystem');
    }

    self.sendto(buffer, offset, length, arguments[3], null, arguments[4]);
  } else if (this.type === 'udp4' || this.type === 'udp6') {
    // send(buffer, offset, length, port, address [, callback])
    if (typeof arguments[4] !== 'string') {
      throw new Error(this.type + ' sockets must send to port, address');
    }

    if (binding.isIP(arguments[4])) {
      self.sendto(arguments[0], arguments[1], arguments[2], arguments[3],
                  arguments[4], arguments[5]);
    } else {
      var port = arguments[3],
          callback = arguments[5];
      dnsLookup(this.type, arguments[4], function(err, ip, addressFamily) {
        if (err) {  // DNS error
          if (callback) {
            callback(err);
          }
          self.emit('error', err);
          return;
        }
        self.sendto(buffer, offset, length, port, ip, callback);
      });
    }
  }
};

Socket.prototype.sendto = function(buffer,
                                   offset,
                                   length,
                                   port,
                                   addr,
                                   callback) {
  try {
    var bytes = binding.sendto(this.fd, buffer, offset, length, 0, port, addr);
  } catch (err) {
    if (callback) {
      callback(err);
    }
    return;
  }

  if (callback) {
    callback(null, bytes);
  }
};

Socket.prototype.close = function() {
  var self = this;

  if (!this.fd) throw new Error('Not running');

  this.watcher.stop();
  this._watcherStarted = false;

  close(this.fd);
  this.fd = null;

  if (this.type === 'unix_dgram' && this.path) {
    fs.unlink(this.path, function() {
      self.emit('close');
    });
  } else {
    this.emit('close');
  }
};
