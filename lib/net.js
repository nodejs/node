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
var stream = require('stream');
var timers = require('timers');
var util = require('util');
var assert = require('assert');
var cares = process.binding('cares_wrap');
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


var debug;
if (process.env.NODE_DEBUG && /net/.test(process.env.NODE_DEBUG)) {
  var pid = process.pid;
  debug = function(x) {
    // if console is not set up yet, then skip this.
    if (!console.error)
      return;
    console.error('NET: %d', pid,
                  util.format.apply(util, arguments).slice(0, 500));
  };
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
exports._normalizeConnectArgs = normalizeConnectArgs;


// called when creating new Socket, or when re-using a closed Socket
function initSocketHandle(self) {
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

  this._connecting = false;
  this._handle = null;

  switch (typeof options) {
    case 'number':
      options = { fd: options }; // Legacy interface.
      break;
    case 'undefined':
      options = {};
      break;
  }

  stream.Duplex.call(this, options);

  if (options.handle) {
    // Initialize TCPWrap and PipeWrap
    process.binding('tcp_wrap');
    process.binding('pipe_wrap');

    this._handle = options.handle; // private
  } else if (typeof options.fd !== 'undefined') {
    this._handle = createPipe();
    this._handle.open(options.fd);
    this.readable = options.readable !== false;
    this.writable = options.writable !== false;
  } else {
    // these will be set once there is a connection
    this.readable = this.writable = false;
  }

  this.onend = null;

  // shut down the socket when we're finished with it.
  this.on('finish', onSocketFinish);
  this.on('_socketEnd', onSocketEnd);

  initSocketHandle(this);

  this._pendingWrite = null;

  // handle strings directly
  this._writableState.decodeStrings = false;

  // default to *not* allowing half open sockets
  this.allowHalfOpen = options && options.allowHalfOpen || false;

  // if we have a handle, then start the flow of data into the
  // buffer.  if not, then this will happen when we connect
  if (this._handle && options.readable !== false)
    this.read(0);
}
util.inherits(Socket, stream.Duplex);

// the user has called .end(), and all the bytes have been
// sent out to the other side.
// If allowHalfOpen is false, or if the readable side has
// ended already, then destroy.
// If allowHalfOpen is true, then we need to do a shutdown,
// so that only the writable side will be cleaned up.
function onSocketFinish() {
  debug('onSocketFinish');
  if (this._readableState.ended) {
    debug('oSF: ended, destroy', this._readableState);
    return this.destroy();
  }

  debug('oSF: not ended, call shutdown()');

  // otherwise, just shutdown, or destroy() if not possible
  if (!this._handle || !this._handle.shutdown)
    return this.destroy();

  var shutdownReq = this._handle.shutdown();

  if (!shutdownReq)
    return this._destroy(errnoException(errno, 'shutdown'));

  shutdownReq.oncomplete = afterShutdown;
}


function afterShutdown(status, handle, req) {
  var self = handle.owner;

  debug('afterShutdown destroyed=%j', self.destroyed,
        self._readableState);

  // callback may come after call to destroy.
  if (self.destroyed)
    return;

  if (self._readableState.ended) {
    debug('readableState ended, destroying');
    self.destroy();
  } else {
    self.once('_socketEnd', self.destroy);
  }
}

// the EOF has been received, and no more bytes are coming.
// if the writable side has ended already, then clean everything
// up.
function onSocketEnd() {
  // XXX Should not have to do as much crap in this function.
  // ended should already be true, since this is called *after*
  // the EOF errno and onread has returned null to the _read cb.
  debug('onSocketEnd', this._readableState);
  this._readableState.ended = true;
  if (this._readableState.endEmitted) {
    this.readable = false;
  } else {
    this.once('end', function() {
      this.readable = false;
    });
    this.read(0);
  }

  if (!this.allowHalfOpen)
    this.destroySoon();
}

exports.Socket = Socket;
exports.Stream = Socket; // Legacy naming.

Socket.prototype.read = function(n) {
  if (n === 0)
    return stream.Readable.prototype.read.call(this, n);

  this.read = stream.Readable.prototype.read;
  this._consuming = true;
  return this.read(n);
};


Socket.prototype.listen = function() {
  debug('socket.listen');
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
      return this._handle.writeQueueSize + this._writableState.length;
    }
  }
});


// Just call handle.readStart until we have enough in the buffer
Socket.prototype._read = function(n, callback) {
  debug('_read');
  if (this._connecting || !this._handle) {
    debug('_read wait for connection');
    this.once('connect', this._read.bind(this, n, callback));
    return;
  }

  assert(callback === this._readableState.onread);
  assert(this._readableState.reading = true);

  if (!this._handle.reading) {
    debug('Socket._read readStart');
    this._handle.reading = true;
    var r = this._handle.readStart();
    if (r)
      this._destroy(errnoException(errno, 'read'));
  } else {
    debug('readStart already has been called.');
  }
};


Socket.prototype.end = function(data, encoding) {
  stream.Duplex.prototype.end.call(this, data, encoding);
  this.writable = false;
  DTRACE_NET_STREAM_END(this);

  // just in case we're waiting for an EOF.
  if (!this._readableState.endEmitted)
    this.read(0);
  return;
};


Socket.prototype.destroySoon = function() {
  if (this.writable)
    this.end();

  if (this._writableState.finishing || this._writableState.finished)
    this.destroy();
  else
    this.once('finish', this.destroy);
};


Socket.prototype._destroy = function(exception, cb) {
  debug('destroy');

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
    debug('already destroyed, fire error callbacks');
    fireErrorCallbacks();
    return;
  }

  self._connecting = false;

  this.readable = this.writable = false;

  timers.unenroll(this);

  debug('close');
  if (this._handle) {
    if (this !== process.stderr)
      debug('close handle');
    this._handle.close();
    this._handle.onread = noop;
    this._handle = null;
  }

  fireErrorCallbacks();

  process.nextTick(function() {
    debug('emit close');
    self.emit('close', exception ? true : false);
  });

  this.destroyed = true;

  if (this.server) {
    COUNTER_NET_SERVER_CONNECTION_CLOSE(this);
    debug('has server');
    this.server._connections--;
    if (this.server._emitCloseIfDrained) {
      this.server._emitCloseIfDrained();
    }
  }
};


Socket.prototype.destroy = function(exception) {
  debug('destroy', exception);
  this._destroy(exception);
};


// This function is called whenever the handle gets a
// buffer, or when there's an error reading.
function onread(buffer, offset, length) {
  var handle = this;
  var self = handle.owner;
  assert(handle === self._handle, 'handle != self._handle');

  timers.active(self);

  var end = offset + length;
  debug('onread', global.errno, offset, length, end);

  if (buffer) {
    debug('got data');

    // read success.
    // In theory (and in practice) calling readStop right now
    // will prevent this from being called again until _read() gets
    // called again.

    // if we didn't get any bytes, that doesn't necessarily mean EOF.
    // wait for the next one.
    if (offset === end) {
      debug('not any data, keep waiting');
      return;
    }

    // if it's not enough data, we'll just call handle.readStart()
    // again right away.
    self.bytesRead += length;

    // Optimization: emit the original buffer with end points
    var ret = true;
    if (self.ondata) self.ondata(buffer, offset, end);
    else ret = self.push(buffer.slice(offset, end));

    if (handle.reading && !ret) {
      handle.reading = false;
      debug('readStop');
      var r = handle.readStop();
      if (r)
        self._destroy(errnoException(errno, 'read'));
    }

  } else if (errno == 'EOF') {
    debug('EOF');

    if (self._readableState.length === 0)
      self.readable = false;

    if (self.onend) self.once('end', self.onend);

    // send a null to the _read cb to signal the end of data.
    self.push(null);

    // internal end event so that we know that the actual socket
    // is no longer readable, and we can start the shutdown
    // procedure. No need to wait for all the data to be consumed.
    self.emit('_socketEnd');
  } else {
    debug('error', errno);
    // Error
    if (errno == 'ECONNRESET') {
      self._destroy();
    } else {
      self._destroy(errnoException(errno, 'read'));
    }
  }
}


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


Socket.prototype._getsockname = function() {
  if (!this._handle || !this._handle.getsockname) {
    return {};
  }
  if (!this._sockname) {
    this._sockname = this._handle.getsockname();
    if (this._sockname === null) {
      return {};
    }
  }
  return this._sockname;
};


Socket.prototype.__defineGetter__('localAddress', function() {
  return this._getsockname().address;
});


Socket.prototype.__defineGetter__('localPort', function() {
  return this._getsockname().port;
});


Socket.prototype.write = function(chunk, encoding, cb) {
  if (typeof chunk !== 'string' && !Buffer.isBuffer(chunk))
    throw new TypeError('invalid data');
  return stream.Duplex.prototype.write.apply(this, arguments);
};


Socket.prototype._write = function(dataEncoding, cb) {
  // assert(Array.isArray(dataEncoding));
  var data = dataEncoding[0];
  var encoding = dataEncoding[1] || 'utf8';

  // If we are still connecting, then buffer this for later.
  // The Writable logic will buffer up any more writes while
  // waiting for this one to be done.
  if (this._connecting) {
    this._pendingWrite = dataEncoding;
    this.once('connect', function() {
      this._write(dataEncoding, cb);
    });
    return;
  }
  this._pendingWrite = null;

  timers.active(this);

  if (!this._handle) {
    this._destroy(new Error('This socket is closed.'), cb);
    return false;
  }

  var enc = Buffer.isBuffer(data) ? 'buffer' : encoding;
  var writeReq = createWriteReq(this._handle, data, enc);

  if (!writeReq || typeof writeReq !== 'object')
    return this._destroy(errnoException(errno, 'write'), cb);

  writeReq.oncomplete = afterWrite;
  this._bytesDispatched += writeReq.bytes;

  // If it was entirely flushed, we can write some more right now.
  // However, if more is left in the queue, then wait until that clears.
  if (this._handle.writeQueueSize === 0)
    cb();
  else
    writeReq.cb = cb;
};

function createWriteReq(handle, data, encoding) {
  switch (encoding) {
    case 'buffer':
      return handle.writeBuffer(data);

    case 'utf8':
    case 'utf-8':
      return handle.writeUtf8String(data);

    case 'ascii':
      return handle.writeAsciiString(data);

    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      return handle.writeUcs2String(data);

    default:
      return handle.writeBuffer(new Buffer(data, encoding));
  }
}


Socket.prototype.__defineGetter__('bytesWritten', function() {
  var bytes = this._bytesDispatched,
      state = this._writableState,
      pending = this._pendingWrite;

  state.buffer.forEach(function(el) {
    el = el[0];
    bytes += Buffer.byteLength(el[0], el[1]);
  });

  if (pending)
    bytes += Buffer.byteLength(pending[0], pending[1]);

  return bytes;
});


function afterWrite(status, handle, req) {
  var self = handle.owner;
  var state = self._writableState;
  if (self !== process.stderr && self !== process.stdout)
    debug('afterWrite', status, req);

  // callback may come after call to destroy.
  if (self.destroyed) {
    debug('afterWrite destroyed');
    return;
  }

  if (status) {
    debug('write failure', errnoException(errno, 'write'));
    self._destroy(errnoException(errno, 'write'), req.cb);
    return;
  }

  timers.active(self);

  if (self !== process.stderr && self !== process.stdout)
    debug('afterWrite call cb');

  if (req.cb)
    req.cb.call(self);
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

  if (this.destroyed) {
    this._readableState.reading = false;
    this._readableState.ended = false;
    this._writableState.ended = false;
    this._writableState.ending = false;
    this._writableState.finished = false;
    this._writableState.finishing = false;
    this.destroyed = false;
    this._handle = null;
  }

  var self = this;
  var pipe = !!options.path;

  if (!this._handle) {
    this._handle = pipe ? createPipe() : createTCP();
    initSocketHandle(this);
  }

  if (typeof cb === 'function') {
    self.once('connect', cb);
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

  assert(handle === self._handle, 'handle != self._handle');

  debug('afterConnect');

  assert.ok(self._connecting);
  self._connecting = false;

  if (status == 0) {
    self.readable = readable;
    self.writable = writable;
    timers.active(self);

    self.emit('connect');

    // start the first read, or get an immediate EOF.
    // this doesn't actually consume any bytes, because len=0.
    if (readable)
      self.read(0);

  } else {
    self._connecting = false;
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

  Object.defineProperty(this, 'connections', {
    get: util.deprecate(function() {

      if (self._usingSlaves) {
        return null;
      }
      return self._connections;
    }, 'connections property is deprecated. Use getConnections() method'),
    set: util.deprecate(function(val) {
      return (self._connections = val);
    }, 'connections property is deprecated. Use getConnections() method'),
    configurable: true, enumerable: true
  });

  this._handle = null;
  this._usingSlaves = false;
  this._slaves = [];

  this.allowHalfOpen = options.allowHalfOpen || false;
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
  debug('listen2', address, port, addressType, backlog);
  var self = this;
  var r = 0;

  // If there is not yet a handle, we need to create one and bind.
  // In the case of a server sent via IPC, we don't need to do this.
  if (!self._handle) {
    debug('_listen2: create a handle');
    self._handle = createServerHandle(address, port, addressType, fd);
    if (!self._handle) {
      var error = errnoException(errno, 'listen');
      process.nextTick(function() {
        self.emit('error', error);
      });
      return;
    }
  } else {
    debug('_listen2: have a handle already');
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
    // Bind to a random port.
    listen(self, '0.0.0.0', 0, null, backlog);

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
    } else if (typeof h.fd === 'number' && h.fd >= 0) {
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


  self._connections++;
  socket.server = self;

  DTRACE_NET_SERVER_CONNECTION(socket);
  COUNTER_NET_SERVER_CONNECTION(socket);
  self.emit('connection', socket);
  socket.emit('connect');
}


Server.prototype.getConnections = function(cb) {
  function end(err, connections) {
    process.nextTick(function() {
      cb(err, connections);
    });
  }

  if (!this._usingSlaves) {
    return end(null, this._connections);
  }

  // Poll slaves
  var left = this._slaves.length,
      total = this._connections;

  function oncount(err, count) {
    if (err) {
      left = -1;
      return end(err);
    }

    total += count;
    if (--left === 0) return end(null, total);
  }

  this._slaves.forEach(function(slave) {
    slave.getConnections(oncount);
  });
};


Server.prototype.close = function(cb) {
  function onSlaveClose() {
    if (--left !== 0) return;

    self._connections = 0;
    self._emitCloseIfDrained();
  }

  if (!this._handle) {
    // Throw error. Follows net_legacy behaviour.
    throw new Error('Not running');
  }

  if (cb) {
    this.once('close', cb);
  }
  this._handle.close();
  this._handle = null;

  if (this._usingSlaves) {
    var self = this,
        left = this._slaves.length;

    // Increment connections to be sure that, even if all sockets will be closed
    // during polling of slaves, `close` event will be emitted only once.
    this._connections++;

    // Poll slaves
    this._slaves.forEach(function(slave) {
      slave.close(onSlaveClose);
    });
  } else {
    this._emitCloseIfDrained();
  }

  return this;
};

Server.prototype._emitCloseIfDrained = function() {
  debug('SERVER _emitCloseIfDrained');
  var self = this;

  if (self._handle || self._connections) {
    debug('SERVER handle? %j   connections? %d',
          !!self._handle, self._connections);
    return;
  }

  process.nextTick(function() {
    debug('SERVER: emit close');
    self.emit('close');
  });
};


Server.prototype.listenFD = util.deprecate(function(fd, type) {
  return this.listen({ fd: fd });
}, 'listenFD is deprecated. Use listen({fd: <number>}).');

Server.prototype._setupSlave = function(socketList) {
  this._usingSlaves = true;
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
exports.isIP = cares.isIP;


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
