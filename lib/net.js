var sys = require("sys");
var fs = require("fs");
var events = require("events");

var kMinPoolSpace = 128;
var kPoolSize = 40*1024;

var debugLevel = process.env['NODE_DEBUG'] ? 1 : 0;
function debug () {
  if (debugLevel > 0) sys.error.apply(this, arguments);
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
var Buffer = require('buffer').Buffer;

var IOWatcher   = process.IOWatcher;
var assert      = process.assert;

var socket      = binding.socket;
var bind        = binding.bind;
var connect     = binding.connect;
var listen      = binding.listen;
var accept      = binding.accept;
var close       = binding.close;
var shutdown    = binding.shutdown;
var read        = binding.read;
var write       = binding.write;
var toRead      = binding.toRead;
var setNoDelay  = binding.setNoDelay;
var socketError = binding.socketError;
var getsockname = binding.getsockname;
var getaddrinfo = binding.getaddrinfo;
var isIP        = binding.isIP;
var errnoException = binding.errnoException;
var EINPROGRESS = binding.EINPROGRESS;
var ENOENT      = binding.ENOENT;
var END_OF_FILE = 42;


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
    socket._idleStart = new Date();
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
        debug('timeout callback ' + msecs);
        // TODO - don't stop and start the watcher all the time.
        // just set its repeat
        var now = new Date();
        debug("now: " + now);
        var first;
        while (first = peek(list)) {
          var diff = now - first._idleStart;
          if (diff < msecs) {
            list.again(msecs - diff);
            debug(msecs + ' list wait because diff is '  + diff);
            return;
          } else {
            remove(first);
            assert(first != peek(list));
            first.emit('timeout');
            first.forceClose(new Error('idle timeout'));
          }
        }
        debug(msecs + ' list empty');
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
      //debug('unenroll');
      if (list && list._idlePrev == list) {
        //debug('unenroll: list empty');
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
        socket._idleStart = new Date();
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


// Allocated on demand.
var pool = null;
function allocNewPool () {
  pool = new Buffer(kPoolSize);
  pool.used = 0;
}


function _doFlush () {
  var socket = this.socket;
  // Stream becomes writeable on connect() but don't flush if there's
  // nothing actually to write
  if (socket.flush()) {
    if (socket._events && socket._events['drain']) socket.emit("drain");
    if (socket.ondrain) socket.ondrain(); // Optimization
  }
}

function initStream (self) {
  self._readWatcher = ioWatchers.alloc();
  self._readWatcher.callback = function () {
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
      bytesRead = read(self.fd,
                       pool,
                       pool.used,
                       pool.length - pool.used);
    } catch (e) {
      self.forceClose(e);
      return;
    }

    //debug('bytesRead ' + bytesRead + '\n');

    if (bytesRead === 0) {
      self.readable = false;
      self._readWatcher.stop();

      if (self._events && self._events['end']) self.emit('end');
      if (self.onend) self.onend();

      if (!self.writable) self.forceClose();
    } else if (bytesRead > 0) {

      timeout.active(self);

      var start = pool.used;
      var end = pool.used + bytesRead;
      pool.used += bytesRead;

      if (!self._encoding) {
        if (self._events && self._events['data']) {
          // emit a slice
          self.emit('data', pool.slice(start, end));
        }

        // Optimization: emit the original buffer with end points
        if (self.ondata) self.ondata(pool, start, end);
      } else {
        // TODO remove me - we should only output Buffer

        var string;
        switch (self._encoding) {
          case 'utf8':
            string = pool.utf8Slice(start, end);
            break;
          case 'ascii':
            string = pool.asciiSlice(start, end);
            break;
          case 'binary':
            string = pool.binarySlice(start, end);
            break;
          default:
            throw new Error('Unsupported encoding ' + self._encoding + '. Use Buffer'); 
        }
        self.emit('data', string);
      }
    }
  };
  self.readable = false;

  // Queue of buffers and string that need to be written to socket.
  self._writeQueue = []; 
  self._writeQueueEncoding = [];

  self._writeWatcher = ioWatchers.alloc();
  self._writeWatcher.socket = self;
  self._writeWatcher.callback = _doFlush;
  self.writable = false;
}

function Stream (fd) {
  events.EventEmitter.call(this);

  this.fd = null;

  if (parseInt(fd) >= 0) {
    this.open(fd);
  }
};
sys.inherits(Stream, events.EventEmitter);
exports.Stream = Stream;


Stream.prototype.open = function (fd) {
  initStream(this);

  this.fd = fd;

  this.readable = true;

  this._writeWatcher.set(this.fd, false, true);
  this.writable = true;
}


exports.createConnection = function (port, host) {
  var s = new Stream();
  s.connect(port, host);
  return s;
};


Object.defineProperty(Stream.prototype, 'readyState', {
  get: function () {
    if (this._resolving) {
      return 'opening';
    } else if (this.readable && this.writable) {
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


// Returns true if all the data was flushed to socket. Returns false if
// something was queued. If data was queued, then the "drain" event will
// signal when it has been finally flushed to socket.
Stream.prototype.write = function (data, encoding) {
  if (this._writeQueue && this._writeQueue.length) {
    // Slow. There is already a write queue, so let's append to it.
    if (this._writeQueueLast() == END_OF_FILE) {
      throw new Error('Stream.close() called already; cannot write.');
    }
    this._writeQueue.push(data); // TODO if string of the same encoding concat?
    this._writeQueueEncoding.push(encoding);
    return false;
  } else {
    // Fast.
    // The most common case. There is no write queue. Just push the data
    // directly to the socket.
    return this._writeOut(data, encoding);
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
Stream.prototype._writeOut = function (data, encoding) {
  if (!this.writable) throw new Error('Stream is not writable');

  if (data.length == 0) return true;

  var buffer, off, len;
  var bytesWritten, charsWritten;

  var queuedData = false;

  if (typeof data != 'string') {
    // 'data' is a buffer, ignore 'encoding'
    buffer = data;
    off = 0;
    len = data.length;

  } else {
    assert(typeof data == 'string')

    if (!pool || pool.length - pool.used < kMinPoolSpace) {
      pool = null;
      allocNewPool();
    }

    if (encoding == 'binary') {
      bytesWritten = pool.binaryWrite(data, pool.used);
      charsWritten = bytesWritten;
    } else if (encoding == 'ascii') {
      bytesWritten = pool.asciiWrite(data, pool.used);
      charsWritten = bytesWritten;

    } else {
      // default to utf8
      bytesWritten = pool.utf8Write(data, pool.used);
      // Don't include the null
      if (pool[pool.used + bytesWritten-1] == 0) bytesWritten--;
      // XXX Hacky way to find out the number of characters written.
      // Waiting for a more optimal way: http://codereview.chromium.org/1539013
      var _s = pool.utf8Slice(pool.used, pool.used + bytesWritten);
      charsWritten = _s.length;

    }

    assert(bytesWritten > 0);

    buffer = pool;
    len = bytesWritten;
    off = pool.used;

    pool.used += bytesWritten;

    debug('wrote ' + bytesWritten + ' bytes to pool');

    if (charsWritten != data.length) {
      //debug("couldn't fit " + (data.length - charsWritten) + " bytes into the pool\n");
      // Unshift whatever didn't fit onto the buffer
      this._writeQueue.unshift(data.slice(charsWritten));
      this._writeQueueEncoding.unshift(encoding);
      this._writeWatcher.start();
      queuedData = true;
    }
  }


  // Send the buffer.

  try {
    bytesWritten = write(this.fd, buffer, off, len);
  } catch (e) {
    this.forceClose(e);
    return false;
  }

  debug('wrote ' + bytesWritten + ' to socket. [fd, off, len] = ' + JSON.stringify([this.fd, off, len]) + "\n");

  timeout.active(this);

  if (bytesWritten == len) {
    // awesome. sent to buffer.
    buffer.used -= len; // Optimization - save the space
    if (queuedData) {
      return false;
    } else {
      return true;
    }
  }

  // Didn't write the entire thing to buffer.
  // Need to wait for the socket to become available before trying again.
  this._writeWatcher.start();

  // Slice out the data left.
  var leftOver = data.slice(off + bytesWritten, off + len);
  leftOver.used = leftOver.length; // used the whole thing...

  //  sys.error('data.used = ' + data.used);
  //if (!this._writeQueue) initWriteStream(this);

  // data should be the next thing to write.
  this._writeQueue.unshift(leftOver);
  this._writeQueueEncoding.unshift(null);

  return false;
}


// Flushes the write buffer out.
// Returns true if the entire buffer was flushed.
Stream.prototype.flush = function () {
  while (this._writeQueue && this._writeQueue.length) {
    var data = this._writeQueue.shift();
    var encoding = this._writeQueueEncoding.shift();

    if (data == END_OF_FILE) {
      this._shutdown();
      return true;
    }

    var flushed = this._writeOut(data,encoding);
    if (!flushed) return false;
  }
  if (this._writeWatcher) this._writeWatcher.stop();
  return true;
};


Stream.prototype.send = function () {
  throw new Error('send renamed to write');
};


Stream.prototype._writeQueueLast = function () {
  return this._writeQueue.length > 0 ? this._writeQueue[this._writeQueue.length-1]
                                     : null;
};


Stream.prototype.setEncoding = function (enc) {
  // TODO check values, error out on bad, and deprecation message?
  this._encoding = enc.toLowerCase();
};


function doConnect (socket, port, host) {
  try {
    connect(socket.fd, port, host);
  } catch (e) {
    socket.forceClose(e);
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
  socket._writeWatcher.callback = function () {
    var errno = socketError(socket.fd);
    if (errno == 0) {
      // connection established
      socket.resume();
      socket.readable = true;
      socket.writable = true;
      socket._writeWatcher.callback = socket._doFlush;
      socket.emit('connect');
    } else if (errno != EINPROGRESS) {
      socket.forceClose(errnoException(errno, 'connect'));
    }
  };
}


// var stream = new Stream();
// stream.connect(80)               - TCP connect to port 80 on the localhost
// stream.connect(80, 'nodejs.org') - TCP connect to port 80 on nodejs.org
// stream.connect('/tmp/socket')    - UNIX connect to socket specified by path
Stream.prototype.connect = function () {
  var self = this;
  initStream(self);
  if (self.fd) throw new Error('Stream already opened');
  if (!self._readWatcher) throw new Error('No readWatcher');
  
  timeout.active(socket);

  var port = parseInt(arguments[0]);

  if (port >= 0) {
    //debug('new fd = ' + self.fd);
    // TODO dns resolution on arguments[1]
    var port = arguments[0];
    self._resolving = true;
    lookupDomainName(arguments[1], function (ip, isV4) {
      self.type = isV4 ? 'tcp4' : 'tcp6';
      self.fd = socket(self.type);
      self._resolving = false;
      doConnect(self, port, ip);
    });
  } else {
    self.fd = socket('unix');
    self.type = 'unix';
    // TODO check if sockfile exists?
    doConnect(self, arguments[0]);
  }
};


Stream.prototype.address = function () {
  return getsockname(this.fd);
};


Stream.prototype.setNoDelay = function (v) {
  if (this.type == 'tcp') setNoDelay(this.fd, v);
};


Stream.prototype.setTimeout = function (msecs) {
  timeout.enroll(this, msecs);
};


Stream.prototype.pause = function () {
  this._readWatcher.stop();
};


Stream.prototype.resume = function () {
  if (this.fd === null) throw new Error('Cannot resume() closed Stream.');
  this._readWatcher.set(this.fd, true, false);
  this._readWatcher.start();
};


Stream.prototype.forceClose = function (exception) {
  // pool is shared between sockets, so don't need to free it here.
  var self = this;

  // TODO would like to set _writeQueue to null to avoid extra object alloc,
  // but lots of code assumes this._writeQueue is always an array.
  this._writeQueue = [];

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

  // FIXME Bug when this.fd == 0
  if (typeof this.fd == 'number') {
    close(this.fd);
    this.fd = null;
    process.nextTick(function () {
      if (exception) self.emit('error', exception);
      self.emit('close', exception ? true : false);
    });
  }
};


Stream.prototype._shutdown = function () {
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


Stream.prototype.close = function () {
  if (this.writable) {
    if (this._writeQueueLast() != END_OF_FILE) {
      this._writeQueue.push(END_OF_FILE);
      this.flush();
    }
  }
};


function Server (listener) {
  events.EventEmitter.call(this);
  var self = this;

  if (listener) {
    self.addListener('connection', listener);
  }

  self.watcher = new IOWatcher();
  self.watcher.host = self;
  self.watcher.callback = function () {
    while (self.fd) {
      var peerInfo = accept(self.fd);
      if (!peerInfo) return;

      var s = new Stream(peerInfo.fd);
      s.remoteAddress = peerInfo.address;
      s.remotePort = peerInfo.port;
      s.type = self.type;
      s.server = self;
      s.resume();

      self.emit('connection', s);
      // The 'connect' event  probably should be removed for server-side
      // sockets. It's redundent.
      s.emit('connect');
    }
  };
}
sys.inherits(Server, events.EventEmitter);
exports.Server = Server;


exports.createServer = function (listener) {
  return new Server(listener);
};

// This function does both an ipv4 and ipv6 look up.
// It first tries the ipv4 look up, if that fails, then it does the ipv6.
//  callback(dn, isV4
function lookupDomainName (dn, callback) {
  var kind = isIP(dn);
  debug('kind = ' + kind);
  if (kind) {
    // Always wait until the next tick this is so people can do
    //
    //   server.listen(8000);
    //   server.addListener('listening', fn);
    //
    // Marginally slower, but a lot fewer WTFs.
    process.nextTick(function () { callback(dn, kind == 4 ? true : false); })
  } else {
    debug("getaddrinfo 4 " + dn);
    getaddrinfo(dn, 4, function (r4) {
      if (r4 instanceof Error) throw r4;
      if (r4.length > 0) {
        debug("getaddrinfo 4 found " + r4);
        callback(r4[0], true);
      } else {
        debug("getaddrinfo 6 " + dn);
        getaddrinfo(dn, 6, function (r6) {
          if (r6 instanceof Error) throw r6;
          if (r6.length < 0) {
            throw new Error("No address associated with hostname " + dn);
          }
          debug("getaddrinfo 6 found " + r6);
          callback(r6[0], false);
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
          self._doListen();
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
              self._doListen();
            }
          });
        }
      }
    });
  } else if (!arguments[1]) {
    // Don't bind(). OS will assign a port with INADDR_ANY.
    // The port can be found with server.address()
    self.type = 'tcp4';
    self.fd = socket(self.type);
    bind(self.fd, arguments[0]);
    process.nextTick(function () {
      self._doListen();
    });
  } else {
    // the first argument is the port, the second an IP
    var port = arguments[0];
    lookupDomainName(arguments[1], function (ip, isV4) {
      self.type = isV4 ? 'tcp4' : 'tcp6';
      self.fd = socket(self.type);
      bind(self.fd, port, ip);
      self._doListen();
    });
  }
};

Server.prototype._doListen = function () {
  listen(this.fd, 128);
  this.watcher.set(this.fd, true, false);
  this.watcher.start();
  this.emit("listening");
}



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
