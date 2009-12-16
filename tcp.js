var debugLevel = 0;
if ("NODE_DEBUG" in process.ENV) debugLevel = 1;
function debug (x) {
  if (debugLevel > 0) {
    process.stdio.writeError(x + "\n");
  }
}

var socket    = process.socket;
var bind      = process.bind;
var listen    = process.listen;
var accept    = process.accept;
var close     = process.close;
var shutdown  = process.shutdown;
var read      = process.read;
var write     = process.write;
var toRead    = process.toRead;

var Stream = function (peerInfo) {
  process.EventEmitter.call();

  var self = this;

  self.fd = peerInfo.fd;
  self.remoteAddress = peerInfo.remoteAddress;
  self.remotePort = peerInfo.remotePort;

  // Allocated on demand.
  self.recvBuffer = null;
  self.sendQueue = [];

  self.readWatcher = new process.IOWatcher(function () {
    debug("\n" + self.fd + " readable");

    // If this is the first recv (recvBuffer doesn't exist) or we've used up
    // most of the recvBuffer, allocate a new one.
    if (!self.recvBuffer || 
        self.recvBuffer.length - self.recvBuffer.used < 128) {
      self._allocateNewRecvBuf();
    }

    debug("recvBuffer.used " + self.recvBuffer.used);
    var bytesRead = read(self.fd,
                         self.recvBuffer,
                         self.recvBuffer.used,
                         self.recvBuffer.length - self.recvBuffer.used);
    debug("bytesRead " + bytesRead + "\n");

    if (bytesRead == 0) {
      self.readable = false;
      self.readWatcher.stop();
      self.emit("eof");
    } else {
      var slice = self.recvBuffer.slice(self.recvBuffer.used,
                                        self.recvBuffer.used + bytesRead);
      self.recvBuffer.used += bytesRead;
      self.emit("receive", slice);
    }
  });
  self.readWatcher.set(self.fd, true, false);
  self.readWatcher.start();

  self.writeWatcher = new process.IOWatcher(function () {
    self.flush();
  });
  self.writeWatcher.set(self.fd, false, true);

  self.readable = true;
  self.writable = true;
};
process.inherits(Stream, process.EventEmitter);

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

Stream.prototype.send = function (data, encoding) {
  var self = this;
  if (typeof(data) == "string") {
    var buffer;
    if (self.sendQueue.length == 0) {
      buffer = self._allocateSendBuffer();
    } else {
      // walk through the sendQueue, find the first empty buffer
      for (var i = 0; i < self.sendQueue.length; i++) {
        if (self.sendQueue[i].used == 0) {
          buffer = self.sendQueue[i];
          break;
        }
      }
      // if we didn't find one, take the last
      if (!buffer) {
        buffer = self.sendQueue[self.sendQueue.length-1];
        // if last buffer is empty
        if (buffer.length == buffer.used) buffer = self._allocateSendBuffer();
      }
    }

    encoding = encoding || "ascii"; // default to ascii since it's faster

    var charsWritten;

    if (encoding.toLowerCase() == "utf8") {
      charsWritten = buffer.utf8Write(data,
                                      buffer.used,
                                      buffer.length - buffer.used);
      buffer.used += process.Buffer.utf8Length(data.slice(0, charsWritten));
    } else {
      // ascii
      charsWritten = buffer.asciiWrite(data,
                                       buffer.used,
                                       buffer.length - buffer.used);
      buffer.used += charsWritten;
      debug("ascii charsWritten " + charsWritten);
      debug("ascii buffer.used " + buffer.used);
    }


    // If we didn't finish, then recurse with the rest of the string.
    if (charsWritten < data.length) {
      debug("recursive send");
      self.send(data.slice(charsWritten), encoding);
    }
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
  }
  this.flush();
};

// returns true if flushed without getting EAGAIN
// false if it got EAGAIN
Stream.prototype.flush = function () {
  var self = this;
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
      this.writeWatcher.start();
      return false;
    }
    b.sent += bytesWritten;
    debug("bytes sent: " + b.sent);
  }
  this.writeWatcher.stop();
  return true;
};

Stream.prototype.close = function () {
  this.readable = false;
  this.writable = false;

  this.writeWatcher.stop();
  this.readWatcher.stop();
  close(this.fd);
  debug("close peer " + this.fd);
  this.fd = null;
};

var Server = function (listener) {
  var self = this;

  if (listener) {
    self.addListener("connection", listener);
  }

  self.watcher = new process.IOWatcher(function (readable, writeable) {
    while (self.fd) {
      var peerInfo = accept(self.fd);
      debug("accept: " + JSON.stringify(peerInfo));
      if (!peerInfo) return;
      var peer = new Stream(peerInfo);
      self.emit("connection", peer);
    }
  });
};
process.inherits(Server, process.EventEmitter);

Server.prototype.listen = function () {
  var self = this;

  if (self.fd) throw new Error("Already running");

  var backlogIndex;
  if (typeof(arguments[0]) == "string" && arguments.length == 1) {
    // the first argument specifies a path
    self.fd = process.socket("UNIX");
    // TODO unlink sockfile if exists?
    // if (lstat(SOCKFILE, &tstat) == 0) {
    //   assert(S_ISSOCK(tstat.st_mode));
    //   unlink(SOCKFILE);
    // }
    bind(self.fd, arguments[0]);
    backlogIndex = 1;
  } else {
    // the first argument is the port, the second an IP
    self.fd = process.socket("TCP");
    // TODO dns resolution on arguments[1]
    bind(self.fd, arguments[0], arguments[1]);
    backlogIndex = typeof(arguments[1]) == "string" ? 2 : 1;
  }
  listen(self.fd, arguments[backlogIndex] ? arguments[backlogIndex] : 128);

  self.watcher.set(self.fd, true, false); 
  self.watcher.start();
};

Server.prototype.close = function () {
  var self = this;
  if (!self.fd) throw new Error("Not running");
  self.watcher.stop();
  close(self.fd);
  self.fd = null;
};

///////////////////////////////////////////////////////

process.Buffer.prototype.toString = function () {
  return this.utf8Slice(0, this.length);
};

var sys = require("sys");

var server = new Server(function (peer) {
  sys.puts("connection (" + peer.fd + "): " 
          + peer.remoteAddress 
          + " port " 
          + peer.remotePort
          );
  sys.puts("server fd: " + server.fd);

  peer.addListener("receive", function (b) {
    sys.puts("recv (" + b.length + "): " + b);
    peer.send("pong\r\n");
  });
});
//server.listen(8000);
server.listen(8000);
sys.puts("server fd: " + server.fd);
