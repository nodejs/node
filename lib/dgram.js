'use strict';

const assert = require('assert');
const util = require('util');
const events = require('events');
const constants = require('constants');

const UDP = process.binding('udp_wrap').UDP;
const SendWrap = process.binding('udp_wrap').SendWrap;

const BIND_STATE_UNBOUND = 0;
const BIND_STATE_BINDING = 1;
const BIND_STATE_BOUND = 2;

// lazily loaded
var cluster = null;
var dns = null;

const errnoException = util._errnoException;
const exceptionWithHostPort = util._exceptionWithHostPort;

function lookup(address, family, callback) {
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


exports._createSocketHandle = function(address, port, addressType, fd) {
  // Opening an existing fd is not supported for UDP handles.
  assert(typeof fd !== 'number' || fd < 0);

  var handle = newHandle(addressType);

  if (port || address) {
    var err = handle.bind(address, port || 0, 0);
    if (err) {
      handle.close();
      return err;
    }
  }

  return handle;
};


function Socket(type, listener) {
  events.EventEmitter.call(this);

  if (typeof type === 'object') {
    var options = type;
    type = options.type;
  }

  var handle = newHandle(type);
  handle.owner = this;

  this._handle = handle;
  this._receiving = false;
  this._bindState = BIND_STATE_UNBOUND;
  this.type = type;
  this.fd = null; // compatibility hack

  // If true - UV_UDP_REUSEADDR flag will be set
  this._reuseAddr = options && options.reuseAddr;

  if (typeof listener === 'function')
    this.on('message', listener);
}
util.inherits(Socket, events.EventEmitter);
exports.Socket = Socket;


exports.createSocket = function(type, listener) {
  return new Socket(type, listener);
};


function startListening(socket) {
  socket._handle.onmessage = onMessage;
  // Todo: handle errors
  socket._handle.recvStart();
  socket._receiving = true;
  socket._bindState = BIND_STATE_BOUND;
  socket.fd = -42; // compatibility hack

  socket.emit('listening');
}

function replaceHandle(self, newHandle) {

  // Set up the handle that we got from master.
  newHandle.lookup = self._handle.lookup;
  newHandle.bind = self._handle.bind;
  newHandle.send = self._handle.send;
  newHandle.owner = self;

  // Replace the existing handle by the handle we got from master.
  self._handle.close();
  self._handle = newHandle;
}

Socket.prototype.bind = function(port /*, address, callback*/) {
  var self = this;

  self._healthCheck();

  if (this._bindState != BIND_STATE_UNBOUND)
    throw new Error('Socket is already bound');

  this._bindState = BIND_STATE_BINDING;

  if (typeof arguments[arguments.length - 1] === 'function')
    self.once('listening', arguments[arguments.length - 1]);

  if (port instanceof UDP) {
    replaceHandle(self, port);
    startListening(self);
    return self;
  }

  var address;
  var exclusive;

  if (port !== null && typeof port === 'object') {
    address = port.address || '';
    exclusive = !!port.exclusive;
    port = port.port;
  } else {
    address = typeof arguments[1] === 'function' ? '' : arguments[1];
    exclusive = false;
  }

  // resolve address first
  self._handle.lookup(address, function(err, ip) {
    if (err) {
      self._bindState = BIND_STATE_UNBOUND;
      self.emit('error', err);
      return;
    }

    if (!cluster)
      cluster = require('cluster');

    if (cluster.isWorker && !exclusive) {
      cluster._getServer(self, ip, port, self.type, -1, function(err, handle) {
        if (err) {
          var ex = exceptionWithHostPort(err, 'bind', ip, port);
          self.emit('error', ex);
          self._bindState = BIND_STATE_UNBOUND;
          return;
        }

        if (!self._handle)
          // handle has been closed in the mean time.
          return handle.close();

        replaceHandle(self, handle);
        startListening(self);
      });

    } else {
      if (!self._handle)
        return; // handle has been closed in the mean time

      var flags = 0;
      if (self._reuseAddr)
        flags |= constants.UV_UDP_REUSEADDR;

      var err = self._handle.bind(ip, port || 0, flags);
      if (err) {
        var ex = exceptionWithHostPort(err, 'bind', ip, port);
        self.emit('error', ex);
        self._bindState = BIND_STATE_UNBOUND;
        // Todo: close?
        return;
      }

      startListening(self);
    }
  });

  return self;
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

  if (typeof buffer === 'string')
    buffer = new Buffer(buffer);

  if (!(buffer instanceof Buffer))
    throw new TypeError('First argument must be a buffer or string.');

  offset = offset | 0;
  if (offset < 0)
    throw new RangeError('Offset should be >= 0');

  if ((length == 0 && offset > buffer.length) ||
      (length > 0 && offset >= buffer.length))
    throw new RangeError('Offset into buffer too large');

  // Sending a zero-length datagram is kind of pointless but it _is_
  // allowed, hence check that length >= 0 rather than > 0.
  length = length | 0;
  if (length < 0)
    throw new RangeError('Length should be >= 0');

  if (offset + length > buffer.length)
    throw new RangeError('Offset + length beyond buffer length');

  port = port | 0;
  if (port <= 0 || port > 65535)
    throw new RangeError('Port should be > 0 and < 65536');

  // Normalize callback so it's either a function or undefined but not anything
  // else.
  if (typeof callback !== 'function')
    callback = undefined;

  self._healthCheck();

  if (self._bindState == BIND_STATE_UNBOUND)
    self.bind({port: 0, exclusive: true}, null);

  // If the socket hasn't been bound yet, push the outbound packet onto the
  // send queue and send after binding is complete.
  if (self._bindState != BIND_STATE_BOUND) {
    // If the send queue hasn't been initialized yet, do it, and install an
    // event handler that flushes the send queue after binding is done.
    if (!self._sendQueue) {
      self._sendQueue = [];
      self.once('listening', function() {
        // Flush the send queue.
        for (var i = 0; i < self._sendQueue.length; i++)
          self.send.apply(self, self._sendQueue[i]);
        self._sendQueue = undefined;
      });
    }
    self._sendQueue.push([buffer, offset, length, port, address, callback]);
    return;
  }

  self._handle.lookup(address, function(ex, ip) {
    if (ex) {
      if (callback) callback(ex);
      self.emit('error', ex);
    }
    else if (self._handle) {
      var req = new SendWrap();
      req.buffer = buffer;  // Keep reference alive.
      req.length = length;
      req.address = address;
      req.port = port;
      if (callback) {
        req.callback = callback;
        req.oncomplete = afterSend;
      }
      var err = self._handle.send(req,
                                  buffer,
                                  offset,
                                  length,
                                  port,
                                  ip,
                                  !!callback);
      if (err && callback) {
        // don't emit as error, dgram_legacy.js compatibility
        process.nextTick(function() {
          var ex = exceptionWithHostPort(err, 'send', address, port);
          callback(ex);
        });
      }
    }
  });
};


function afterSend(err) {
  if (err) {
    err = exceptionWithHostPort(err, 'send', this.address, this.port);
  }
  this.callback(err, this.length);
}


Socket.prototype.close = function(callback) {
  if (typeof callback === 'function')
    this.on('close', callback);
  this._healthCheck();
  this._stopReceiving();
  this._handle.close();
  this._handle = null;
  var self = this;
  process.nextTick(function() {
    self.emit('close');
  });

  return this;
};


Socket.prototype.address = function() {
  this._healthCheck();

  var out = {};
  var err = this._handle.getsockname(out);
  if (err) {
    throw errnoException(err, 'getsockname');
  }

  return out;
};


Socket.prototype.setBroadcast = function(arg) {
  var err = this._handle.setBroadcast(arg ? 1 : 0);
  if (err) {
    throw errnoException(err, 'setBroadcast');
  }
};


Socket.prototype.setTTL = function(arg) {
  if (typeof arg !== 'number') {
    throw new TypeError('Argument must be a number');
  }

  var err = this._handle.setTTL(arg);
  if (err) {
    throw errnoException(err, 'setTTL');
  }

  return arg;
};


Socket.prototype.setMulticastTTL = function(arg) {
  if (typeof arg !== 'number') {
    throw new TypeError('Argument must be a number');
  }

  var err = this._handle.setMulticastTTL(arg);
  if (err) {
    throw errnoException(err, 'setMulticastTTL');
  }

  return arg;
};


Socket.prototype.setMulticastLoopback = function(arg) {
  var err = this._handle.setMulticastLoopback(arg ? 1 : 0);
  if (err) {
    throw errnoException(err, 'setMulticastLoopback');
  }

  return arg; // 0.4 compatibility
};


Socket.prototype.addMembership = function(multicastAddress,
                                          interfaceAddress) {
  this._healthCheck();

  if (!multicastAddress) {
    throw new Error('multicast address must be specified');
  }

  var err = this._handle.addMembership(multicastAddress, interfaceAddress);
  if (err) {
    throw errnoException(err, 'addMembership');
  }
};


Socket.prototype.dropMembership = function(multicastAddress,
                                           interfaceAddress) {
  this._healthCheck();

  if (!multicastAddress) {
    throw new Error('multicast address must be specified');
  }

  var err = this._handle.dropMembership(multicastAddress, interfaceAddress);
  if (err) {
    throw errnoException(err, 'dropMembership');
  }
};


Socket.prototype._healthCheck = function() {
  if (!this._handle)
    throw new Error('Not running'); // error message from dgram_legacy.js
};


Socket.prototype._stopReceiving = function() {
  if (!this._receiving)
    return;

  this._handle.recvStop();
  this._receiving = false;
  this.fd = null; // compatibility hack
};


function onMessage(nread, handle, buf, rinfo) {
  var self = handle.owner;
  if (nread < 0) {
    return self.emit('error', errnoException(nread, 'recvmsg'));
  }
  rinfo.size = buf.length; // compatibility
  self.emit('message', buf, rinfo);
}


Socket.prototype.ref = function() {
  if (this._handle)
    this._handle.ref();
};


Socket.prototype.unref = function() {
  if (this._handle)
    this._handle.unref();
};
