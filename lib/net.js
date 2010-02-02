var debugLevel = 0;
if ('NODE_DEBUG' in process.ENV) debugLevel = 1;
function debug (x) {
  if (debugLevel > 0) {
    process.stdio.writeError(x + '\n');
  }
}


var IOWatcher   = process.IOWatcher;
var assert      = process.assert;
var socket      = process.socket;
var bind        = process.bind;
var connect     = process.connect;
var listen      = process.listen;
var accept      = process.accept;
var close       = process.close;
var shutdown    = process.shutdown;
var read        = process.read;
var recvMsg     = process.recvMsg;
var sendFD      = process.sendFD;
var write       = process.write;
var toRead      = process.toRead;
var setNoDelay  = process.setNoDelay;
var socketError = process.socketError;
var getsockname = process.getsockname;
var getaddrinfo = process.getaddrinfo;
var needsLookup = process.needsLookup;
var EINPROGRESS = process.EINPROGRESS;
var ENOENT      = process.ENOENT;
var END_OF_FILE = 0;

// This is a free list to avoid creating so many of the same object.

function FreeList (name, max, constructor) {
  this.name = name;
  this.constructor = constructor;
  this.max = max;
  this.list = [];
}

FreeList.prototype.alloc = function () {
  debug("alloc " + this.name + " " + this.list.length);
  return this.list.length ? this.list.shift()
                          : this.constructor.apply(this, arguments);
}

FreeList.prototype.free = function (obj) {
  debug("free " + this.name + " " + this.list.length);
  if (this.list.length < this.max) {
    this.list.push(obj);
  }
}


var ioWatchers = new FreeList("iowatcher", 100, function () {
  return new IOWatcher();
});

var nb = 0;
var buffers = new FreeList("buffer", 100, function (l) {
  return new process.Buffer(500);
});

// Allocated on demand.
var recvBuffer = null;
function allocRecvBuffer () {
  recvBuffer = new process.Buffer(40*1024);
  recvBuffer.used = 0;
}

function Socket (peerInfo) {
  process.EventEmitter.call(this);

  var self = this;

  self._readWatcher = ioWatchers.alloc();
  self._readWatcher.callback = function () {
    // If this is the first recv (recvBuffer doesn't exist) or we've used up
    // most of the recvBuffer, allocate a new one.
    if (recvBuffer) {
      if (recvBuffer.length - recvBuffer.used < 128) {
        // discard the old recvBuffer. Can't add to the free list because
        // users might have refernces to slices on it.
        recvBuffer = null;
        allocRecvBuffer();
      }
    } else {
      allocRecvBuffer();
    }

    debug('recvBuffer.used ' + recvBuffer.used);
    var bytesRead;

    if (self.type == "unix") {
      bytesRead = recvMsg(self.fd,
                          recvBuffer,
                          recvBuffer.used,
                          recvBuffer.length - recvBuffer.used);
      debug('recvMsg.fd ' + recvMsg.fd);
      if (recvMsg.fd) {
        self.emit('fd', recvMsg.fd);
      }
    } else {
      bytesRead = read(self.fd,
                       recvBuffer,
                       recvBuffer.used,
                       recvBuffer.length - recvBuffer.used);
    }

    debug('bytesRead ' + bytesRead + '\n');

    if (!recvMsg.fd && bytesRead == 0) {
      self.readable = false;
      self._readWatcher.stop();
      self.emit('end');
      if (!self.writable) self.forceClose();
    } else if (bytesRead > 0) {
      var start = recvBuffer.used;
      var end = recvBuffer.used + bytesRead;
      if (self.listeners('data').length) {
        // emit a slice
        self.emit('data', recvBuffer.slice(start, end));
      }
      if (self.listeners('dataLite').length) {
        // emit the original buffer with end points.
        self.emit('dataLite', recvBuffer, start, end);
      }
      recvBuffer.used += bytesRead;
    }
  };
  self.readable = false;

  self.sendQueue = [];    // queue of buffers that need to be written to socket
                          // XXX use link list?
  self.sendQueueSize = 0; // in bytes, not to be confused with sendQueue.length!
  self.sendMessageQueueSize = 0; // number of messages remaining to be sent
  self._doFlush = function () {
    /* Socket becomes writeable on connect() but don't flush if there's
     * nothing actually to write */
    if ((self.sendQueueSize == 0) && (self.sendMessageQueueSize == 0)) {
        return;
    }
    if (self.flush()) {
      assert(self.sendQueueSize == 0);
      assert(self.sendMessageQueueSize == 0);
      self.emit("drain");
    }
  };
  self._writeWatcher = ioWatchers.alloc();
  self._writeWatcher.callback = self._doFlush;
  self.writable = false;

  if (peerInfo) {
    self.fd = peerInfo.fd;
    self.remoteAddress = peerInfo.remoteAddress;
    self.remotePort = peerInfo.remotePort;

    self._readWatcher.set(self.fd, true, false);
    self._readWatcher.start();
    self.readable = true;

    self._writeWatcher.set(self.fd, false, true);
    self.writable = true;
  }
};
process.inherits(Socket, process.EventEmitter);
exports.Socket = Socket;


exports.createConnection = function (port, host) {
  var s = new Socket();
  s.connect(port, host);
  return s;
};


Socket.prototype._allocateSendBuffer = function () {
  var b = buffers.alloc(1024);
  b.used = 0;
  b.sent = 0;
  b.isMsg = false;
  this.sendQueue.push(b);
  return b;
};


Socket.prototype._sendString = function (data, encoding) {
  var self = this;
  if (!self.writable) throw new Error('Socket is not writable');
  var buffer;
  if (self.sendQueue.length == 0) {
    buffer = self._allocateSendBuffer();
  } else {
    buffer = self._sendQueueLast();
    if (buffer.used == buffer.length) {
      buffer = self._allocateSendBuffer();
    }
  }

  encoding = encoding || 'ascii'; // default to ascii since it's faster

  var charsWritten;
  var bytesWritten;

  // The special encoding "fd" means that data is an integer FD and we want
  // to pass the FD on the socket with sendmsg()
  if (encoding == "fd") {
    buffer.isFd = true;
    // TODO is this OK -- does it guarantee that the fd is the only thing in the buffer?
    charsWritten = buffer.asciiWrite(data, buffer.used, buffer.length - buffer.used);
    bytesWritten = charsWritten;
  } else if (encoding.toLowerCase() == 'utf8') {
    buffer.isFd = false;
    charsWritten = buffer.utf8Write(data,
                                    buffer.used,
                                    buffer.length - buffer.used);
    bytesWritten = process.Buffer.utf8ByteLength(data.slice(0, charsWritten));
  } else {
    // ascii
    buffer.isFd = false;
    charsWritten = buffer.asciiWrite(data,
                                     buffer.used,
                                     buffer.length - buffer.used);
    bytesWritten = charsWritten;
  }

  buffer.used += bytesWritten;
  if (buffer.isFd) {
    self.sendMessageQueueSize += 1;
  } else {
    self.sendQueueSize += bytesWritten;
  }

  debug('charsWritten ' + charsWritten);
  debug('buffer.used ' + buffer.used);

  // If we didn't finish, then recurse with the rest of the string.
  if (charsWritten < data.length) {
    debug('recursive send');
    self._sendString(data.slice(charsWritten), encoding);
  }
};


Socket.prototype._sendQueueLast = function () {
  return this.sendQueue.length > 0 ? this.sendQueue[this.sendQueue.length-1]
                                   : null;
};


// Returns true if all the data was flushed to socket. Returns false if
// something was queued. If data was queued, then the "drain" event will
// signal when it has been finally flushed to socket.
Socket.prototype.send = function (data, encoding) {
  var self = this;

  if (!self.writable) throw new Error('Socket is not writable');

  if (self._sendQueueLast() == END_OF_FILE) {
    throw new Error('socket.close() called already; cannot write.');
  }

  if (typeof(data) == 'string') {
    self._sendString(data, encoding);
  } else {
    // data is a process.Buffer
    // walk through the sendQueue, find the first empty buffer
    //var inserted = false;
    data.sent = 0;
    data.used = data.length;
    self.sendQueue.push(data);
    self.sendQueueSize += data.used;
  }
  return this.flush();
};

// Sends a file descriptor over a unix socket
Socket.prototype.sendFD = function(socketToPass) {
  var self = this;

  if (!self.writable) throw new Error('Socket is not writable');

  if (self._sendQueueLast() == END_OF_FILE) {
    throw new Error('socket.close() called already; cannot write.');
  }

  if (self.type != "unix") {
    throw new Error('FD passing only available on unix sockets');
  }

  if (! socketToPass instanceof Socket) {
    throw new Error('Provided arg is not a socket');
  }

  return self.send(socketToPass.fd.toString(), "fd");
};


// Flushes the write buffer out. Emits "drain" if the buffer is empty.
Socket.prototype.flush = function () {
  var self = this;

  var bytesWritten;
  while (self.sendQueue.length) {
    if (!self.writable) throw new Error('Socket is not writable');

    var b = self.sendQueue[0];

    if (b == END_OF_FILE) {
      self._shutdown();
      break;
    }

    if (b.sent == b.used) {
      // shift!
      self.sendQueue.shift();
      buffers.free(b);
      continue;
    }

    var fdToSend = null;
    if (b.isFd) {
      fdToSend = parseInt(b.asciiSlice(b.sent, b.used - b.sent));
      bytesWritten = sendFD(self.fd, fdToSend);
    } else {
      bytesWritten = write(self.fd,
                           b,
                           b.sent,
                           b.used - b.sent);
    }
    if (bytesWritten === null) {
      // could not flush everything
      self._writeWatcher.start();
      assert(self.sendQueueSize > 0);
      return false;
    }
    if (b.isFd) {
      b.sent = b.used;
      self.sendMessageQueueSize -= 1;
      debug('sent fd: ' + fdToSend);
    } else {
      b.sent += bytesWritten;
      self.sendQueueSize -= bytesWritten;
      debug('bytes sent: ' + b.sent);
    }
  }
  if (self._writeWatcher) self._writeWatcher.stop();
  return true;
};


// var stream = new Socket();
// stream.connect(80)               - TCP connect to port 80 on the localhost
// stream.connect(80, 'nodejs.org') - TCP connect to port 80 on nodejs.org
// stream.connect('/tmp/socket')    - UNIX connect to socket specified by path
Socket.prototype.connect = function () {
  var self = this;
  if (self.fd) throw new Error('Socket already opened');

  function doConnect () {
    try {
      connect(self.fd, arguments[0], arguments[1]);
    } catch (e) {
      close(self.fd);
      throw e;
    }

    // Don't start the read watcher until connection is established
    self._readWatcher.set(self.fd, true, false);

    // How to connect on POSIX: Wait for fd to become writable, then call
    // socketError() if there isn't an error, we're connected. AFAIK this a
    // platform independent way determining when a non-blocking connection
    // is established, but I have only seen it documented in the Linux
   // Manual Page connect(2) under the error code EINPROGRESS.
    self._writeWatcher.set(self.fd, false, true);
    self._writeWatcher.start();
    self._writeWatcher.callback = function () {
        var errno = socketError(self.fd);
      if (errno == 0) {
        // connection established
        self._readWatcher.start();
        self.readable = true;
        self.writable = true;
        self._writeWatcher.callback = self._doFlush;
        self.emit('connect');
      } else if (errno != EINPROGRESS) {
        var e = new Error('connection error');
        e.errno = errno;
        self.forceClose(e);
      }
    };
  }

  if (typeof(arguments[0]) == 'string') {
    self.fd = socket('unix');
    self.type = 'unix';
    // TODO check if sockfile exists?
    doConnect(arguments[0]);
  } else {
    self.fd = socket('tcp');
    self.type = 'tcp';
    // TODO dns resolution on arguments[1]
    var port = arguments[0];
    lookupDomainName(arguments[1], function (ip) {
      doConnect(port, ip);
    });
  }
};


Socket.prototype.address = function () {
  return getsockname(this.fd);
};

Socket.prototype.setNoDelay = function (v) {
  if (this.type == 'tcp') setNoDelay(this.fd, v);
};


Socket.prototype.forceClose = function (exception) {
  // recvBuffer is shared between sockets, so don't need to free it here.

  var b;
  while (this.sendQueue.length) {
    b = this.sendQueue.shift();
    if (b instanceof process.Buffer) buffers.free(b);
  }

  if (this._writeWatcher) {
    this._writeWatcher.stop();
    ioWatchers.free(this._writeWatcher);
    this._writeWatcher = null;
  }

  if (this._readWatcher) {
    this._readWatcher.stop();
    ioWatchers.free(this._readWatcher);
    this._readWatcher = null;
  }

  if (this.fd) {
    close(this.fd);
    this.fd = null;
    this.emit('close', exception);
  }
};


Socket.prototype._shutdown = function () {
  if (this.writable) {
    this.writable = false;
    shutdown(this.fd, 'write');
    if (!this.readable) this.forceClose();
  }
};


Socket.prototype.close = function () {
  if (this.writable) {
    if (this._sendQueueLast() != END_OF_FILE) {
      this.sendQueue.push(END_OF_FILE);
      this.flush();
    }
  }
};


function Server (listener) {
  process.EventEmitter.call(this);
  var self = this;

  if (listener) {
    self.addListener('connection', listener);
  }

  self.watcher = new IOWatcher();
  self.watcher.host = self;
  self.watcher.callback = function (readable, writeable) {
    while (self.fd) {
      var peerInfo = accept(self.fd);
      debug('accept: ' + JSON.stringify(peerInfo));
      if (!peerInfo) return;
      var peer = new Socket(peerInfo);
      peer.type = self.type;
      peer.server = self;
      self.emit('connection', peer);
    }
  };
}
process.inherits(Server, process.EventEmitter);
exports.Server = Server;


exports.createServer = function (listener) {
  return new Server(listener);
};

/* This function does both an ipv4 and ipv6 look up.
 * It first tries the ipv4 look up, if that fails, then it does the ipv6.
 */
function lookupDomainName (dn, callback) {
  if (!needsLookup(dn)) {
    callback(dn);
  } else {
    debug("getaddrinfo 4 " + dn);
    getaddrinfo(dn, 4, function (r4) {
      if (r4 instanceof Error) throw r4;
      if (r4.length > 0) {
        debug("getaddrinfo 4 found " + r4);
        callback(r4[0]);
      } else {
        debug("getaddrinfo 6 " + dn);
        getaddrinfo(dn, 6, function (r6) {
          if (r6 instanceof Error) throw r6;
          if (r6.length < 0) {
            throw new Error("No address associated with hostname " + dn);
          }
          debug("getaddrinfo 6 found " + r6);
          callback(r6[0]);
        });
      }
    });
  }
}


// Listen on a UNIX socket
// server.listen("/tmp/socket");
//
// Listen on port 8000, accept connections from INADDR_ANY.
// server.listen(8000);
//
// Listen on port 8000, accept connections to "192.168.1.2"
// server.listen(8000, "192.168.1.2");
Server.prototype.listen = function () {
  var self = this;
  if (self.fd) throw new Error('Server already opened');

  function doListen () {
    listen(self.fd, 128);
    self.watcher.set(self.fd, true, false);
    self.watcher.start();
    self.emit("listening");
  }

  if (typeof(arguments[0]) == 'string') {
    // the first argument specifies a path
    self.fd = socket('unix');
    self.type = 'unix';
    var path = arguments[0];
    self.path = path;
    // unlink sockfile if it exists
    process.fs.stat(path, function (r) {
      if (r instanceof Error) {
        if (r.errno == ENOENT) {
          bind(self.fd, path);
          doListen();
        } else {
          throw r;
        }
      } else {
        if (!r.isFile()) {
          throw new Error("Non-file exists at  " + path);
        } else {
          process.fs.unlink(path, function (err) {
            if (err) {
              throw err;
            } else {
              bind(self.fd, path);
              doListen();
            }
          });
        }
      }
    });
  } else if (!arguments[0]) {
    // Don't bind(). OS will assign a port with INADDR_ANY.
    // The port can be found with server.address()
    self.fd = socket('tcp');
    self.type = 'tcp';
    doListen();
  } else {
    // the first argument is the port, the second an IP
    self.fd = socket('tcp');
    self.type = 'tcp';
    var port = arguments[0];
    debug("starting tcp server on port " + port);
    lookupDomainName(arguments[1], function (ip) {
      debug("starting tcp server on ip " + ip);
      bind(self.fd, port, ip);
      doListen();
    });
  }
};


Server.prototype.address = function () {
  return getsockname(this.fd);
};


Server.prototype.close = function () {
  var self = this;
  if (!self.fd) throw new Error('Not running');

  self.watcher.stop();

  close(self.fd);
  self.fd = null;

  if (self.type === "unix") {
    process.fs.unlink(self.path, function () {
      self.emit("close");
    });
  } else {
    self.emit("close");
  }
};
