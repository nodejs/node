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
var stream = require('stream');
var timers = require('timers');

var kMinPoolSpace = 128;
var kPoolSize = 40 * 1024;

var debug;
if (process.env.NODE_DEBUG && /net/.test(process.env.NODE_DEBUG)) {
  debug = function(x) { console.error('NET:', x); };
} else {
  debug = function() { };
}


var binding = process.binding('net');

// Note about Buffer interface:
// I'm attempting to do the simplest possible interface to abstracting raw
// memory allocation. This might turn out to be too simple - it seems that
// I always use a buffer.used member to keep track of how much I've filled.
// Perhaps giving the Buffer a file-like interface with a head (which would
// represent buffer.used) that can be seeked around would be easier. I'm not
// yet convinced that every use-case can be fit into that abstraction, so
// waiting to implement it until I get more experience with this.
var FreeList = require('freelist').FreeList;

var IOWatcher = process.binding('io_watcher').IOWatcher;
var constants = process.binding('constants');
var assert = require('assert').ok;

var socket = binding.socket;
var bind = binding.bind;
var connect = binding.connect;
var listen = binding.listen;
var accept = binding.accept;
var close = binding.close;
var shutdown = binding.shutdown;
var read = binding.read;
var write = binding.write;
var toRead = binding.toRead;
var setNoDelay = binding.setNoDelay;
var setKeepAlive = binding.setKeepAlive;
var socketError = binding.socketError;
var getsockname = binding.getsockname;
var errnoException = binding.errnoException;
var sendMsg = binding.sendMsg;
var recvMsg = binding.recvMsg;

var EINPROGRESS = constants.EINPROGRESS || constants.WSAEINPROGRESS;
var ENOENT = constants.ENOENT;
var EMFILE = constants.EMFILE;

var END_OF_FILE = 42;


var ioWatchers = new FreeList('iowatcher', 100, function() {
  return new IOWatcher();
});

exports.isIP = binding.isIP;

exports.isIPv4 = function(input) {
  if (binding.isIP(input) === 4) {
    return true;
  }
  return false;
};

exports.isIPv6 = function(input) {
  if (binding.isIP(input) === 6) {
    return true;
  }
  return false;
};

// Allocated on demand.
var pool = null;
function allocNewPool() {
  pool = new Buffer(kPoolSize);
  pool.used = 0;
}

var emptyBuffer = null;
function allocEmptyBuffer() {
  emptyBuffer = new Buffer(1);
  emptyBuffer.sent = 0;
  emptyBuffer.length = 0;
}

function setImplmentationMethods(self) {
  function noData(buf, off, len) {
    return !buf ||
           (off != undefined && off >= buf.length) ||
           (len == 0);
  };

  if (self.type == 'unix') {
    self._writeImpl = function(buf, off, len, fd, flags) {
      // Detect and disallow zero-byte writes wth an attached file
      // descriptor. This is an implementation limitation of sendmsg(2).
      if (fd && noData(buf, off, len)) {
        throw new Error('File descriptors can only be written with data');
      }

      return sendMsg(self.fd, buf, off, len, fd, flags);
    };

    self._readImpl = function(buf, off, len) {
      var bytesRead = recvMsg(self.fd, buf, off, len);

      // Do not emit this in the same stack, otherwise we risk corrupting our
      // buffer pool which is full of read data, but has not had had its
      // pointers updated just yet.
      //
      // Save off recvMsg.fd in a closure so that, when we emit it later, we're
      // emitting the same value that we see now. Otherwise, we can end up
      // calling emit() after recvMsg() has been called again and end up
      // emitting null (or another FD).
      if (typeof recvMsg.fd === 'number') {
        var fd = recvMsg.fd;
        process.nextTick(function() {
          self.emit('fd', fd);
        });
      }

      return bytesRead;
    };
  } else {
    self._writeImpl = function(buf, off, len, fd, flags) {
      // XXX: TLS support requires that 0-byte writes get processed
      //      by the kernel for some reason. Otherwise, we'd just
      //      fast-path return here.

      // Drop 'fd' and 'flags' as these are not supported by the write(2)
      // system call
      return write(self.fd, buf, off, len);
    };

    self._readImpl = function(buf, off, len) {
      return read(self.fd, buf, off, len);
    };
  }

  self._shutdownImpl = function() {
    shutdown(self.fd, 'write');
  };

}


function onReadable(readable, writable) {
  assert(this.socket);
  var socket = this.socket;
  socket._onReadable();
}


function onWritable(readable, writable) {
  assert(this.socket);
  var socket = this.socket;
  if (socket._connecting) {
    assert(socket.writable);
    socket._onConnect();
  } else {
    socket._onWritable();
  }
}

function initSocket(self) {
  self._readWatcher = ioWatchers.alloc();
  self._readWatcher.socket = self;
  self._readWatcher.callback = onReadable;
  self.readable = self.destroyed = false;

  // Queue of buffers and string that need to be written to socket.
  self._writeQueue = [];
  self._writeQueueEncoding = [];
  self._writeQueueFD = [];
  self._writeQueueCallbacks = [];
  // Number of charactes (which approx. equals number of bytes)
  self.bufferSize = 0;

  self._writeWatcher = ioWatchers.alloc();
  self._writeWatcher.socket = self;
  self._writeWatcher.callback = onWritable;
  self.writable = false;
}

// Deprecated API: Socket(fd, type)
// New API: Socket({ fd: 10, type: 'unix', allowHalfOpen: true })
function Socket(options) {
  if (!(this instanceof Socket)) return new Socket(arguments[0], arguments[1]);
  stream.Stream.call(this);

  this.bufferSize = 0;
  this.fd = null;
  this.type = null;
  this.allowHalfOpen = false;

  if (typeof options == 'object') {
    this.fd = options.fd !== undefined ? parseInt(options.fd, 10) : null;
    this.type = options.type || null;
    this.allowHalfOpen = options.allowHalfOpen || false;
  } else if (typeof options == 'number') {
    this.fd = arguments[0];
    this.type = arguments[1] || null;
  }

  if (parseInt(this.fd, 10) >= 0) {
    this.open(this.fd, this.type);
  } else {
    setImplmentationMethods(this);
  }
}

util.inherits(Socket, stream.Stream);
exports.Socket = Socket;

// Legacy naming.
exports.Stream = Socket;

Socket.prototype._onTimeout = function() {
  this.emit('timeout');
};


Socket.prototype.open = function(fd, type) {
  initSocket(this);

  this.fd = fd;
  this.type = type || getFdType(fd)
  this.readable = true;

  setImplmentationMethods(this);

  this._writeWatcher.set(this.fd, false, true);
  this.writable = true;
};


exports.createConnection = function(port, host) {
  var s = new Socket();
  s.connect(port, host);
  return s;
};


Object.defineProperty(Socket.prototype, 'readyState', {
  get: function() {
    if (this._connecting) {
      return 'opening';
    } else if (this.readable && this.writable) {
      assert(typeof this.fd === 'number');
      return 'open';
    } else if (this.readable && !this.writable) {
      assert(typeof this.fd === 'number');
      return 'readOnly';
    } else if (!this.readable && this.writable) {
      assert(typeof this.fd === 'number');
      return 'writeOnly';
    } else {
      assert(typeof this.fd !== 'number');
      return 'closed';
    }
  }
});


// Returns true if all the data was flushed to socket. Returns false if
// something was queued. If data was queued, then the 'drain' event will
// signal when it has been finally flushed to socket.
Socket.prototype.write = function(data /* [encoding], [fd], [cb] */) {
  var encoding, fd, cb;

  assert(this.bufferSize >= 0);

  // parse arguments
  if (typeof arguments[1] == 'string') {
    encoding = arguments[1];
    if (typeof arguments[2] == 'number') {
      fd = arguments[2];
      cb = arguments[3];
    } else {
      cb = arguments[2];
    }
  } else if (typeof arguments[1] == 'number') {
    fd = arguments[1];
    cb = arguments[2];
  } else if (typeof arguments[2] == 'number') {
    // This case is to support old calls when the encoding argument
    // was not optional: s.write(buf, undefined, pipeFDs[1])
    encoding = arguments[1];
    fd = arguments[2];
    cb = arguments[3];
  } else {
    cb = arguments[1];
  }

  // TODO - actually use cb

  if (this._connecting || (this._writeQueue && this._writeQueue.length)) {
    if (!this._writeQueue) {
      this.bufferSize = 0;
      this._writeQueue = [];
      this._writeQueueEncoding = [];
      this._writeQueueFD = [];
      this._writeQueueCallbacks = [];
    }

    // Slow. There is already a write queue, so let's append to it.
    if (this._writeQueueLast() === END_OF_FILE) {
      throw new Error('Socket.end() called already; cannot write.');
    }

    var last = this._writeQueue.length - 1;

    this.bufferSize += data.length;

    if (typeof data == 'string' &&
        this._writeQueue.length &&
        typeof this._writeQueue[last] === 'string' &&
        this._writeQueueEncoding[last] === encoding) {
      // optimization - concat onto last
      this._writeQueue[last] += data;

      if (cb) {
        if (!this._writeQueueCallbacks[last]) {
          this._writeQueueCallbacks[last] = cb;
        } else {
          // awful
          this._writeQueueCallbacks[last] = function() {
            this._writeQueueCallbacks[last]();
            cb();
          };
        }
      }
    } else {
      this._writeQueue.push(data);
      this._writeQueueEncoding.push(encoding);
      this._writeQueueCallbacks.push(cb);
    }

    if (fd != undefined) {
      this._writeQueueFD.push(fd);
    }

    this._onBufferChange();
    DTRACE_NET_SOCKET_WRITE(this, 0);

    return false;
  } else {
    // Fast.
    // The most common case. There is no write queue. Just push the data
    // directly to the socket.
    return this._writeOut(data, encoding, fd, cb);
  }
};

// Directly writes the data to socket.
//
// Steps:
//   1. If it's a string, write it to the `pool`. (If not space remains
//      on the pool make a new one.)
//   2. Write data to socket. Return true if flushed.
//   3. Slice out remaining
//   4. Unshift remaining onto _writeQueue. Return false.
Socket.prototype._writeOut = function(data, encoding, fd, cb) {
  if (!this.writable) {
    throw new Error('Socket is not writable');
  }

  var buffer, off, len;
  var bytesWritten, charsWritten;
  var queuedData = false;

  if (typeof data != 'string') {
    // 'data' is a buffer, ignore 'encoding'
    buffer = data;
    off = 0;
    len = data.length;

  } else {
    assert(typeof data == 'string');

    if (!pool || pool.length - pool.used < kMinPoolSpace) {
      pool = null;
      allocNewPool();
    }

    if (!encoding || encoding == 'utf8' || encoding == 'utf-8') {
      // default to utf8
      bytesWritten = pool.write(data, 'utf8', pool.used);
      charsWritten = Buffer._charsWritten;
    } else {
      bytesWritten = pool.write(data, encoding, pool.used);
      charsWritten = bytesWritten;
    }

    if (encoding && data.length > 0) {
      assert(bytesWritten > 0);
    }

    buffer = pool;
    len = bytesWritten;
    off = pool.used;

    pool.used += bytesWritten;

    debug('wrote ' + bytesWritten + ' bytes to pool');

    if (charsWritten != data.length) {
      // debug('couldn't fit ' +
      //      (data.length - charsWritten) +
      //      ' bytes into the pool\n');
      // Unshift whatever didn't fit onto the buffer
      assert(data.length > charsWritten);
      this.bufferSize += data.length - charsWritten;
      this._writeQueue.unshift(data.slice(charsWritten));
      this._writeQueueEncoding.unshift(encoding);
      this._writeQueueCallbacks.unshift(cb);
      this._writeWatcher.start();
      this._onBufferChange();
      queuedData = true;
    }
  }

  try {
    bytesWritten = this._writeImpl(buffer, off, len, fd, 0);
    DTRACE_NET_SOCKET_WRITE(this, bytesWritten);
  } catch (e) {
    this.destroy(e);
    return false;
  }

  debug('wrote ' + bytesWritten + ' bytes to socket.')
  debug('[fd, off, len] = ' + JSON.stringify([this.fd, off, len]));

  timers.active(this);

  if (bytesWritten == len) {
    // awesome. sent to buffer.
    if (buffer === pool) {
      // If we're just writing from the pool then we can make a little
      // optimization and save the space.
      buffer.used -= len;
    }

    if (queuedData) {
      return false;
    } else {
      if (cb) cb();
      return true;
    }
  }

  // Didn't write the entire thing to buffer.
  // Need to wait for the socket to become available before trying again.
  this._writeWatcher.start();

  // Slice out the data left.
  var leftOver = buffer.slice(off + bytesWritten, off + len);
  leftOver.used = leftOver.length; // used the whole thing...

  //  util.error('data.used = ' + data.used);
  //if (!this._writeQueue) initWriteSocket(this);

  // data should be the next thing to write.
  this.bufferSize += leftOver.length;
  this._writeQueue.unshift(leftOver);
  this._writeQueueEncoding.unshift(null);
  this._writeQueueCallbacks.unshift(cb);
  this._onBufferChange();

  // If didn't successfully write any bytes, enqueue our fd and try again
  if (!bytesWritten) {
    this._writeQueueFD.unshift(fd);
  }

  return false;
};


Socket.prototype._onBufferChange = function() {
  // Put DTrace hooks here.
  ;
};


// Flushes the write buffer out.
// Returns true if the entire buffer was flushed.
Socket.prototype.flush = function() {
  while (this._writeQueue && this._writeQueue.length) {
    var data = this._writeQueue.shift();
    var encoding = this._writeQueueEncoding.shift();
    var cb = this._writeQueueCallbacks.shift();
    var fd = this._writeQueueFD.shift();

    if (data === END_OF_FILE) {
      this._shutdown();
      return true;
    }

    // Only decrement if it's not the END_OF_FILE object...
    this.bufferSize -= data.length;
    this._onBufferChange();

    var flushed = this._writeOut(data, encoding, fd, cb);
    if (!flushed) return false;
  }
  if (this._writeWatcher) this._writeWatcher.stop();
  return true;
};


Socket.prototype._writeQueueLast = function() {
  return this._writeQueue.length > 0 ?
      this._writeQueue[this._writeQueue.length - 1] : null;
};


Socket.prototype.setEncoding = function(encoding) {
  var StringDecoder = require('string_decoder').StringDecoder; // lazy load
  this._decoder = new StringDecoder(encoding);
};


function doConnect(socket, port, host) {
  if (socket.destroyed) return;

  timers.active(socket);

  try {
    connect(socket.fd, port, host);
  } catch (e) {
    socket.destroy(e);
    return;
  }

  debug('connecting to ' + host + ' : ' + port);

  // Don't start the read watcher until connection is established
  socket._readWatcher.set(socket.fd, true, false);

  // How to connect on POSIX: Wait for fd to become writable, then call
  // socketError() if there isn't an error, we're connected. AFAIK this a
  // platform independent way determining when a non-blocking connection
  // is established, but I have only seen it documented in the Linux
  // Manual Page connect(2) under the error code EINPROGRESS.
  socket._writeWatcher.set(socket.fd, false, true);
  socket._writeWatcher.start();
}


function toPort(x) { return (x = Number(x)) >= 0 ? x : false; }


Socket.prototype._onConnect = function() {
  var errno = socketError(this.fd);
  if (errno == 0) {
    // connection established
    this._connecting = false;
    this.resume();
    assert(this.writable);
    this.readable = this.writable = true;
    try {
      this.emit('connect');
    } catch (e) {
      this.destroy(e);
      return;
    }


    if (this._writeQueue && this._writeQueue.length) {
      // Flush this in case any writes are queued up while connecting.
      this._onWritable();
    }

  } else if (errno != EINPROGRESS) {
    this.destroy(errnoException(errno, 'connect'));
  }
};


Socket.prototype._onWritable = function() {
  // Socket becomes writable on connect() but don't flush if there's
  // nothing actually to write
  if (this.flush()) {
    if (this._events && this._events['drain']) this.emit('drain');
    if (this.ondrain) this.ondrain(); // Optimization
    if (this.__destroyOnDrain) this.destroy();
  }
};


Socket.prototype._onReadable = function() {
  var self = this;

  // If this is the first recv (pool doesn't exist) or we've used up
  // most of the pool, allocate a new one.
  if (!pool || pool.length - pool.used < kMinPoolSpace) {
    // discard the old pool. Can't add to the free list because
    // users might have refernces to slices on it.
    pool = null;
    allocNewPool();
  }

  //debug('pool.used ' + pool.used);
  var bytesRead;

  try {
    bytesRead = self._readImpl(pool,
                               pool.used,
                               pool.length - pool.used);
    DTRACE_NET_SOCKET_READ(this, bytesRead);
  } catch (e) {
    if (e.code == 'ECONNRESET') {
      self.destroy();
    } else {
      self.destroy(e);
    }
    return;
  }

  // Note that some _readImpl() implementations return -1 bytes
  // read as an indication not to do any processing on the result
  // (but not an error).

  if (bytesRead === 0) {
    self.readable = false;
    self._readWatcher.stop();

    if (!self.writable) self.destroy();
    // Note: 'close' not emitted until nextTick.

    if (!self.allowHalfOpen) self.end();
    if (self._events && self._events['end']) self.emit('end');
    if (self.onend) self.onend();
  } else if (bytesRead > 0) {

    timers.active(self);

    var start = pool.used;
    var end = pool.used + bytesRead;
    pool.used += bytesRead;

    debug('socket ' + self.fd + ' received ' + bytesRead + ' bytes');

    if (self._decoder) {
      // emit String
      var string = self._decoder.write(pool.slice(start, end));
      if (string.length) self.emit('data', string);
    } else {
      // emit buffer
      if (self._events && self._events['data']) {
        // emit a slice
        self.emit('data', pool.slice(start, end));
      }
    }

    // Optimization: emit the original buffer with end points
    if (self.ondata) self.ondata(pool, start, end);
  }
};


// var socket = new Socket();
// socket.connect(80)               - TCP connect to port 80 on the localhost
// socket.connect(80, 'nodejs.org') - TCP connect to port 80 on nodejs.org
// socket.connect('/tmp/socket')    - UNIX connect to socket specified by path
Socket.prototype.connect = function() {
  var self = this;
  initSocket(self);
  if (typeof self.fd === 'number') throw new Error('Socket already opened');
  if (!self._readWatcher) throw new Error('No readWatcher');

  timers.active(this);

  self._connecting = true; // set false in doConnect
  self.writable = true;

  var lastArg = arguments[arguments.length - 1];
  if (typeof lastArg == 'function') {
    self.addListener('connect', lastArg);
  }

  var port = toPort(arguments[0]);
  if (port === false) {
    // UNIX
    self.fd = socket('unix');
    self.type = 'unix';

    setImplmentationMethods(this);
    doConnect(self, arguments[0]);
  } else {
    // TCP
    require('dns').lookup(arguments[1], function(err, ip, addressType) {
      if (err) {
        self.emit('error', err);
      } else {
        timers.active(self);
        self.type = addressType == 4 ? 'tcp4' : 'tcp6';
        self.fd = socket(self.type);
        self.remoteAddress = ip;
        self.remotePort = port;
        doConnect(self, port, ip);
      }
    });
  }
};


Socket.prototype.address = function() {
  return getsockname(this.fd);
};


Socket.prototype.setNoDelay = function(v) {
  if ((this.type == 'tcp4') || (this.type == 'tcp6')) {
    setNoDelay(this.fd, v);
  }
};

Socket.prototype.setKeepAlive = function(enable, time) {
  if ((this.type == 'tcp4') || (this.type == 'tcp6')) {
    var secondDelay = Math.ceil(time / 1000);
    setKeepAlive(this.fd, enable, secondDelay);
  }
};

Socket.prototype.setTimeout = function(msecs, callback) {
  if (msecs > 0) {
    timers.enroll(this, msecs);
    if (typeof this.fd === 'number') { timers.active(this); }
    if (callback) {
      this.once('timeout', callback);
    }
  } else if (msecs === 0) {
    timers.unenroll(this);
  }
};


Socket.prototype.pause = function() {
  if (this._readWatcher) this._readWatcher.stop();
};


Socket.prototype.resume = function() {
  if (typeof this.fd !== 'number') {
    throw new Error('Cannot resume() closed Socket.');
  }
  if (this._readWatcher) {
    this._readWatcher.stop();
    this._readWatcher.set(this.fd, true, false);
    this._readWatcher.start();
  }
};

Socket.prototype.destroySoon = function() {
  if (this.flush()) {
    this.destroy();
  } else {
    this.__destroyOnDrain = true;
  }
};

Socket.prototype.destroy = function(exception) {
  // pool is shared between sockets, so don't need to free it here.
  var self = this;

  debug('destroy ' + this.fd);

  // TODO would like to set _writeQueue to null to avoid extra object alloc,
  // but lots of code assumes this._writeQueue is always an array.
  assert(this.bufferSize >= 0);
  this._writeQueue = [];
  this._writeQueueEncoding = [];
  this._writeQueueCallbacks = [];
  this._writeQueueFD = [];
  this.bufferSize = 0;

  this.readable = this.writable = false;

  if (this._writeWatcher) {
    this._writeWatcher.stop();
    this._writeWatcher.socket = null;
    ioWatchers.free(this._writeWatcher);
    this._writeWatcher = null;
  }

  if (this._readWatcher) {
    this._readWatcher.stop();
    this._readWatcher.socket = null;
    ioWatchers.free(this._readWatcher);
    this._readWatcher = null;
  }

  timers.unenroll(this);

  if (this.server && !this.destroyed) {
    this.server.connections--;
  }

  // FIXME Bug when this.fd == 0
  if (typeof this.fd === 'number') {
    debug('close ' + this.fd);
    close(this.fd);
    this.fd = null;
    process.nextTick(function() {
      if (exception) self.emit('error', exception);
      self.emit('close', exception ? true : false);
    });
  }

  this.destroyed = true;
};


Socket.prototype._shutdown = function() {
  if (!this.writable) {
    throw new Error('The connection is not writable');
  } else {
    // readable and writable
    this.writable = false;

    if (this.readable) {

      try {
        this._shutdownImpl();
      } catch (e) {
        if (e.code == 'ENOTCONN') {
          // Allowed.
          this.destroy();
        } else {
          this.destroy(e);
        }
      }
    } else {
      // writable but not readable
      this.destroy();
    }
  }
};


Socket.prototype.end = function(data, encoding) {
  if (this.writable) {
    if (this._writeQueueLast() !== END_OF_FILE) {
      DTRACE_NET_STREAM_END(this);
      if (data) this.write(data, encoding);
      this._writeQueue.push(END_OF_FILE);
      if (!this._connecting) {
        this.flush();
      }
    }
  }
};


function Server(/* [ options, ] listener */) {
  if (!(this instanceof Server)) return new Server(arguments[0], arguments[1]);
  events.EventEmitter.call(this);
  var self = this;

  var options = {};
  if (typeof arguments[0] == 'object') {
    options = arguments[0];
  }

  // listener: find the last argument that is a function
  for (var l = arguments.length - 1; l >= 0; l--) {
    if (typeof arguments[l] == 'function') {
      self.addListener('connection', arguments[l]);
    }
    if (arguments[l] !== undefined) break;
  }

  self.connections = 0;

  self.allowHalfOpen = options.allowHalfOpen || false;

  self.watcher = new IOWatcher();
  self.watcher.host = self;
  self.watcher.callback = function() {
    // Just in case we don't have a dummy fd.
    getDummyFD();

    if (self._pauseTimer) {
      // Somehow the watcher got started again. Need to wait until
      // the timer finishes.
      self.watcher.stop();
    }

    while (typeof self.fd === 'number') {
      try {
        var peerInfo = accept(self.fd);
      } catch (e) {
        if (e.errno != EMFILE) throw e;

        // Gracefully reject pending clients by freeing up a file
        // descriptor.
        rescueEMFILE(function() {
          self._rejectPending();
        });
        return;
      }
      if (!peerInfo) return;

      if (self.maxConnections && self.connections >= self.maxConnections) {
        // Close the connection we just had
        close(peerInfo.fd);
        // Reject all other pending connectins.
        self._rejectPending();
        return;
      }

      self.connections++;

      var options = { fd: peerInfo.fd,
                      type: self.type,
                      allowHalfOpen: self.allowHalfOpen };
      var s = new Socket(options);
      s.remoteAddress = peerInfo.address;
      s.remotePort = peerInfo.port;
      s.type = self.type;
      s.server = self;
      s.resume();

      DTRACE_NET_SERVER_CONNECTION(s);
      self.emit('connection', s);
    }
  };
}
util.inherits(Server, events.EventEmitter);
exports.Server = Server;


exports.createServer = function() {
  return new Server(arguments[0], arguments[1]);
};


// Just stop trying to accepting connections for a while.
// Useful for throttling against DoS attacks.
Server.prototype.pause = function(msecs) {
  // We're already paused.
  if (this._pauseTimer) return;

  var self = this;
  msecs = msecs || 1000;

  this.watcher.stop();

  // Wait a second before accepting more.
  this._pauseTimer = setTimeout(function() {
    // Our fd should still be there. If someone calls server.close() then
    // the pauseTimer should be cleared.
    assert(parseInt(self.fd) >= 0);
    self._pauseTimer = null;
    self.watcher.start();
  }, msecs);
};


Server.prototype._rejectPending = function() {
  var self = this;
  var acceptCount = 0;
  // Accept and close the waiting clients one at a time.
  // Single threaded programming ftw.
  while (true) {
    var peerInfo = accept(this.fd);
    if (!peerInfo) return;
    close(peerInfo.fd);

    // Don't become DoS'd by incoming requests
    if (++acceptCount > 50) {
      this.pause();
      return;
    }
  }
};


// Listen on a UNIX socket
// server.listen('/tmp/socket');
//
// Listen on port 8000, accept connections from INADDR_ANY.
// server.listen(8000);
//
// Listen on port 8000, accept connections to '192.168.1.2'
// server.listen(8000, '192.168.1.2');
Server.prototype.listen = function() {
  var self = this;
  if (typeof self.fd === 'number') throw new Error('Server already opened');

  var lastArg = arguments[arguments.length - 1];
  if (typeof lastArg == 'function') {
    self.addListener('listening', lastArg);
  }

  var port = toPort(arguments[0]);

  if (arguments.length == 0 || typeof arguments[0] == 'function') {
    // Don't bind(). OS will assign a port with INADDR_ANY.
    // The port can be found with server.address()
    self.type = 'tcp4';
    self.fd = socket(self.type);
    self._doListen(port);
  } else if (port === false) {
    // the first argument specifies a path
    self.fd = socket('unix');
    self.type = 'unix';
    var path = arguments[0];
    self.path = path;
    // unlink sockfile if it exists
    require('fs').stat(path, function(err, r) {
      if (err) {
        if (err.errno == ENOENT) {
          self._doListen(path);
        } else {
          throw r;
        }
      } else {
        if (!r.isSocket()) {
          throw new Error('Non-socket exists at  ' + path);
        } else {
          require('fs').unlink(path, function(err) {
            if (err) throw err;
            self._doListen(path);
          });
        }
      }
    });
  } else {
    // the first argument is the port, the second an IP
    require('dns').lookup(arguments[1], function(err, ip, addressType) {
      if (err) {
        self.emit('error', err);
      } else {
        self.type = addressType == 4 ? 'tcp4' : 'tcp6';
        self.fd = socket(self.type);
        self._doListen(port, ip);
      }
    });
  }
};

Server.prototype.listenFD = function(fd, type) {
  if (typeof this.fd === 'number') {
    throw new Error('Server already opened');
  }

  this.fd = fd;
  this.type = type || null;
  this._startWatcher();
};

Server.prototype._startWatcher = function() {
  this.watcher.set(this.fd, true, false);
  this.watcher.start();
  this.emit('listening');
};

Server.prototype._doListen = function() {
  var self = this;

  // Ensure we have a dummy fd for EMFILE conditions.
  getDummyFD();

  try {
    bind(self.fd, arguments[0], arguments[1]);
  } catch (err) {
    self.close();
    self.emit('error', err);
    return;
  }

  // Need to the listening in the nextTick so that people potentially have
  // time to register 'listening' listeners.
  process.nextTick(function() {
    // It could be that server.close() was called between the time the
    // original listen command was issued and this. Bail if that's the case.
    // See test/simple/test-net-eaddrinuse.js
    if (typeof self.fd !== 'number') return;

    try {
      listen(self.fd, self._backlog || 128);
    } catch (err) {
      self.close();
      self.emit('error', err);
      return;
    }

    self._startWatcher();
  });
};


Server.prototype.address = function() {
  return getsockname(this.fd);
};


Server.prototype.close = function() {
  var self = this;
  if (typeof self.fd !== 'number') throw new Error('Not running');

  self.watcher.stop();

  close(self.fd);
  self.fd = null;

  if (self._pauseTimer) {
    clearTimeout(self._pauseTimer);
    self._pauseTimer = null;
  }

  if (self.type === 'unix') {
    require('fs').unlink(self.path, function() {
      self.emit('close');
    });
  } else {
    self.emit('close');
  }
};

function getFdType(fd) {
  var type = 'file';
  var family;
  try {
    type = binding.getsocktype(fd);
    family = binding.getsockfamily(fd);
    if (family == "AF_UNIX") {
      type = "unix";
    } else if (type == "SOCK_STREAM" && family == "AF_INET") {
      type = "tcp";
    } else if (type == "SOCK_STREAM" && family == "AF_INET6") {
      type = "tcp6";
    } else if (type == "SOCK_DGRAM" && family == "AF_INET") {
      type = "udp";
    } else if (type == "SOCK_DGRAM" && family == "AF_INET6") {
      type = "udp6";
    }
  } catch (e) {
    if (e.code == 'ENOTSOCK') {
      type = 'file';
    } else {
      throw e;
    }
  }
  return type;
}
exports.getFdType = getFdType;

var dummyFD = null;
var lastEMFILEWarning = 0;
// Ensures to have at least one free file-descriptor free.
// callback should only use 1 file descriptor and close it before end of call
function rescueEMFILE(callback) {
  // Output a warning, but only at most every 5 seconds.
  var now = new Date();
  if (now - lastEMFILEWarning > 5000) {
    console.error('(node) Hit max file limit. Increase "ulimit -n"');
    lastEMFILEWarning = now;
  }

  if (dummyFD) {
    close(dummyFD);
    dummyFD = null;
    callback();
    getDummyFD();
  }
}

function getDummyFD() {
  if (!dummyFD) {
    try {
      dummyFD = socket('tcp');
    } catch (e) {
      dummyFD = null;
    }
  }
}
