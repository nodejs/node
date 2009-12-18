var debugLevel = 0;
if ('NODE_DEBUG' in process.ENV) debugLevel = 1;
function debug (x) {
  if (debugLevel > 0) {
    process.stdio.writeError(x + '\n');
  }
}


var assert      = process.assert;
var socket      = process.socket;
var bind        = process.bind;
var connect     = process.connect;
var listen      = process.listen;
var accept      = process.accept;
var close       = process.close;
var shutdown    = process.shutdown;
var read        = process.read;
var write       = process.write;
var toRead      = process.toRead;
var socketError = process.socketError;
var getsockname = process.getsockname;
var getaddrinfo = process.getaddrinfo;
var needsLookup = process.needsLookup;
var EINPROGRESS = process.EINPROGRESS;


function Stream (peerInfo) {
  process.EventEmitter.call();

  var self = this;

  // Allocated on demand.
  self.recvBuffer = null;

  self.readWatcher = new process.IOWatcher(function () {
    // If this is the first recv (recvBuffer doesn't exist) or we've used up
    // most of the recvBuffer, allocate a new one.
    if (!self.recvBuffer || 
        self.recvBuffer.length - self.recvBuffer.used < 128) {
      self._allocateNewRecvBuf();
    }

    debug('recvBuffer.used ' + self.recvBuffer.used);
    var bytesRead = read(self.fd,
                         self.recvBuffer,
                         self.recvBuffer.used,
                         self.recvBuffer.length - self.recvBuffer.used);
    debug('bytesRead ' + bytesRead + '\n');

    if (bytesRead == 0) {
      self.readable = false;
      self.readWatcher.stop();
      self.emit('eof');
      if (!self.writable) self.forceClose();  
    } else {
      var slice = self.recvBuffer.slice(self.recvBuffer.used,
                                        self.recvBuffer.used + bytesRead);
      self.recvBuffer.used += bytesRead;
      self.emit('receive', slice);
    }
  });
  self.readable = false;

  self.sendQueue = [];    // queue of buffers that need to be written to socket
                          // XXX use link list? 
  self.sendQueueSize = 0; // in bytes, not to be confused with sendQueue.length!
  self._doFlush = function () {
    assert(self.sendQueueSize > 0);
    if (self.flush()) {
      assert(self.sendQueueSize == 0);
      self.emit("drain");
    }
  };
  self.writeWatcher = new process.IOWatcher(self._doFlush);
  self.writable = false;

  if (peerInfo) {
    self.fd = peerInfo.fd;
    self.remoteAddress = peerInfo.remoteAddress;
    self.remotePort = peerInfo.remotePort;

    self.readWatcher.set(self.fd, true, false);
    self.readWatcher.start();
    self.readable = true;

    self.writeWatcher.set(self.fd, false, true);
    self.writable = true;
  }
};
process.inherits(Stream, process.EventEmitter);
exports.Stream = Stream;


exports.createConnection = function (port, host) {
  var s = new Stream();
  s.connect(port, host);
  return s;
};


Stream.prototype._allocateNewRecvBuf = function () {
  var self = this;

  var newBufferSize = 1024; // TODO make this adjustable from user API

  if (toRead) {
    // Note: only Linux supports toRead().
    // Is the extra system call even worth it?
    var bytesToRead = toRead(self.fd);
    if (bytesToRead > 1024) {
      newBufferSize = 4*1024;
    } else if (bytesToRead == 0) {
      // Probably getting an EOF - so let's not allocate so much.
      if (self.recvBuffer &&
          self.recvBuffer.length - self.recvBuffer.used > 0) {
        return; // just recv the eof on the old buf.
      }
      newBufferSize = 128;
    }
  }

  self.recvBuffer = new process.Buffer(newBufferSize);
  self.recvBuffer.used = 0;
};


Stream.prototype._allocateSendBuffer = function () {
  var b = new process.Buffer(1024);
  b.used = 0;
  b.sent = 0;
  this.sendQueue.push(b);
  return b;
};


Stream.prototype._sendString = function (data, encoding) {
  var self = this;
  if (!self.writable) throw new Error('Stream is not writable');
  var buffer;
  if (self.sendQueue.length == 0) {
    buffer = self._allocateSendBuffer();
  } else {
    // walk through the sendQueue, find the buffer with free space
    for (var i = 0; i < self.sendQueue.length; i++) {
      if (self.sendQueue[i].used == 0) {
        buffer = self.sendQueue[i];
        break;
      }
    }
    // if we didn't find one, take the last
    if (!buffer) {
      buffer = self.sendQueue[self.sendQueue.length-1];
      // if last buffer is used up
      if (buffer.length == buffer.used) buffer = self._allocateSendBuffer();
    }
  }

  encoding = encoding || 'ascii'; // default to ascii since it's faster

  var charsWritten;
  var bytesWritten;

  if (encoding.toLowerCase() == 'utf8') {
    charsWritten = buffer.utf8Write(data,
                                    buffer.used,
                                    buffer.length - buffer.used);
    bytesWritten = process.Buffer.utf8Length(data.slice(0, charsWritten));
  } else {
    // ascii
    charsWritten = buffer.asciiWrite(data,
                                     buffer.used,
                                     buffer.length - buffer.used);
    bytesWritten = charsWritten;
  }
  
  buffer.used += bytesWritten;
  self.sendQueueSize += bytesWritten;

  debug('charsWritten ' + charsWritten);
  debug('buffer.used ' + buffer.used);

  // If we didn't finish, then recurse with the rest of the string.
  if (charsWritten < data.length) {
    debug('recursive send');
    self._sendString(data.slice(charsWritten), encoding);
  }
};


// Returns true if all the data was flushed to socket. Returns false if
// something was queued. If data was queued, then the "drain" event will
// signal when it has been finally flushed to socket.
Stream.prototype.send = function (data, encoding) {
  var self = this;
  if (!self.writable) throw new Error('Stream is not writable');
  if (typeof(data) == 'string') {
    self._sendString(data, encoding);
  } else {
    // data is a process.Buffer
    // walk through the sendQueue, find the first empty buffer
    var inserted = false;
    data.sent = 0;
    data.used = data.length;
    for (var i = 0; i < self.sendQueue.length; i++) {
      if (self.sendQueue[i].used == 0) {
        // if found, insert the data there
        self.sendQueue.splice(i, 0, data);
        inserted = true;
        break;
      }
    }

    if (!inserted) self.sendQueue.push(data);

    self.sendQueueSize += data.used;
  }
  return this.flush();
};


// Flushes the write buffer out. Emits "drain" if the buffer is empty.
Stream.prototype.flush = function () {
  var self = this;
  if (!self.writable) throw new Error('Stream is not writable');

  var bytesWritten;
  while (self.sendQueue.length > 0) {
    var b = self.sendQueue[0];

    if (b.sent == b.used) {
      // this can be improved - save the buffer for later?
      self.sendQueue.shift()
      continue;
    }

    bytesWritten = write(self.fd,
                         b,
                         b.sent,
                         b.used - b.sent);
    if (bytesWritten === null) {
      // could not flush everything
      self.writeWatcher.start();
      assert(self.sendQueueSize > 0);
      return false;
    }
    b.sent += bytesWritten;
    self.sendQueueSize -= bytesWritten;
    debug('bytes sent: ' + b.sent);
  }
  self.writeWatcher.stop();
  return true;
};


// var stream = new Stream();
// stream.connect(80)               - TCP connect to port 80 on the localhost
// stream.connect(80, 'nodejs.org') - TCP connect to port 80 on nodejs.org
// stream.connect('/tmp/socket')    - UNIX connect to socket specified by path
Stream.prototype.connect = function () {
  var self = this;
  if (self.fd) throw new Error('Stream already opened');

  if (typeof(arguments[0]) == 'string' && arguments.length == 1) {
    self.fd = process.socket('UNIX');
    // TODO check if sockfile exists?
  } else {
    self.fd = process.socket('TCP');
    // TODO dns resolution on arguments[1]
  }

  try {
    connect(self.fd, arguments[0], arguments[1]);
  } catch (e) {
    close(self.fd);
    throw e;
  }

  // Don't start the read watcher until connection is established
  self.readWatcher.set(self.fd, true, false);

  // How to connect on POSIX: Wait for fd to become writable, then call
  // socketError() if there isn't an error, we're connected. AFAIK this a
  // platform independent way determining when a non-blocking connection
  // is established, but I have only seen it documented in the Linux
  // Manual Page connect(2) under the error code EINPROGRESS.
  self.writeWatcher.set(self.fd, false, true);
  self.writeWatcher.start();
  self.writeWatcher.callback = function () {
    var errno = socketError(self.fd);
    if (errno == 0) {
      // connection established
      self.readWatcher.start();
      self.readable = true;
      self.writable = true;
      self.writeWatcher.callback = self._doFlush;
      self.emit('connect');
    } else if (errno != EINPROGRESS) {
      var e = new Error('connection error');
      e.errno = errno;
      self.forceClose(e);
    }
  };
};


Stream.prototype.forceClose = function (exception) {
  if (this.fd) {
    this.readable = false;
    this.writable = false;

    this.writeWatcher.stop();
    this.readWatcher.stop();
    close(this.fd);
    debug('close peer ' + this.fd);
    this.fd = null;
    this.emit('close', exception); 
  }
};


Stream.prototype._shutdown = function () {
  if (this.writable) {
    this.writable = false;
    shutdown(this.fd, "write");
  }
};


Stream.prototype.close = function () {
  var self = this;
  var closeMethod;
  if (self.readable && self.writable) {
    closeMethod = self._shutdown;
  } else if (!self.readable && self.writable) {
    // already got EOF
    closeMethod = self.forceClose;
  }
  // In the case we've already shutdown write side, 
  // but haven't got EOF: ignore. In the case we're
  // fully closed already: ignore.

  if (closeMethod) {
    if (self.sendQueueSize == 0) {
      // no queue. just shut down the socket.
      closeMethod();
    } else {
      self.addListener("drain", closeMethod);
    }
  }
};


function Server (listener) {
  var self = this;

  if (listener) {
    self.addListener('connection', listener);
  }

  self.watcher = new process.IOWatcher(function (readable, writeable) {
    while (self.fd) {
      var peerInfo = accept(self.fd);
      debug('accept: ' + JSON.stringify(peerInfo));
      if (!peerInfo) return;
      var peer = new Stream(peerInfo);
      self.emit('connection', peer);
    }
  });
};
process.inherits(Server, process.EventEmitter);
exports.Server = Server;


exports.createServer = function (listener) {
  return new Server(listener);
};


Server.prototype.listen = function () {
  var self = this;
  if (self.fd) throw new Error('Server already opened');

  if (typeof(arguments[0]) == 'string' && arguments.length == 1) {
    // the first argument specifies a path
    self.fd = process.socket('UNIX');
    self.type = 'UNIX';
    // TODO unlink sockfile if exists?
    // if (lstat(SOCKFILE, &tstat) == 0) {
    //   assert(S_ISSOCK(tstat.st_mode));
    //   unlink(SOCKFILE);
    // }
    bind(self.fd, arguments[0]);
  } else if (arguments.length == 0) {
    self.fd = process.socket('TCP');
    self.type = 'TCP';
    // Don't bind(). OS will assign a port with INADDR_ANY. The port will be
    // passed to the 'listening' event.
  } else {
    // the first argument is the port, the second an IP
    self.fd = process.socket('TCP');
    self.type = 'TCP';
    if (needsLookup(arguments[1])) {
      getaddrinfo(arguments[1], function (ip) {
      });
    }
    // TODO dns resolution on arguments[1]
    bind(self.fd, arguments[0], arguments[1]);
  }

  listen(self.fd, 128);
  self.emit("listening");

  self.watcher.set(self.fd, true, false); 
  self.watcher.start();
};


Server.prototype.sockName = function () {
  return getsockname(self.fd);
};


Server.prototype.close = function () {
  if (!this.fd) throw new Error('Not running');
  this.watcher.stop();
  close(this.fd);
  this.fd = null;
  this.emit("close");
};
