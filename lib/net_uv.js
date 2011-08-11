var events = require('events');
var stream = require('stream');
var timers = require('timers');
var util = require('util');
var assert = require('assert');

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
var FLAG_GOT_EOF      = 1 << 0;
var FLAG_SHUTDOWN     = 1 << 1;
var FLAG_DESTROY_SOON = 1 << 2;
var FLAG_SHUTDOWNQUED = 1 << 3;


var debug;
if (process.env.NODE_DEBUG && /net/.test(process.env.NODE_DEBUG)) {
  debug = function(x) { console.error('NET:', x); };
} else {
  debug = function() { };
}


function isPipeName(s) {
  return typeof s === 'string' && toPort(s) === false;
}


exports.createServer = function() {
  return new Server(arguments[0], arguments[1]);
};


exports.connect = exports.createConnection = function(port /* [host], [cb] */) {
  var s;

  if (isPipeName(port)) {
    s = new Socket({ handle: createPipe() });
  } else {
    s = new Socket();
  }

  s.connect(port, arguments[1], arguments[2]);
  return s;
};

/* called when creating new Socket, or when re-using a closed Socket */
function initSocketHandle(self) {
  self._writeRequests = [];

  self._flags = 0;
  self._connectQueueSize = 0;
  self.destroyed = false;
  self.bytesRead = 0;
  self.bytesWritten = 0;

  // Handle creation may be deferred to bind() or connect() time.
  if (self._handle) {
    self._handle.socket = self;
    self._handle.onread = onread;

    if (self._handle.getsockname) {
      var sockname = self._handle.getsockname();
      self.remoteAddress = sockname.address;
      self.remotePort = sockname.port;
    }
    // also export sockname.family?
  }
}

function Socket(options) {
  if (!(this instanceof Socket)) return new Socket(options);

  stream.Stream.call(this);

  // private
  this._handle = options && options.handle;
  initSocketHandle(this);

  this.allowHalfOpen = options && options.allowHalfOpen;
}
util.inherits(Socket, stream.Stream);


exports.Socket = Socket;
exports.Stream = Socket; // Legacy naming.


Socket.prototype.listen = function() {
  var self = this;
  self.on('connection', arguments[0]);
  listen(self, null, null);
};


Socket.prototype.setTimeout = function(msecs, callback) {
  if (msecs > 0) {
    timers.enroll(this, msecs);
    timers.active(this);
    if (callback) {
      this.once('timeout', callback);
    }
  } else if (msecs === 0) {
    timers.unenroll(this);
  }
};


Socket.prototype._onTimeout = function() {
  this.emit('timeout');
};


Socket.prototype.setNoDelay = function() {
  /* TODO implement me */
};


Socket.prototype.setKeepAlive = function(setting, msecs) {
  /* TODO implement me */
};


Socket.prototype.address = function() {
  return this._handle.getsockname();
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
    return this._handle.writeQueueSize + this._connectQueueSize;
  }
});


Socket.prototype.pause = function() {
  this._handle.readStop();
};


Socket.prototype.resume = function() {
  if (this._handle) {
    this._handle.readStart();
  }
};


Socket.prototype.end = function(data, encoding) {
  if (this._connecting && ((this._flags & FLAG_SHUTDOWNQUED) == 0)) {
    // still connecting, add data to buffer
    if (data) this.write(data, encoding);
    this.writable = false;
    this._flags |= FLAG_SHUTDOWNQUED;
  }

  if (!this.writable) return;
  this.writable = false;

  if (data) this.write(data, encoding);
  DTRACE_NET_STREAM_END(this);

  if (this._flags & FLAG_GOT_EOF) {
    this.destroySoon();
  } else {
    this._flags |= FLAG_SHUTDOWN;
    var shutdownReq = this._handle.shutdown();

    if (!shutdownReq) {
      this.destroy(errnoException(errno, 'shutdown'));
      return false;
    }

    shutdownReq.oncomplete = afterShutdown;
  }

  return true;
};


function afterShutdown(status, handle, req) {
  var self = handle.socket;

  assert.ok(self._flags & FLAG_SHUTDOWN);
  assert.ok(!self.writable);

  // callback may come after call to destroy.
  if (self.destroyed) {
    return;
  }

  if (self._flags & FLAG_GOT_EOF || !self.readable) {
    self.destroy();
  } else {
  }
}


Socket.prototype.destroySoon = function() {
  this.writable = false;
  this._flags |= FLAG_DESTROY_SOON;

  if (this._writeRequests.length == 0) {
    this.destroy();
  }
};


Socket.prototype._connectQueueCleanUp = function(exception) {
  this._connecting = false;
  this._connectQueueSize = 0;
  this._connectQueue = null;
};


Socket.prototype.destroy = function(exception) {
  var self = this;

  self._connectQueueCleanUp();

  debug('destroy ' + this.fd);

  this.readable = this.writable = false;

  timers.unenroll(this);

  if (this.server && !this.destroyed) {
    this.server.connections--;
    this.server._emitCloseIfDrained();
  }

  debug('close ' + this.fd);
  if (this._handle) {
    this._handle.close();
    this._handle = null;
  }

  process.nextTick(function() {
    if (exception) self.emit('error', exception);
    self.emit('close', exception ? true : false);
  });

  this.destroyed = true;
};


function onread(buffer, offset, length) {
  var handle = this;
  var self = handle.socket;
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

  } else {
    // EOF
    self.readable = false;

    assert.ok(!(self._flags & FLAG_GOT_EOF));
    self._flags |= FLAG_GOT_EOF;

    // We call destroy() before end(). 'close' not emitted until nextTick so
    // the 'end' event will come first as required.
    if (!self.writable) self.destroy();

    if (!self.allowHalfOpen) self.end();
    if (self._events && self._events['end']) self.emit('end');
    if (self.onend) self.onend();
  }
}


Socket.prototype.setEncoding = function(encoding) {
  var StringDecoder = require('string_decoder').StringDecoder; // lazy load
  this._decoder = new StringDecoder(encoding);
};


Socket.prototype.write = function(data /* [encoding], [fd], [cb] */) {
  var encoding, fd, cb;

  // parse arguments
  if (typeof arguments[3] == 'function') {
    cb = arguments[3];
    fd = arguments[2];
    encoding = arguments[1];
  } else if (typeof arguments[2] == 'function') {
    cb = arguments[2];
    if (typeof arguments[1] == 'number') {
      fd = arguments[1];
    } else {
      encoding = arguments[1];
    }
  } else if (typeof arguments[1] == 'function') {
    cb = arguments[1];
  } else {
    if (typeof arguments[1] == 'number') {
      fd = arguments[1];
    } else {
      encoding = arguments[1];
      fd = arguments[2];
    }
  }

  this.bytesWritten += data.length;

  // Change strings to buffers. SLOW
  if (typeof data == 'string') {
    data = new Buffer(data, encoding);
  }

  // If we are still connecting, then buffer this for later.
  if (this._connecting) {
    this._connectQueueSize += data.length;
    if (this._connectQueue) {
      this._connectQueue.push([data, encoding, fd, cb]);
    } else {
      this._connectQueue = [ [data, encoding, fd, cb] ];
    }
    return false;
  }

  var writeReq = this._handle.write(data);

  if (!writeReq) {
    this.destroy(errnoException(errno, 'write'));
    return false;
  }

  writeReq.oncomplete = afterWrite;
  writeReq.cb = cb;
  this._writeRequests.push(writeReq);

  return this._handle.writeQueueSize == 0;
};


function afterWrite(status, handle, req, buffer) {
  var self = handle.socket;

  // callback may come after call to destroy.
  if (self.destroyed) {
    return;
  }
  // TODO check status.

  var req_ = self._writeRequests.shift();
  assert.equal(req, req_);

  if (self._writeRequests.length == 0) {
    // TODO remove all uses of ondrain - this is not a good hack.
    if (self.ondrain) self.ondrain();
    self.emit('drain');
  }

  if (req.cb) req.cb();

  if (self._writeRequests.length == 0  && self._flags & FLAG_DESTROY_SOON) {
    self.destroy();
  }
}


function connect(self, address, port, addressType) {
  if (port) {
    self.remotePort = port;
  }
  self.remoteAddress = address;

  // TODO return promise from Socket.prototype.connect which
  // wraps _connectReq.

  assert.ok(self._connecting);

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
    self.destroy(errnoException(errno, 'connect'));
  }
}


Socket.prototype.connect = function(port /* [host], [cb] */) {
  var self = this;

  var pipe = isPipeName(port);

  if (this.destroyed || !this._handle) {
    this._handle = pipe ? createPipe() : createTCP();
    initSocketHandle(this);
  }

  var host;
  if (typeof arguments[1] === 'function') {
    self.on('connect', arguments[1]);
  } else {
    host = arguments[1];
    if (typeof arguments[2] === 'function') {
      self.on('connect', arguments[2]);
    }
  }

  timers.active(this);

  self._connecting = true;
  self.writable = true;

  if (pipe) {
    connect(self, /*pipe_name=*/port);

  } else if (typeof host == 'string') {
    debug("connect: find host " + host);
    require('dns').lookup(host, function(err, ip, addressType) {
      if (err) {
        self.emit('error', err);
      } else {
        timers.active(self);

        addressType = addressType || 4;

        // node_net.cc handles null host names graciously but user land
        // expects remoteAddress to have a meaningful value
        ip = ip || (addressType === 4 ? '127.0.0.1' : '0:0:0:0:0:0:0:1');

        connect(self, ip, port, addressType);
      }
    });

  } else {
    debug("connect: missing host");
    connect(self, '127.0.0.1', port, 4);
  }
};


function afterConnect(status, handle, req) {
  var self = handle.socket;

  // callback may come after call to destroy
  if (self.destroyed) {
    return;
  }

  assert.equal(handle, self._handle);

  debug("afterConnect");

  assert.ok(self._connecting);
  self._connecting = false;

  if (status == 0) {
    self.readable = self.writable = true;
    timers.active(self);

    handle.readStart();

    self.emit('connect');

    if (self._connectQueue) {
      debug('Drain the connect queue');
      for (var i = 0; i < self._connectQueue.length; i++) {
        self.write.apply(self, self._connectQueue[i]);
      }
      self._connectQueueCleanUp();
    }

    if (self._flags & FLAG_SHUTDOWNQUED) {
      // end called before connected - call end now with no data
      self._flags &= ~FLAG_SHUTDOWNQUED;
      self.end();
    }
  } else {
    self._connectQueueCleanUp()
    self.destroy(errnoException(errno, 'connect'));
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

  this.connections = 0;
  this.allowHalfOpen = options.allowHalfOpen || false;

  this._handle = null;
}
util.inherits(Server, events.EventEmitter);
exports.Server = Server;


function toPort(x) { return (x = Number(x)) >= 0 ? x : false; }


function listen(self, address, port, addressType) {
  var r = 0;

  if (!self._handle) {
    // assign handle in listen, and clean up if bind or listen fails
    self._handle =
        (port == -1 && addressType == -1) ? createPipe() : createTCP();
  }

  self._handle.socket = self;
  self._handle.onconnection = onconnection;

  if (address || port) {
    debug("bind to " + address);
    if (addressType == 6) {
      r = self._handle.bind6(address, port);
    } else {
      r = self._handle.bind(address, port);
    }
  }

  if (r) {
    self._handle.close();
    self._handle = null;

    process.nextTick(function() {
      self.emit('error', errnoException(errno, 'listen'));
    });
  } else {
    r = self._handle.listen(self._backlog || 128);
    if (r) {
      self._handle.close();
      self._handle = null;

      process.nextTick(function() {
        self.emit('error', errnoException(errno, 'listen'));
      });
    } else {
      process.nextTick(function() {
        self.emit('listening');
      });
    }
  }
}

Server.prototype.listen = function() {
  var self = this;

  var lastArg = arguments[arguments.length - 1];
  if (typeof lastArg == 'function') {
    self.addListener('listening', lastArg);
  }

  var port = toPort(arguments[0]);

  if (arguments.length == 0 || typeof arguments[0] == 'function') {
    // Don't bind(). OS will assign a port with INADDR_ANY.
    // The port can be found with server.address()
    listen(self, null, null);

  } else if (isPipeName(arguments[0])) {
    // UNIX socket or Windows pipe.
    listen(self, arguments[0], -1, -1);

  } else if (typeof arguments[1] == 'undefined' ||
             typeof arguments[1] == 'function') {
    // The first argument is the port, no IP given.
    listen(self, '0.0.0.0', port, 4);

  } else {
    // The first argument is the port, the second an IP
    require('dns').lookup(arguments[1], function(err, ip, addressType) {
      if (err) {
        self.emit('error', err);
      } else {
        listen(self, ip || '0.0.0.0', port, ip ? addressType : 4);
      }
    });
  }
};

Server.prototype.address = function() {
  return this._handle.getsockname();
};

function onconnection(clientHandle) {
  var handle = this;
  var self = handle.socket;

  debug("onconnection");

  if (!clientHandle) {
    self.emit('error', errnoException(errno, 'accept'));
    return;
  }

  if (self.maxConnections && self.connections >= self.maxConnections) {
    clientHandle.close();
    return;
  }

  var socket = new Socket({
    handle: clientHandle,
    allowHalfOpen: self.allowHalfOpen
  });
  socket.readable = socket.writable = true;
  socket.resume();

  self.connections++;
  socket.server = self;

  DTRACE_NET_SERVER_CONNECTION(socket);
  self.emit('connection', socket);
  socket.emit('connect');
}


Server.prototype.close = function() {
  if (!this._handle) {
    // Throw error. Follows net_legacy behaviour.
    throw new Error('Not running');
  }

  this._handle.close();
  this._handle = null;
  this._emitCloseIfDrained();
};

Server.prototype._emitCloseIfDrained = function() {
  if (!this._handle && !this.connections) {
    this.emit('close');
  }
};

// TODO: implement for compatibility with 0.4.x? Requires libuv API change.
Server.prototype.listenFD = function(fd, type) {
  throw new Error('Not implemented');
};


// TODO: isIP should be moved to the DNS code. Putting it here now because
// this is what the legacy system did.
// NOTE: This does not accept IPv6 with an IPv4 dotted address at the end,
//  and it does not detect more than one double : in a string.
exports.isIP = function(input) {
  if (!input) {
    return 4;
  } else if (/^(\d?\d?\d)\.(\d?\d?\d)\.(\d?\d?\d)\.(\d?\d?\d)$/.test(input)) {
    var parts = input.split('.');
    for (var i = 0; i < parts.length; i++) {
      var part = parseInt(parts[i]);
      if (part < 0 || 255 < part) {
        return 0;
      }
    }
    return 4;
  } else if (/^::|^::1|^([a-fA-F0-9]{1,4}::?){1,7}([a-fA-F0-9]{1,4})$/.test(input)) {
    return 6;
  } else {
    return 0;
  }
}


exports.isIPv4 = function(input) {
  return exports.isIP(input) === 4;
};


exports.isIPv6 = function(input) {
  return exports.isIP(input) === 6;
};
