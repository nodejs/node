var sys = require("./sys");
var fs = require("./fs");
var debugLevel = 0;
if ('NODE_DEBUG' in process.ENV) debugLevel = 1;
function debug (x) {
  if (debugLevel > 0) {
    process.stdio.writeError(x + '\n');
  }
}


var Buffer      = process.Buffer;
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
var errnoException = process.errnoException;
var EINPROGRESS = process.EINPROGRESS;
var ENOENT      = process.ENOENT;
var END_OF_FILE = 0;


// IDLE TIMEOUTS
//
// Because often many sockets will have the same idle timeout we will not
// use one timeout watcher per socket. It is too much overhead.  Instead
// we'll use a single watcher for all sockets with the same timeout value
// and a linked list. This technique is described in the libev manual:
// http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#Be_smart_about_timeouts


var timeout = new (function () {
  // Object containing all lists, timers
  // key = time in milliseconds
  // value = list
  var lists = {};

  // show the most idle socket
  function peek (list) {
    if (list._idlePrev == list) return null;
    return list._idlePrev;
  }


  // remove the most idle socket from the list
  function shift (list) {
    var first = list._idlePrev;
    remove(first);
    return first;
  }


  // remove a socket from its list
  function remove (socket) {
    socket._idleNext._idlePrev = socket._idlePrev;
    socket._idlePrev._idleNext = socket._idleNext;
  }


  // remove a socket from its list and place at the end.
  function append (list, socket) {
    remove(socket);
    socket._idleNext = list._idleNext;
    socket._idleNext._idlePrev = socket;
    socket._idlePrev = list
    list._idleNext = socket;
  }


  function normalize (msecs) {
    if (!msecs || msecs <= 0) return 0;
    // round up to one sec
    if (msecs < 1000) return 1000;
    // round down to nearest second.
    return msecs - (msecs % 1000);
  }

  // the main function - creates lists on demand and the watchers associated
  // with them.
  function insert (socket, msecs) {
    socket._idleStart = process.now;
    socket._idleTimeout = msecs;

    if (!msecs) return;

    var list;

    if (lists[msecs]) {
      list = lists[msecs];
    } else {
      list = new process.Timer();
      list._idleNext = list;
      list._idlePrev = list;

      lists[msecs] = list;

      list.callback = function () {
        sys.puts('timeout callback ' + msecs);
        // TODO - don't stop and start the watcher all the time.
        // just set its repeat
        var now = process.now;
        var first;
        while (first = peek(list)) {
          var diff = now - first._idleStart;
          if (diff < msecs) {
            list.again(msecs - diff);
            sys.puts(msecs + ' list wait');
            return;
          } else {
            remove(first);
            assert(first != peek(list));
            first.emit('timeout');
            first.forceClose(new Error('idle timeout'));
          }
        }
        sys.puts(msecs + ' list empty');
        assert(list._idleNext == list); // list is empty
        list.stop();
      };
    }

    if (list._idleNext == list) {
      // if empty (re)start the timer
      list.again(msecs);
    }

    append(list, socket);
    assert(list._idleNext != list); // list is not empty
  }


  var unenroll = this.unenroll = function (socket) {
    if (socket._idleNext) {
      socket._idleNext._idlePrev = socket._idlePrev;
      socket._idlePrev._idleNext = socket._idleNext;

      var list = lists[socket._idleTimeout];
      // if empty then stop the watcher
      //sys.puts('unenroll');
      if (list && list._idlePrev == list) {
        //sys.puts('unenroll: list empty');
        list.stop();
      }
    }
  };


  // Does not start the time, just sets up the members needed.
  this.enroll = function (socket, msecs) {
    // if this socket was already in a list somewhere
    // then we should unenroll it from that
    if (socket._idleNext) unenroll(socket);

    socket._idleTimeout = msecs;
    socket._idleNext = socket;
    socket._idlePrev = socket;
  };

  // call this whenever the socket is active (not idle)
  // it will reset its timeout.
  this.active = function (socket) {
    var msecs = socket._idleTimeout;
    if (msecs) {
      var list = lists[msecs];
      if (socket._idleNext == socket) {
        insert(socket, msecs);
      } else {
        // inline append
        socket._idleStart = process.now;
        socket._idleNext._idlePrev = socket._idlePrev;
        socket._idlePrev._idleNext = socket._idleNext;
        socket._idleNext = list._idleNext;
        socket._idleNext._idlePrev = socket;
        socket._idlePrev = list
        list._idleNext = socket;
      }
    }
  };
})();





// This is a free list to avoid creating so many of the same object.

function FreeList (name, max, constructor) {
  this.name = name;
  this.constructor = constructor;
  this.max = max;
  this.list = [];
}


FreeList.prototype.alloc = function () {
  //debug("alloc " + this.name + " " + this.list.length);
  return this.list.length ? this.list.shift()
                          : this.constructor.apply(this, arguments);
};


FreeList.prototype.free = function (obj) {
  //debug("free " + this.name + " " + this.list.length);
  if (this.list.length < this.max) {
    this.list.push(obj);
  }
};


var ioWatchers = new FreeList("iowatcher", 100, function () {
  return new IOWatcher();
});


var nb = 0;
var buffers = new FreeList("buffer", 100, function (l) {
  return new Buffer(l);
});


// Allocated on demand.
var recvBuffer = null;
function allocRecvBuffer () {
  recvBuffer = new Buffer(40*1024);
  recvBuffer.used = 0;
}


function _doFlush () {
  var socket = this.socket;
  // Socket becomes writeable on connect() but don't flush if there's
  // nothing actually to write
  if ((socket._writeQueueSize == 0) && (socket._writeMessageQueueSize == 0)) {
      return;
  }
  if (socket.flush()) {
    assert(socket._writeQueueSize == 0);
    assert(socket._writeMessageQueueSize == 0);

    if (socket._events && socket._events['drain']) socket.emit("drain");
    if (socket.ondrain) socket.ondrain(); // Optimization
  }
}

function initSocket (self) {
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

    //debug('recvBuffer.used ' + recvBuffer.used);
    var bytesRead;

    try {
      if (self.type == "unix") {
        bytesRead = recvMsg(self.fd,
                            recvBuffer,
                            recvBuffer.used,
                            recvBuffer.length - recvBuffer.used);
        //debug('recvMsg.fd ' + recvMsg.fd);
        if (recvMsg.fd) {
          self.emit('fd', recvMsg.fd);
        }
      } else {
        bytesRead = read(self.fd,
                         recvBuffer,
                         recvBuffer.used,
                         recvBuffer.length - recvBuffer.used);
      }
    } catch (e) {
      self.forceClose(e);
      return;
    }

    //debug('bytesRead ' + bytesRead + '\n');

    if (!recvMsg.fd && bytesRead == 0) {
      self.readable = false;
      self._readWatcher.stop();

      if (self._events && self._events['end']) self.emit('end');
      if (self.onend) self.onend();

      if (!self.writable) self.forceClose();
    } else if (bytesRead > 0) {

      timeout.active(self);

      var start = recvBuffer.used;
      var end = recvBuffer.used + bytesRead;

      if (!self._encoding) {
        if (self._events && self._events['data']) {
          // emit a slice
          self.emit('data', recvBuffer.slice(start, end));
        }

        // Optimization: emit the original buffer with end points
        if (self.ondata) self.ondata(recvBuffer, start, end);
      } else {
        // TODO remove me - we should only output Buffer

        var string;
        switch (self._encoding) {
          case 'utf8':
            string = recvBuffer.utf8Slice(start, end);
            break;
          case 'ascii':
            string = recvBuffer.asciiSlice(start, end);
            break;
          default:
            throw new Error('Unsupported encoding ' + self._encoding + '. Use Buffer'); 
        }
        self.emit('data', string);
      }


      recvBuffer.used += bytesRead;
    }
  };
  self.readable = false;

  self._writeQueue = [];    // queue of buffers that need to be written to socket
                          // XXX use link list?
  self._writeQueueSize = 0; // in bytes, not to be confused with _writeQueue.length!
  self._writeMessageQueueSize = 0; // number of messages remaining to be sent

  self._writeWatcher = ioWatchers.alloc();
  self._writeWatcher.socket = self;
  self._writeWatcher.callback = _doFlush;
  self.writable = false;
}

function Socket (peerInfo) {
  process.EventEmitter.call(this);

  if (peerInfo) {
    initSocket(this);

    this.fd = peerInfo.fd;
    this.remoteAddress = peerInfo.remoteAddress;
    this.remotePort = peerInfo.remotePort;

    this.resume();
    this.readable = true;

    this._writeWatcher.set(this.fd, false, true);
    this.writable = true;
  }
};
sys.inherits(Socket, process.EventEmitter);
exports.Socket = Socket;


exports.createConnection = function (port, host) {
  var s = new Socket();
  s.connect(port, host);
  return s;
};


var readyStateMessage;
Object.defineProperty(Socket.prototype, 'readyState', {
  get: function () {
    if (!readyStateMessage) {
      readyStateMessage = 'readyState is depricated. Use stream.readable or stream.writable';
      sys.error(readyStateMessage);
    }
    if (this.readable && this.writable) {
      return 'open';
    } else if (this.readable && !this.writable){
      return 'readOnly';
    } else if (!this.readable && this.writable){
      return 'writeOnly';
    } else {
      return 'closed';
    }
  }
});


Socket.prototype._allocateSendBuffer = function () {
  var b = buffers.alloc(1024);
  b.used = 0;
  b.sent = 0;
  b.isMsg = false;
  this._writeQueue.push(b);
  return b;
};


Socket.prototype._writeString = function (data, encoding) {
  var self = this;
  if (!self.writable) throw new Error('Socket is not writable');
  var buffer;

  if (self._writeQueue.length == 0) {
    buffer = self._allocateSendBuffer();
  } else {
    buffer = self.__writeQueueLast();
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
    charsWritten = buffer.utf8Write(data, buffer.used);
    bytesWritten = Buffer.utf8ByteLength(data.slice(0, charsWritten));
  } else {
    // ascii
    buffer.isFd = false;
    charsWritten = buffer.asciiWrite(data, buffer.used);
    bytesWritten = charsWritten;
  }

  buffer.used += bytesWritten;
  if (buffer.isFd) {
    self._writeMessageQueueSize += 1;
  } else {
    self._writeQueueSize += bytesWritten;
  }

  //debug('charsWritten ' + charsWritten);
  //debug('buffer.used ' + buffer.used);

  // If we didn't finish, then recurse with the rest of the string.
  if (charsWritten < data.length) {
    //debug('recursive write');
    self._writeString(data.slice(charsWritten), encoding);
  }
};


Socket.prototype.__writeQueueLast = function () {
  return this._writeQueue.length > 0 ? this._writeQueue[this._writeQueue.length-1]
                                   : null;
};


Socket.prototype.send = function () {
  throw new Error('send renamed to write');
};

Socket.prototype.setEncoding = function (enc) {
  // TODO check values, error out on bad, and deprecation message?
  this._encoding = enc.toLowerCase();
};

// Returns true if all the data was flushed to socket. Returns false if
// something was queued. If data was queued, then the "drain" event will
// signal when it has been finally flushed to socket.
Socket.prototype.write = function (data, encoding) {
  var self = this;

  if (!self.writable) throw new Error('Socket is not writable');

  if (self.__writeQueueLast() == END_OF_FILE) {
    throw new Error('socket.close() called already; cannot write.');
  }

  if (typeof(data) == 'string') {
    self._writeString(data, encoding);
  } else {
    // data is a Buffer
    // walk through the _writeQueue, find the first empty buffer
    //var inserted = false;
    data.sent = 0;
    data.used = data.length;
    self._writeQueue.push(data);
    self._writeQueueSize += data.used;
  }
  return this.flush();
};

// Sends a file descriptor over a unix socket
Socket.prototype.sendFD = function(socketToPass) {
  var self = this;

  if (!self.writable) throw new Error('Socket is not writable');

  if (self.__writeQueueLast() == END_OF_FILE) {
    throw new Error('socket.close() called already; cannot write.');
  }

  if (self.type != "unix") {
    throw new Error('FD passing only available on unix sockets');
  }

  if (! socketToPass instanceof Socket) {
    throw new Error('Provided arg is not a socket');
  }

  return self.write(socketToPass.fd.toString(), "fd");
};


// Flushes the write buffer out.
// Returns true if the entire buffer was flushed.
Socket.prototype.flush = function () {
  var self = this;

  var bytesWritten;
  while (self._writeQueue.length) {
    if (!self.writable) throw new Error('Socket is not writable');

    var b = self._writeQueue[0];

    if (b == END_OF_FILE) {
      self._shutdown();
      return true;
    }

    if (b.sent == b.used) {
      // shift!
      self._writeQueue.shift();
      buffers.free(b);
      continue;
    }

    var fdToSend = null;

    try {
      if (b.isFd) {
        fdToSend = parseInt(b.asciiSlice(b.sent, b.used - b.sent));
        bytesWritten = writeFD(self.fd, fdToSend);
      } else {
        bytesWritten = write(self.fd,
                             b,
                             b.sent,
                             b.used - b.sent);
      }
    } catch (e) {
      self.forceClose(e);
      return false;
    }

    timeout.active(self);


    if (bytesWritten === null) {
      // EAGAIN
      debug('write EAGAIN');
      self._writeWatcher.start();
      assert(self._writeQueueSize > 0);
      return false;
    } else if (b.isFd) {
      b.sent = b.used;
      self._writeMessageQueueSize -= 1;
      //debug('sent fd: ' + fdToSend);
    } else {
      b.sent += bytesWritten;
      self._writeQueueSize -= bytesWritten;
      debug('bytes sent: ' + b.sent);
    }
  }
  if (self._writeWatcher) self._writeWatcher.stop();
  return true;
};


function doConnect (socket, port, host) {
  try {
    connect(socket.fd, port, host);
  } catch (e) {
    socket.forceClose(e);
  }

  // Don't start the read watcher until connection is established
  socket._readWatcher.set(socket.fd, true, false);

  // How to connect on POSIX: Wait for fd to become writable, then call
  // socketError() if there isn't an error, we're connected. AFAIK this a
  // platform independent way determining when a non-blocking connection
  // is established, but I have only seen it documented in the Linux
 // Manual Page connect(2) under the error code EINPROGRESS.
  socket._writeWatcher.set(socket.fd, false, true);
  socket._writeWatcher.start();
  socket._writeWatcher.callback = function () {
    var errno = socketError(socket.fd);
    if (errno == 0) {
      // connection established
      socket._readWatcher.start();
      socket.readable = true;
      socket.writable = true;
      socket._writeWatcher.callback = socket._doFlush;
      socket.emit('connect');
    } else if (errno != EINPROGRESS) {
      socket.forceClose(errnoException(errno, 'connect'));
    }
  };
}


// var stream = new Socket();
// stream.connect(80)               - TCP connect to port 80 on the localhost
// stream.connect(80, 'nodejs.org') - TCP connect to port 80 on nodejs.org
// stream.connect('/tmp/socket')    - UNIX connect to socket specified by path
Socket.prototype.connect = function () {
  initSocket(this);

  var self = this;
  if (self.fd) throw new Error('Socket already opened');
  
  timeout.active(socket);

  if (typeof(arguments[0]) == 'string') {
    self.fd = socket('unix');
    self.type = 'unix';
    // TODO check if sockfile exists?
    doConnect(self, arguments[0]);
  } else {
    self.fd = socket('tcp');
    debug('new fd = ' + self.fd);
    self.type = 'tcp';
    // TODO dns resolution on arguments[1]
    var port = arguments[0];
    lookupDomainName(arguments[1], function (ip) {
      doConnect(self, port, ip);
    });
  }
};


Socket.prototype.address = function () {
  return getsockname(this.fd);
};


Socket.prototype.setNoDelay = function (v) {
  if (this.type == 'tcp') setNoDelay(this.fd, v);
};


Socket.prototype.setTimeout = function (msecs) {
  timeout.enroll(this, msecs);
};


Socket.prototype.pause = function () {
  this._readWatcher.stop();
};


Socket.prototype.resume = function () {
  if (!this.fd) throw new Error('Cannot resume() closed Socket.');
  this._readWatcher.set(this.fd, true, false);
  this._readWatcher.start();
};


Socket.prototype.forceClose = function (exception) {
  // recvBuffer is shared between sockets, so don't need to free it here.
  var self = this;

  var b;
  while (this._writeQueue.length) {
    b = this._writeQueue.shift();
    if (b instanceof Buffer) buffers.free(b);
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

  timeout.unenroll(this);

  if (this.fd) {
    close(this.fd);
    debug('close ' + this.fd);
    this.fd = null;
    process.nextTick(function () {
      if (exception) self.emit('error', exception);
      self.emit('close', exception ? true : false);
    });
  }
};


Socket.prototype._shutdown = function () {
  if (this.writable) {
    this.writable = false;

    try {
      shutdown(this.fd, 'write')
    } catch (e) {
      this.forceClose(e);
      return;
    }

    if (!this.readable) this.forceClose();
  }
};


Socket.prototype.close = function () {
  if (this.writable) {
    if (this.__writeQueueLast() != END_OF_FILE) {
      this._writeQueue.push(END_OF_FILE);
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
      if (!peerInfo) return;
      var peer = new Socket(peerInfo);
      peer.type = self.type;
      peer.server = self;
      self.emit('connection', peer);
      // The 'connect' event  probably should be removed for server-side
      // sockets. It's redundent.
      peer.emit('connect'); 
      timeout.active(peer);
    }
  };
}
sys.inherits(Server, process.EventEmitter);
exports.Server = Server;


exports.createServer = function (listener) {
  return new Server(listener);
};

/* This function does both an ipv4 and ipv6 look up.
 * It first tries the ipv4 look up, if that fails, then it does the ipv6.
 */
function lookupDomainName (dn, callback) {
  if (!needsLookup(dn)) {
    // Always wait until the next tick this is so people can do
    //
    //   server.listen(8000);
    //   server.addListener('listening', fn);
    //
    // Marginally slower, but a lot fewer WTFs.
    process.nextTick(function () { callback(dn); })
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
    fs.stat(path, function (err, r) {
      if (err) {
        if (err.errno == ENOENT) {
          bind(self.fd, path);
          doListen();
        } else {
          throw r;
        }
      } else {
        if (!r.isFile()) {
          throw new Error("Non-file exists at  " + path);
        } else {
          fs.unlink(path, function (err) {
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
    lookupDomainName(arguments[1], function (ip) {
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
    fs.unlink(self.path, function () {
      self.emit("close");
    });
  } else {
    self.emit("close");
  }
};
