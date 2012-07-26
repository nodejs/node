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

var events = require('events');
var Stream = require('stream');
var timers = require('timers');
var util = require('util');
var assert = require('assert');
var cluster;

function noop() {}

// constructor for lazy loading
function createPipe() {
  var Pipe = process.binding('pipe_wrap').Pipe;
  return new Pipe();
}

// constructor for lazy loading
function createTCP() {
  var TCP = process.binding('tcp_wrap').TCP;
  return new TCP();
}


/* Bit flags for socket._flags */
var FLAG_GOT_EOF = 1 << 0;
var FLAG_SHUTDOWN = 1 << 1;
var FLAG_DESTROY_SOON = 1 << 2;
var FLAG_SHUTDOWN_QUEUED = 1 << 3;


var debug;
if (process.env.NODE_DEBUG && /net/.test(process.env.NODE_DEBUG)) {
  debug = function(x) { console.error('NET:', x); };
} else {
  debug = function() { };
}


function isPipeName(s) {
  return typeof s === 'string' && toNumber(s) === false;
}


exports.createServer = function() {
  return new Server(arguments[0], arguments[1]);
};


// Target API:
//
// var s = net.connect({port: 80, host: 'google.com'}, function() {
//   ...
// });
//
// There are various forms:
//
// connect(options, [cb])
// connect(port, [host], [cb])
// connect(path, [cb]);
//
exports.connect = exports.createConnection = function() {
  var args = normalizeConnectArgs(arguments);
  var s = new Socket(args[0]);
  return Socket.prototype.connect.apply(s, args);
};

// Returns an array [options] or [options, cb]
// It is the same as the argument of Socket.prototype.connect().
function normalizeConnectArgs(args) {
  var options = {};

  if (typeof args[0] === 'object') {
    // connect(options, [cb])
    options = args[0];
  } else if (isPipeName(args[0])) {
    // connect(path, [cb]);
    options.path = args[0];
  } else {
    // connect(port, [host], [cb])
    options.port = args[0];
    if (typeof args[1] === 'string') {
      options.host = args[1];
    }
  }

  var cb = args[args.length - 1];
  return (typeof cb === 'function') ? [options, cb] : [options];
}


/* called when creating new Socket, or when re-using a closed Socket */
function initSocketHandle(self) {
  self._pendingWriteReqs = 0;

  self._flags = 0;
  self._connectQueueSize = 0;
  self.destroyed = false;
  self.errorEmitted = false;
  self.bytesRead = 0;
  self._bytesDispatched = 0;

  // Handle creation may be deferred to bind() or connect() time.
  if (self._handle) {
    self._handle.owner = self;
    self._handle.onread = onread;
  }
}

function Socket(options) {
  if (!(this instanceof Socket)) return new Socket(options);

  Stream.call(this);

  switch (typeof options) {
    case 'number':
      options = { fd: options }; // Legacy interface.
      break;
    case 'undefined':
      options = {};
      break;
  }

  if (typeof options.fd === 'undefined') {
    this._handle = options && options.handle; // private
  } else {
    this._handle = createPipe();
    this._handle.open(options.fd);
    this.readable = this.writable = true;
  }

  initSocketHandle(this);
  this.allowHalfOpen = options && options.allowHalfOpen;
}
util.inherits(Socket, Stream);


exports.Socket = Socket;
exports.Stream = Socket; // Legacy naming.


Socket.prototype.listen = function() {
  var self = this;
  self.on('connection', arguments[0]);
  listen(self, null, null, null);
};


Socket.prototype.setTimeout = function(msecs, callback) {
  if (msecs > 0 && !isNaN(msecs) && isFinite(msecs)) {
    timers.enroll(this, msecs);
    timers.active(this);
    if (callback) {
      this.once('timeout', callback);
    }
  } else if (msecs === 0) {
    timers.unenroll(this);
    if (callback) {
      this.removeListener('timeout', callback);
    }
  }
};


Socket.prototype._onTimeout = function() {
  debug('_onTimeout');
  this.emit('timeout');
};


Socket.prototype.setNoDelay = function(enable) {
  // backwards compatibility: assume true when `enable` is omitted
  if (this._handle && this._handle.setNoDelay)
    this._handle.setNoDelay(typeof enable === 'undefined' ? true : !!enable);
};


Socket.prototype.setKeepAlive = function(setting, msecs) {
  if (this._handle && this._handle.setKeepAlive)
    this._handle.setKeepAlive(setting, ~~(msecs / 1000));
};


Socket.prototype.address = function() {
  if (this._handle && this._handle.getsockname) {
    return this._handle.getsockname();
  }
  return null;
};


Object.defineProperty(Socket.prototype, 'readyState', {
  get: function() {
    if (this._connecting) {
      return 'opening';
    } else if (this.readable && this.writable) {
      return 'open';
    } else if (this.readable && !this.writable) {
      return 'readOnly';
    } else if (!this.readable && this.writable) {
      return 'writeOnly';
    } else {
      return 'closed';
    }
  }
});


Object.defineProperty(Socket.prototype, 'bufferSize', {
  get: function() {
    if (this._handle) {
      return this._handle.writeQueueSize + this._connectQueueSize;
    }
  }
});


Socket.prototype.pause = function() {
  this._paused = true;
  if (this._connecting) {
    // will actually pause once the handle is established.
    return;
  }
  if (this._handle) {
    this._handle.readStop();
  }
};


Socket.prototype.resume = function() {
  this._paused = false;
  if (this._connecting) {
    // will actually resume once the handle is established.
    return;
  }
  if (this._handle) {
    this._handle.readStart();
  }
};


Socket.prototype.end = function(data, encoding) {
  if (this._connecting && ((this._flags & FLAG_SHUTDOWN_QUEUED) == 0)) {
    // still connecting, add data to buffer
    if (data) this.write(data, encoding);
    this.writable = false;
    this._flags |= FLAG_SHUTDOWN_QUEUED;
  }

  if (!this.writable) return;
  this.writable = false;

  if (data) this.write(data, encoding);
  DTRACE_NET_STREAM_END(this);

  if (!this.readable) {
    this.destroySoon();
  } else {
    this._flags |= FLAG_SHUTDOWN;
    var shutdownReq = this._handle.shutdown();

    if (!shutdownReq) {
      this._destroy(errnoException(errno, 'shutdown'));
      return false;
    }

    shutdownReq.oncomplete = afterShutdown;
  }

  return true;
};


function afterShutdown(status, handle, req) {
  var self = handle.owner;

  assert.ok(self._flags & FLAG_SHUTDOWN);
  assert.ok(!self.writable);

  // callback may come after call to destroy.
  if (self.destroyed) {
    return;
  }

  if (self._flags & FLAG_GOT_EOF || !self.readable) {
    self._destroy();
  } else {
  }
}


Socket.prototype.destroySoon = function() {
  this.writable = false;
  this._flags |= FLAG_DESTROY_SOON;

  if (this._pendingWriteReqs == 0) {
    this._destroy();
  }
};


Socket.prototype._connectQueueCleanUp = function(exception) {
  this._connecting = false;
  this._connectQueueSize = 0;
  this._connectQueue = null;
};


Socket.prototype._destroy = function(exception, cb) {
  var self = this;

  function fireErrorCallbacks() {
    if (cb) cb(exception);
    if (exception && !self.errorEmitted) {
      process.nextTick(function() {
        self.emit('error', exception);
      });
      self.errorEmitted = true;
    }
  };

  if (this.destroyed) {
    fireErrorCallbacks();
    return;
  }

  self._connectQueueCleanUp();

  debug('destroy');

  this.readable = this.writable = false;

  timers.unenroll(this);

  debug('close');
  if (this._handle) {
    this._handle.close();
    this._handle.onread = noop;
    this._handle = null;
  }

  fireErrorCallbacks();

  process.nextTick(function() {
    self.emit('close', exception ? true : false);
  });

  this.destroyed = true;

  if (this.server) {
    this.server._connections--;
    if (this.server._emitCloseIfDrained) {
      this.server._emitCloseIfDrained();
    }
  }
};


Socket.prototype.destroy = function(exception) {
  this._destroy(exception);
};


function onread(buffer, offset, length) {
  var handle = this;
  var self = handle.owner;
  assert.equal(handle, self._handle);

  timers.active(self);

  var end = offset + length;

  if (buffer) {
    // Emit 'data' event.

    if (self._decoder) {
      // Emit a string.
      var string = self._decoder.write(buffer.slice(offset, end));
      if (string.length) self.emit('data', string);
    } else {
      // Emit a slice. Attempt to avoid slicing the buffer if no one is
      // listening for 'data'.
      if (self._events && self._events['data']) {
        self.emit('data', buffer.slice(offset, end));
      }
    }

    self.bytesRead += length;

    // Optimization: emit the original buffer with end points
    if (self.ondata) self.ondata(buffer, offset, end);

  } else if (errno == 'EOF') {
    // EOF
    self.readable = false;

    assert.ok(!(self._flags & FLAG_GOT_EOF));
    self._flags |= FLAG_GOT_EOF;

    // We call destroy() before end(). 'close' not emitted until nextTick so
    // the 'end' event will come first as required.
    if (!self.writable) self._destroy();

    if (!self.allowHalfOpen) self.end();
    if (self._events && self._events['end']) self.emit('end');
    if (self.onend) self.onend();
  } else {
    // Error
    if (errno == 'ECONNRESET') {
      self._destroy();
    } else {
      self._destroy(errnoException(errno, 'read'));
    }
  }
}


Socket.prototype.setEncoding = function(encoding) {
  var StringDecoder = require('string_decoder').StringDecoder; // lazy load
  this._decoder = new StringDecoder(encoding);
};


Socket.prototype._getpeername = function() {
  if (!this._handle || !this._handle.getpeername) {
    return {};
  }
  if (!this._peername) {
    this._peername = this._handle.getpeername();
    // getpeername() returns null on error
    if (this._peername === null) {
      return {};
    }
  }
  return this._peername;
};


Socket.prototype.__defineGetter__('remoteAddress', function() {
  return this._getpeername().address;
});


Socket.prototype.__defineGetter__('remotePort', function() {
  return this._getpeername().port;
});


/*
 * Arguments data, [encoding], [cb]
 */
Socket.prototype.write = function(data, arg1, arg2) {
  var encoding, cb;

  // parse arguments
  if (arg1) {
    if (typeof arg1 === 'string') {
      encoding = arg1;
      cb = arg2;
    } else if (typeof arg1 === 'function') {
      cb = arg1;
    } else {
      throw new Error('bad arg');
    }
  }

  if (typeof data === 'string') {
    encoding = (encoding || 'utf8').toLowerCase();
    switch (encoding) {
      case 'utf8':
      case 'utf-8':
      case 'ascii':
      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
        // This encoding can be handled in the binding layer.
        break;

      default:
        data = new Buffer(data, encoding);
    }
  } else if (!Buffer.isBuffer(data)) {
    throw new TypeError('First argument must be a buffer or a string.');
  }

  // If we are still connecting, then buffer this for later.
  if (this._connecting) {
    this._connectQueueSize += data.length;
    if (this._connectQueue) {
      this._connectQueue.push([data, encoding, cb]);
    } else {
      this._connectQueue = [[data, encoding, cb]];
    }
    return false;
  }

  return this._write(data, encoding, cb);
};


Socket.prototype._write = function(data, encoding, cb) {
  timers.active(this);

  if (!this._handle) {
    this._destroy(new Error('This socket is closed.'), cb);
    return false;
  }

  var writeReq;

  if (Buffer.isBuffer(data)) {
    writeReq = this._handle.writeBuffer(data);

  } else {
    switch (encoding) {
      case 'utf8':
      case 'utf-8':
        writeReq = this._handle.writeUtf8String(data);
        break;

      case 'ascii':
        writeReq = this._handle.writeAsciiString(data);
        break;

      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
        writeReq = this._handle.writeUcs2String(data);
        break;

      default:
        assert(0);
    }
  }

  if (!writeReq || typeof writeReq !== 'object') {
    this._destroy(errnoException(errno, 'write'), cb);
    return false;
  }

  writeReq.oncomplete = afterWrite;
  writeReq.cb = cb;

  this._pendingWriteReqs++;
  this._bytesDispatched += writeReq.bytes;

  return this._handle.writeQueueSize == 0;
};


Socket.prototype.__defineGetter__('bytesWritten', function() {
  var bytes = this._bytesDispatched,
      connectQueue = this._connectQueue;

  if (connectQueue) {
    connectQueue.forEach(function(el) {
      var data = el[0];
      if (Buffer.isBuffer(data)) {
        bytes += data.length;
      } else {
        bytes += Buffer.byteLength(data, el[1]);
      }
    }, this);
  }

  return bytes;
});


function afterWrite(status, handle, req) {
  var self = handle.owner;

  // callback may come after call to destroy.
  if (self.destroyed) {
    return;
  }

  if (status) {
    self._destroy(errnoException(errno, 'write'), req.cb);
    return;
  }

  timers.active(self);

  self._pendingWriteReqs--;

  if (self._pendingWriteReqs == 0) {
    self.emit('drain');
  }

  if (req.cb) req.cb();

  if (self._pendingWriteReqs == 0 && self._flags & FLAG_DESTROY_SOON) {
    self._destroy();
  }
}


function connect(self, address, port, addressType, localAddress) {
  // TODO return promise from Socket.prototype.connect which
  // wraps _connectReq.

  assert.ok(self._connecting);

  if (localAddress) {
    var r;
    if (addressType == 6) {
      r = self._handle.bind6(localAddress);
    } else {
      r = self._handle.bind(localAddress);
    }

    if (r) {
      self._destroy(errnoException(errno, 'bind'));
      return;
    }
  }

  var connectReq;
  if (addressType == 6) {
    connectReq = self._handle.connect6(address, port);
  } else if (addressType == 4) {
    connectReq = self._handle.connect(address, port);
  } else {
    connectReq = self._handle.connect(address, afterConnect);
  }

  if (connectReq !== null) {
    connectReq.oncomplete = afterConnect;
  } else {
    self._destroy(errnoException(errno, 'connect'));
  }
}


Socket.prototype.connect = function(options, cb) {
  if (typeof options !== 'object') {
    // Old API:
    // connect(port, [host], [cb])
    // connect(path, [cb]);
    var args = normalizeConnectArgs(arguments);
    return Socket.prototype.connect.apply(this, args);
  }

  var self = this;
  var pipe = !!options.path;

  if (this.destroyed || !this._handle) {
    this._handle = pipe ? createPipe() : createTCP();
    initSocketHandle(this);
  }

  if (typeof cb === 'function') {
    self.on('connect', cb);
  }

  timers.active(this);

  self._connecting = true;
  self.writable = true;

  if (pipe) {
    connect(self, options.path);

  } else if (!options.host) {
    debug('connect: missing host');
    connect(self, '127.0.0.1', options.port, 4);

  } else {
    var host = options.host;
    debug('connect: find host ' + host);
    require('dns').lookup(host, function(err, ip, addressType) {
      // It's possible we were destroyed while looking this up.
      // XXX it would be great if we could cancel the promise returned by
      // the look up.
      if (!self._connecting) return;

      if (err) {
        // net.createConnection() creates a net.Socket object and
        // immediately calls net.Socket.connect() on it (that's us).
        // There are no event listeners registered yet so defer the
        // error event to the next tick.
        process.nextTick(function() {
          self.emit('error', err);
          self._destroy();
        });
      } else {
        timers.active(self);

        addressType = addressType || 4;

        // node_net.cc handles null host names graciously but user land
        // expects remoteAddress to have a meaningful value
        ip = ip || (addressType === 4 ? '127.0.0.1' : '0:0:0:0:0:0:0:1');

        connect(self, ip, options.port, addressType, options.localAddress);
      }
    });
  }
  return self;
};


Socket.prototype.ref = function() {
  if (this._handle)
    this._handle.ref();
};


Socket.prototype.unref = function() {
  if (this._handle)
    this._handle.unref();
};



function afterConnect(status, handle, req, readable, writable) {
  var self = handle.owner;

  // callback may come after call to destroy
  if (self.destroyed) {
    return;
  }

  assert.equal(handle, self._handle);

  debug('afterConnect');

  assert.ok(self._connecting);
  self._connecting = false;

  // now that we're connected, process any pending pause state.
  if (self._paused) {
    self._paused = false;
    self.pause();
  }

  if (status == 0) {
    self.readable = readable;
    self.writable = writable;
    timers.active(self);

    if (self.readable) {
      handle.readStart();
    }

    if (self._connectQueue) {
      debug('Drain the connect queue');
      var connectQueue = self._connectQueue;
      for (var i = 0; i < connectQueue.length; i++) {
        self._write.apply(self, connectQueue[i]);
      }
      self._connectQueueCleanUp();
    }

    self.emit('connect');

    if (self._flags & FLAG_SHUTDOWN_QUEUED) {
      // end called before connected - call end now with no data
      self._flags &= ~FLAG_SHUTDOWN_QUEUED;
      self.end();
    }
  } else {
    self._connectQueueCleanUp();
    self._destroy(errnoException(errno, 'connect'));
  }
}


function errnoException(errorno, syscall) {
  // TODO make this more compatible with ErrnoException from src/node.cc
  // Once all of Node is using this function the ErrnoException from
  // src/node.cc should be removed.
  var e = new Error(syscall + ' ' + errorno);
  e.errno = e.code = errorno;
  e.syscall = syscall;
  return e;
}




function Server(/* [ options, ] listener */) {
  if (!(this instanceof Server)) return new Server(arguments[0], arguments[1]);
  events.EventEmitter.call(this);

  var self = this;

  var options;

  if (typeof arguments[0] == 'function') {
    options = {};
    self.on('connection', arguments[0]);
  } else {
    options = arguments[0] || {};

    if (typeof arguments[1] == 'function') {
      self.on('connection', arguments[1]);
    }
  }

  this._connections = 0;

  // when server is using slaves .connections is not reliable
  // so null will be return if thats the case
  Object.defineProperty(this, 'connections', {
    get: function() {
      if (self._usingSlaves) {
        return null;
      }
      return self._connections;
    },
    set: function(val) {
      return (self._connections = val);
    },
    configurable: true, enumerable: true
  });

  this.allowHalfOpen = options.allowHalfOpen || false;

  this._handle = null;
}
util.inherits(Server, events.EventEmitter);
exports.Server = Server;


function toNumber(x) { return (x = Number(x)) >= 0 ? x : false; }


var createServerHandle = exports._createServerHandle =
    function(address, port, addressType, fd) {
  var r = 0;
  // assign handle in listen, and clean up if bind or listen fails
  var handle;

  if (typeof fd === 'number' && fd >= 0) {
    var tty_wrap = process.binding('tty_wrap');
    var type = tty_wrap.guessHandleType(fd);
    switch (type) {
      case 'PIPE':
        debug('listen pipe fd=' + fd);
        // create a PipeWrap
        handle = createPipe();
        handle.open(fd);
        handle.readable = true;
        handle.writable = true;
        break;

      default:
        // Not a fd we can listen on.  This will trigger an error.
        debug('listen invalid fd=' + fd + ' type=' + type);
        global.errno = 'EINVAL'; // hack, callers expect that errno is set
        handle = null;
        break;
    }
    return handle;

  } else if (port == -1 && addressType == -1) {
    handle = createPipe();
    if (process.platform === 'win32') {
      var instances = parseInt(process.env.NODE_PENDING_PIPE_INSTANCES);
      if (!isNaN(instances)) {
        handle.setPendingInstances(instances);
      }
    }
  } else {
    handle = createTCP();
  }

  if (address || port) {
    debug('bind to ' + address);
    if (addressType == 6) {
      r = handle.bind6(address, port);
    } else {
      r = handle.bind(address, port);
    }
  }

  if (r) {
    handle.close();
    handle = null;
  }

  return handle;
};


Server.prototype._listen2 = function(address, port, addressType, backlog, fd) {
  var self = this;
  var r = 0;

  // If there is not yet a handle, we need to create one and bind.
  // In the case of a server sent via IPC, we don't need to do this.
  if (!self._handle) {
    self._handle = createServerHandle(address, port, addressType, fd);
    if (!self._handle) {
      process.nextTick(function() {
        self.emit('error', errnoException(errno, 'listen'));
      });
      return;
    }
  }

  self._handle.onconnection = onconnection;
  self._handle.owner = self;

  // Use a backlog of 512 entries. We pass 511 to the listen() call because
  // the kernel does: backlogsize = roundup_pow_of_two(backlogsize + 1);
  // which will thus give us a backlog of 512 entries.
  r = self._handle.listen(backlog || 511);

  if (r) {
    var ex = errnoException(errno, 'listen');
    self._handle.close();
    self._handle = null;
    process.nextTick(function() {
      self.emit('error', ex);
    });
    return;
  }

  // generate connection key, this should be unique to the connection
  this._connectionKey = addressType + ':' + address + ':' + port;

  process.nextTick(function() {
    self.emit('listening');
  });
};


function listen(self, address, port, addressType, backlog, fd) {
  if (!cluster) cluster = require('cluster');

  if (cluster.isWorker) {
    cluster._getServer(self, address, port, addressType, fd, function(handle) {
      self._handle = handle;
      self._listen2(address, port, addressType, backlog, fd);
    });
  } else {
    self._listen2(address, port, addressType, backlog, fd);
  }
}


Server.prototype.listen = function() {
  var self = this;

  var lastArg = arguments[arguments.length - 1];
  if (typeof lastArg == 'function') {
    self.once('listening', lastArg);
  }

  var port = toNumber(arguments[0]);

  // The third optional argument is the backlog size.
  // When the ip is omitted it can be the second argument.
  var backlog = toNumber(arguments[1]) || toNumber(arguments[2]);

  var TCP = process.binding('tcp_wrap').TCP;

  if (arguments.length == 0 || typeof arguments[0] == 'function') {
    // Don't bind(). OS will assign a port with INADDR_ANY.
    // The port can be found with server.address()
    listen(self, null, null, backlog);

  } else if (arguments[0] && typeof arguments[0] === 'object') {
    var h = arguments[0];
    if (h._handle) {
      h = h._handle;
    } else if (h.handle) {
      h = h.handle;
    }
    if (h instanceof TCP) {
      self._handle = h;
      listen(self, null, -1, -1, backlog);
    } else if (h.fd && typeof h.fd === 'number' && h.fd >= 0) {
      listen(self, null, null, null, backlog, h.fd);
    } else {
      throw new Error('Invalid listen argument: ' + h);
    }
  } else if (isPipeName(arguments[0])) {
    // UNIX socket or Windows pipe.
    var pipeName = self._pipeName = arguments[0];
    listen(self, pipeName, -1, -1, backlog);

  } else if (typeof arguments[1] == 'undefined' ||
             typeof arguments[1] == 'function' ||
             typeof arguments[1] == 'number') {
    // The first argument is the port, no IP given.
    listen(self, '0.0.0.0', port, 4, backlog);

  } else {
    // The first argument is the port, the second an IP.
    require('dns').lookup(arguments[1], function(err, ip, addressType) {
      if (err) {
        self.emit('error', err);
      } else {
        listen(self, ip || '0.0.0.0', port, ip ? addressType : 4, backlog);
      }
    });
  }
  return self;
};

Server.prototype.address = function() {
  if (this._handle && this._handle.getsockname) {
    return this._handle.getsockname();
  } else if (this._pipeName) {
    return this._pipeName;
  } else {
    return null;
  }
};

function onconnection(clientHandle) {
  var handle = this;
  var self = handle.owner;

  debug('onconnection');

  if (!clientHandle) {
    self.emit('error', errnoException(errno, 'accept'));
    return;
  }

  if (self.maxConnections && self._connections >= self.maxConnections) {
    clientHandle.close();
    return;
  }

  var socket = new Socket({
    handle: clientHandle,
    allowHalfOpen: self.allowHalfOpen
  });
  socket.readable = socket.writable = true;

  socket.resume();

  self._connections++;
  socket.server = self;

  DTRACE_NET_SERVER_CONNECTION(socket);
  self.emit('connection', socket);
  socket.emit('connect');
}


Server.prototype.close = function(cb) {
  if (!this._handle) {
    // Throw error. Follows net_legacy behaviour.
    throw new Error('Not running');
  }

  if (cb) {
    this.once('close', cb);
  }
  this._handle.close();
  this._handle = null;
  this._emitCloseIfDrained();

  // fetch new socket lists
  if (this._usingSlaves) {
    this._slaves.forEach(function(socketList) {
      if (socketList.list.length === 0) return;
      socketList.update();
    });
  }

  return this;
};

Server.prototype._emitCloseIfDrained = function() {
  var self = this;

  if (self._handle || self._connections) return;

  process.nextTick(function() {
    self.emit('close');
  });
};


Server.prototype.listenFD = util.deprecate(function(fd, type) {
  return this.listen({ fd: fd });
}, 'listenFD is deprecated. Use listen({fd: <number>}).');

// when sending a socket using fork IPC this function is executed
Server.prototype._setupSlave = function(socketList) {
  if (!this._usingSlaves) {
    this._usingSlaves = true;
    this._slaves = [];
  }
  this._slaves.push(socketList);
};

Server.prototype.ref = function() {
  if (this._handle)
    this._handle.ref();
};

Server.prototype.unref = function() {
  if (this._handle)
    this._handle.unref();
};


// TODO: isIP should be moved to the DNS code. Putting it here now because
// this is what the legacy system did.
// NOTE: This does not accept IPv6 with an IPv4 dotted address at the end,
//  and it does not detect more than one double : in a string.
exports.isIP = function(input) {
  if (!input) {
    return 0;
  } else if (/^(\d?\d?\d)\.(\d?\d?\d)\.(\d?\d?\d)\.(\d?\d?\d)$/.test(input)) {
    var parts = input.split('.');
    for (var i = 0; i < parts.length; i++) {
      var part = parseInt(parts[i]);
      if (part < 0 || 255 < part) {
        return 0;
      }
    }
    return 4;
  } else if (/^::|^::1|^([a-fA-F0-9]{1,4}::?){1,7}([a-fA-F0-9]{1,4})$/.test(
      input)) {
    return 6;
  } else {
    return 0;
  }
};


exports.isIPv4 = function(input) {
  return exports.isIP(input) === 4;
};


exports.isIPv6 = function(input) {
  return exports.isIP(input) === 6;
};


if (process.platform === 'win32') {
  var simultaneousAccepts;

  exports._setSimultaneousAccepts = function(handle) {
    if (typeof handle === 'undefined') {
      return;
    }

    if (typeof simultaneousAccepts === 'undefined') {
      simultaneousAccepts = (process.env.NODE_MANY_ACCEPTS &&
                             process.env.NODE_MANY_ACCEPTS !== '0');
    }

    if (handle._simultaneousAccepts !== simultaneousAccepts) {
      handle.setSimultaneousAccepts(simultaneousAccepts);
      handle._simultaneousAccepts = simultaneousAccepts;
    }
  };
} else {
  exports._setSimultaneousAccepts = function(handle) {};
}
