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
var events = require('events');

var UDP = process.binding('udp_wrap').UDP;

// lazily loaded
var dns = null;
var net = null;


// no-op callback
function noop() {
}


function isIP(address) {
  if (!net)
    net = require('net');

  return net.isIP(address);
}


function lookup(address, family, callback) {
  // implicit 'bind before send' needs to run on the same tick
  var matchedFamily = isIP(address);
  if (matchedFamily)
    return callback(null, address, matchedFamily);

  if (!dns)
    dns = require('dns');

  return dns.lookup(address, family, callback);
}


function lookup4(address, callback) {
  return lookup(address || '0.0.0.0', 4, callback);
}


function lookup6(address, callback) {
  return lookup(address || '::0', 6, callback);
}


function newHandle(type) {
  if (type == 'udp4') {
    var handle = new UDP;
    handle.lookup = lookup4;
    return handle;
  }

  if (type == 'udp6') {
    var handle = new UDP;
    handle.lookup = lookup6;
    handle.bind = handle.bind6;
    handle.send = handle.send6;
    return handle;
  }

  if (type == 'unix_dgram')
    throw new Error('unix_dgram sockets are not supported any more.');

  throw new Error('Bad socket type specified. Valid types are: udp4, udp6');
}


function Socket(type, listener) {
  events.EventEmitter.call(this);

  var handle = newHandle(type);
  handle.socket = this;

  this._handle = handle;
  this._receiving = false;
  this._bound = false;
  this.type = type;
  this.fd = null; // compatibility hack

  if (typeof listener === 'function')
    this.on('message', listener);
}
util.inherits(Socket, events.EventEmitter);
exports.Socket = Socket;


exports.createSocket = function(type, listener) {
  return new Socket(type, listener);
};


Socket.prototype.bind = function(port, address) {
  var self = this;

  self._healthCheck();

  // resolve address first
  self._handle.lookup(address, function(err, ip) {
    if (!err) {
      if (self._handle.bind(ip, port || 0, /*flags=*/0)) {
        err = errnoException(errno, 'bind');
      }
      else {
        self._bound = true;
        self.emit('listening');
        self._startReceiving();
      }
    }

    if (err) {
      // caller may not have had a chance yet to register its
      // error event listener so defer the error to the next tick
      process.nextTick(function() {
        self.emit('error', err);
      });
    }
  });
};


// thin wrapper around `send`, here for compatibility with dgram_legacy.js
Socket.prototype.sendto = function(buffer,
                                   offset,
                                   length,
                                   port,
                                   address,
                                   callback) {
  if (typeof offset !== 'number' || typeof length !== 'number')
    throw new Error('send takes offset and length as args 2 and 3');

  if (typeof address !== 'string')
    throw new Error(this.type + ' sockets must send to port, address');

  this.send(buffer, offset, length, port, address, callback);
};


Socket.prototype.send = function(buffer,
                                 offset,
                                 length,
                                 port,
                                 address,
                                 callback) {
  var self = this;

  callback = callback || noop;

  self._healthCheck();
  self._startReceiving();

  self._handle.lookup(address, function(err, ip) {
    if (err) {
      if (callback) callback(err);
      self.emit('error', err);
    }
    else if (self._handle) {
      var req = self._handle.send(buffer, offset, length, port, ip);
      if (req) {
        req.oncomplete = afterSend;
        req.cb = callback;
      }
      else {
        // don't emit as error, dgram_legacy.js compatibility
        callback(errnoException(errno, 'send'));
      }
    }
  });
};


function afterSend(status, handle, req, buffer) {
  var self = handle.socket;

  // CHECKME socket's been closed by user, don't call callback?
  if (handle !== self._handle)
    void(0);

  if (req.cb)
    req.cb(null, buffer.length); // compatibility with dgram_legacy.js
}


Socket.prototype.close = function() {
  this._healthCheck();
  this._stopReceiving();
  this._handle.close();
  this._handle = null;
  this.emit('close');
};


Socket.prototype.address = function() {
  this._healthCheck();

  var address = this._handle.getsockname();
  if (!address)
    throw errnoException(errno, 'getsockname');

  return address;
};


Socket.prototype.setBroadcast = function(arg) {
  if (this._handle.setBroadcast((arg) ? 1 : 0)) {
    throw errnoException(errno, 'setBroadcast');
  }
};


Socket.prototype.setTTL = function(arg) {
  if (typeof arg !== 'number') {
    throw new TypeError('Argument must be a number');
  }

  if (this._handle.setTTL(arg)) {
    throw errnoException(errno, 'setTTL');
  }

  return arg;
};


Socket.prototype.setMulticastTTL = function(arg) {
  if (typeof arg !== 'number') {
    throw new TypeError('Argument must be a number');
  }

  if (this._handle.setMulticastTTL(arg)) {
    throw errnoException(errno, 'setMulticastTTL');
  }

  return arg;
};


Socket.prototype.setMulticastLoopback = function(arg) {
  arg = arg ? 1 : 0;

  if (this._handle.setMulticastLoopback(arg)) {
    throw errnoException(errno, 'setMulticastLoopback');
  }

  return arg; // 0.4 compatibility
};


Socket.prototype.addMembership = function(multicastAddress,
                                          interfaceAddress) {
  this._healthCheck();

  if (!multicastAddress) {
    throw new Error('multicast address must be specified');
  }

  if (this._handle.addMembership(multicastAddress, interfaceAddress)) {
    throw new errnoException(errno, 'addMembership');
  }
};


Socket.prototype.dropMembership = function(multicastAddress,
                                           interfaceAddress) {
  this._healthCheck();

  if (!multicastAddress) {
    throw new Error('multicast address must be specified');
  }

  if (this._handle.dropMembership(multicastAddress, interfaceAddress)) {
    throw new errnoException(errno, 'dropMembership');
  }
};


Socket.prototype._healthCheck = function() {
  if (!this._handle)
    throw new Error('Not running'); // error message from dgram_legacy.js
};


Socket.prototype._startReceiving = function() {
  if (this._receiving)
    return;

  if (!this._bound) {
    this.bind(); // bind to random port

    // sanity check
    if (!this._bound)
      throw new Error('implicit bind failed');
  }

  this._handle.onmessage = onMessage;
  this._handle.recvStart();
  this._receiving = true;
  this.fd = -42; // compatibility hack
};


Socket.prototype._stopReceiving = function() {
  if (!this._receiving)
    return;

  // Black hole messages coming in when reading is stopped. Libuv might do
  // this, but node applications (e.g. test/simple/test-dgram-pingpong) may
  // not expect it.
  this._handle.onmessage = noop;

  this._handle.recvStop();
  this._receiving = false;
  this.fd = null; // compatibility hack
};


function onMessage(handle, nread, buf, rinfo) {
  var self = handle.socket;

  if (nread == -1) {
    self.emit('error', errnoException(errno, 'recvmsg'));
  }
  else {
    rinfo.size = buf.length; // compatibility
    self.emit('message', buf, rinfo);
  }
}


// TODO share with net_uv and others
function errnoException(errorno, syscall) {
  var e = new Error(syscall + ' ' + errorno);
  e.errno = e.code = errorno;
  e.syscall = syscall;
  return e;
}
