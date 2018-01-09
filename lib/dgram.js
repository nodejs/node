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

'use strict';

const assert = require('assert');
const errors = require('internal/errors');
const { Buffer } = require('buffer');
const dns = require('dns');
const util = require('util');
const { isUint8Array } = require('internal/util/types');
const EventEmitter = require('events');
const { defaultTriggerAsyncIdScope } = require('internal/async_hooks');
const { UV_UDP_REUSEADDR } = process.binding('constants').os;
const { async_id_symbol } = process.binding('async_wrap');
const { nextTick } = require('internal/process/next_tick');

const { UDP, SendWrap } = process.binding('udp_wrap');

const BIND_STATE_UNBOUND = 0;
const BIND_STATE_BINDING = 1;
const BIND_STATE_BOUND = 2;

const RECV_BUFFER = true;
const SEND_BUFFER = false;

// Lazily loaded
var cluster = null;

const errnoException = util._errnoException;
const exceptionWithHostPort = util._exceptionWithHostPort;


function lookup4(lookup, address, callback) {
  return lookup(address || '127.0.0.1', 4, callback);
}


function lookup6(lookup, address, callback) {
  return lookup(address || '::1', 6, callback);
}


function newHandle(type, lookup) {
  if (lookup === undefined)
    lookup = dns.lookup;
  else if (typeof lookup !== 'function')
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'lookup', 'Function');

  if (type === 'udp4') {
    const handle = new UDP();
    handle.lookup = lookup4.bind(handle, lookup);
    return handle;
  }

  if (type === 'udp6') {
    const handle = new UDP();
    handle.lookup = lookup6.bind(handle, lookup);
    handle.bind = handle.bind6;
    handle.send = handle.send6;
    return handle;
  }

  throw new errors.TypeError('ERR_SOCKET_BAD_TYPE');
}


function _createSocketHandle(address, port, addressType, fd, flags) {
  // Opening an existing fd is not supported for UDP handles.
  assert(typeof fd !== 'number' || fd < 0);

  var handle = newHandle(addressType);

  if (port || address) {
    var err = handle.bind(address, port || 0, flags);
    if (err) {
      handle.close();
      return err;
    }
  }

  return handle;
}

const kOptionSymbol = Symbol('options symbol');

function Socket(type, listener) {
  EventEmitter.call(this);
  var lookup;

  this[kOptionSymbol] = {};
  if (type !== null && typeof type === 'object') {
    var options = type;
    type = options.type;
    lookup = options.lookup;
    this[kOptionSymbol].recvBufferSize = options.recvBufferSize;
    this[kOptionSymbol].sendBufferSize = options.sendBufferSize;
  }

  var handle = newHandle(type, lookup);
  handle.owner = this;

  this._handle = handle;
  this._receiving = false;
  this._bindState = BIND_STATE_UNBOUND;
  this[async_id_symbol] = this._handle.getAsyncId();
  this.type = type;
  this.fd = null; // compatibility hack

  // If true - UV_UDP_REUSEADDR flag will be set
  this._reuseAddr = options && options.reuseAddr;

  if (typeof listener === 'function')
    this.on('message', listener);
}
util.inherits(Socket, EventEmitter);


function createSocket(type, listener) {
  return new Socket(type, listener);
}


function startListening(socket) {
  socket._handle.onmessage = onMessage;
  // Todo: handle errors
  socket._handle.recvStart();
  socket._receiving = true;
  socket._bindState = BIND_STATE_BOUND;
  socket.fd = -42; // compatibility hack

  if (socket[kOptionSymbol].recvBufferSize)
    bufferSize(socket, socket[kOptionSymbol].recvBufferSize, RECV_BUFFER);

  if (socket[kOptionSymbol].sendBufferSize)
    bufferSize(socket, socket[kOptionSymbol].sendBufferSize, SEND_BUFFER);

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

function bufferSize(self, size, buffer) {
  if (size >>> 0 !== size)
    throw new errors.TypeError('ERR_SOCKET_BAD_BUFFER_SIZE');

  const ctx = {};
  const ret = self._handle.bufferSize(size, buffer, ctx);
  if (ret === undefined) {
    throw new errors.Error('ERR_SOCKET_BUFFER_SIZE',
                           new errors.SystemError(ctx));
  }
  return ret;
}

Socket.prototype.bind = function(port_, address_ /*, callback*/) {
  let port = port_;

  this._healthCheck();

  if (this._bindState !== BIND_STATE_UNBOUND)
    throw new errors.Error('ERR_SOCKET_ALREADY_BOUND');

  this._bindState = BIND_STATE_BINDING;

  if (arguments.length && typeof arguments[arguments.length - 1] === 'function')
    this.once('listening', arguments[arguments.length - 1]);

  if (port instanceof UDP) {
    replaceHandle(this, port);
    startListening(this);
    return this;
  }

  var address;
  var exclusive;

  if (port !== null && typeof port === 'object') {
    address = port.address || '';
    exclusive = !!port.exclusive;
    port = port.port;
  } else {
    address = typeof address_ === 'function' ? '' : address_;
    exclusive = false;
  }

  // defaulting address for bind to all interfaces
  if (!address) {
    if (this.type === 'udp4')
      address = '0.0.0.0';
    else
      address = '::';
  }

  // resolve address first
  this._handle.lookup(address, (err, ip) => {
    if (err) {
      this._bindState = BIND_STATE_UNBOUND;
      this.emit('error', err);
      return;
    }

    if (!cluster)
      cluster = require('cluster');

    var flags = 0;
    if (this._reuseAddr)
      flags |= UV_UDP_REUSEADDR;

    if (cluster.isWorker && !exclusive) {
      const onHandle = (err, handle) => {
        if (err) {
          var ex = exceptionWithHostPort(err, 'bind', ip, port);
          this.emit('error', ex);
          this._bindState = BIND_STATE_UNBOUND;
          return;
        }

        if (!this._handle)
          // handle has been closed in the mean time.
          return handle.close();

        replaceHandle(this, handle);
        startListening(this);
      };
      cluster._getServer(this, {
        address: ip,
        port: port,
        addressType: this.type,
        fd: -1,
        flags: flags
      }, onHandle);
    } else {
      if (!this._handle)
        return; // handle has been closed in the mean time

      const err = this._handle.bind(ip, port || 0, flags);
      if (err) {
        var ex = exceptionWithHostPort(err, 'bind', ip, port);
        this.emit('error', ex);
        this._bindState = BIND_STATE_UNBOUND;
        // Todo: close?
        return;
      }

      startListening(this);
    }
  });

  return this;
};


// thin wrapper around `send`, here for compatibility with dgram_legacy.js
Socket.prototype.sendto = function(buffer,
                                   offset,
                                   length,
                                   port,
                                   address,
                                   callback) {
  if (typeof offset !== 'number') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'offset', 'number');
  }

  if (typeof length !== 'number') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'length', 'number');
  }

  if (typeof port !== 'number') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'port', 'number');
  }

  if (typeof address !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'address', 'string');
  }

  this.send(buffer, offset, length, port, address, callback);
};


function sliceBuffer(buffer, offset, length) {
  if (typeof buffer === 'string') {
    buffer = Buffer.from(buffer);
  } else if (!isUint8Array(buffer)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'buffer',
                               ['Buffer', 'Uint8Array', 'string']);
  }

  offset = offset >>> 0;
  length = length >>> 0;

  return buffer.slice(offset, offset + length);
}


function fixBufferList(list) {
  const newlist = new Array(list.length);

  for (var i = 0, l = list.length; i < l; i++) {
    var buf = list[i];
    if (typeof buf === 'string')
      newlist[i] = Buffer.from(buf);
    else if (!isUint8Array(buf))
      return null;
    else
      newlist[i] = buf;
  }

  return newlist;
}


function enqueue(self, toEnqueue) {
  // If the send queue hasn't been initialized yet, do it, and install an
  // event handler that flushes the send queue after binding is done.
  if (!self._queue) {
    self._queue = [];
    self.once('error', onListenError);
    self.once('listening', onListenSuccess);
  }
  self._queue.push(toEnqueue);
}


function onListenSuccess() {
  this.removeListener('error', onListenError);
  clearQueue.call(this);
}


function onListenError(err) {
  this.removeListener('listening', onListenSuccess);
  this._queue = undefined;
  this.emit('error', new errors.Error('ERR_SOCKET_CANNOT_SEND'));
}


function clearQueue() {
  const queue = this._queue;
  this._queue = undefined;

  // Flush the send queue.
  for (var i = 0; i < queue.length; i++)
    queue[i]();
}


// valid combinations
// send(buffer, offset, length, port, address, callback)
// send(buffer, offset, length, port, address)
// send(buffer, offset, length, port, callback)
// send(buffer, offset, length, port)
// send(bufferOrList, port, address, callback)
// send(bufferOrList, port, address)
// send(bufferOrList, port, callback)
// send(bufferOrList, port)
Socket.prototype.send = function(buffer,
                                 offset,
                                 length,
                                 port,
                                 address,
                                 callback) {
  let list;

  if (address || (port && typeof port !== 'function')) {
    buffer = sliceBuffer(buffer, offset, length);
  } else {
    callback = port;
    port = offset;
    address = length;
  }

  if (!Array.isArray(buffer)) {
    if (typeof buffer === 'string') {
      list = [ Buffer.from(buffer) ];
    } else if (!isUint8Array(buffer)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'buffer',
                                 ['Buffer', 'Uint8Array', 'string']);
    } else {
      list = [ buffer ];
    }
  } else if (!(list = fixBufferList(buffer))) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'buffer list arguments',
                               ['Buffer', 'string']);
  }

  port = port >>> 0;
  if (port === 0 || port > 65535)
    throw new errors.RangeError('ERR_SOCKET_BAD_PORT', port);

  // Normalize callback so it's either a function or undefined but not anything
  // else.
  if (typeof callback !== 'function')
    callback = undefined;

  if (typeof address === 'function') {
    callback = address;
    address = undefined;
  } else if (address && typeof address !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'address',
                               ['string', 'falsy']);
  }

  this._healthCheck();

  if (this._bindState === BIND_STATE_UNBOUND)
    this.bind({ port: 0, exclusive: true }, null);

  if (list.length === 0)
    list.push(Buffer.alloc(0));

  // If the socket hasn't been bound yet, push the outbound packet onto the
  // send queue and send after binding is complete.
  if (this._bindState !== BIND_STATE_BOUND) {
    enqueue(this, this.send.bind(this, list, port, address, callback));
    return;
  }

  const afterDns = (ex, ip) => {
    defaultTriggerAsyncIdScope(
      this[async_id_symbol],
      doSend,
      ex, this, ip, list, address, port, callback
    );
  };

  this._handle.lookup(address, afterDns);
};

function doSend(ex, self, ip, list, address, port, callback) {
  if (ex) {
    if (typeof callback === 'function') {
      process.nextTick(callback, ex);
      return;
    }

    process.nextTick(() => self.emit('error', ex));
    return;
  } else if (!self._handle) {
    return;
  }

  var req = new SendWrap();
  req.list = list;  // Keep reference alive.
  req.address = address;
  req.port = port;
  if (callback) {
    req.callback = callback;
    req.oncomplete = afterSend;
  }

  var err = self._handle.send(req,
                              list,
                              list.length,
                              port,
                              ip,
                              !!callback);

  if (err && callback) {
    // don't emit as error, dgram_legacy.js compatibility
    const ex = exceptionWithHostPort(err, 'send', address, port);
    process.nextTick(callback, ex);
  }
}

function afterSend(err, sent) {
  if (err) {
    err = exceptionWithHostPort(err, 'send', this.address, this.port);
  } else {
    err = null;
  }

  this.callback(err, sent);
}

Socket.prototype.close = function(callback) {
  if (typeof callback === 'function')
    this.on('close', callback);

  if (this._queue) {
    this._queue.push(this.close.bind(this));
    return this;
  }

  this._healthCheck();
  this._stopReceiving();
  this._handle.close();
  this._handle = null;
  nextTick(this[async_id_symbol], socketCloseNT, this);

  return this;
};


function socketCloseNT(self) {
  self.emit('close');
}


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


Socket.prototype.setTTL = function(ttl) {
  if (typeof ttl !== 'number') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'ttl', 'number', ttl);
  }

  var err = this._handle.setTTL(ttl);
  if (err) {
    throw errnoException(err, 'setTTL');
  }

  return ttl;
};


Socket.prototype.setMulticastTTL = function(ttl) {
  if (typeof ttl !== 'number') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'ttl', 'number', ttl);
  }

  var err = this._handle.setMulticastTTL(ttl);
  if (err) {
    throw errnoException(err, 'setMulticastTTL');
  }

  return ttl;
};


Socket.prototype.setMulticastLoopback = function(arg) {
  var err = this._handle.setMulticastLoopback(arg ? 1 : 0);
  if (err) {
    throw errnoException(err, 'setMulticastLoopback');
  }

  return arg; // 0.4 compatibility
};


Socket.prototype.setMulticastInterface = function(interfaceAddress) {
  this._healthCheck();

  if (typeof interfaceAddress !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'interfaceAddress',
                               'string');
  }

  const err = this._handle.setMulticastInterface(interfaceAddress);
  if (err) {
    throw errnoException(err, 'setMulticastInterface');
  }
};

Socket.prototype.addMembership = function(multicastAddress,
                                          interfaceAddress) {
  this._healthCheck();

  if (!multicastAddress) {
    throw new errors.TypeError('ERR_MISSING_ARGS', 'multicastAddress');
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
    throw new errors.TypeError('ERR_MISSING_ARGS', 'multicastAddress');
  }

  var err = this._handle.dropMembership(multicastAddress, interfaceAddress);
  if (err) {
    throw errnoException(err, 'dropMembership');
  }
};


Socket.prototype._healthCheck = function() {
  if (!this._handle) {
    // Error message from dgram_legacy.js.
    throw new errors.Error('ERR_SOCKET_DGRAM_NOT_RUNNING');
  }
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

  return this;
};


Socket.prototype.unref = function() {
  if (this._handle)
    this._handle.unref();

  return this;
};


Socket.prototype.setRecvBufferSize = function(size) {
  bufferSize(this, size, RECV_BUFFER);
};


Socket.prototype.setSendBufferSize = function(size) {
  bufferSize(this, size, SEND_BUFFER);
};


Socket.prototype.getRecvBufferSize = function() {
  return bufferSize(this, 0, RECV_BUFFER);
};


Socket.prototype.getSendBufferSize = function() {
  return bufferSize(this, 0, SEND_BUFFER);
};


module.exports = {
  _createSocketHandle,
  createSocket,
  Socket
};
