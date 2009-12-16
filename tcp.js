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

var Peer = function (peerInfo) {
  process.EventEmitter.call();

  var self = this;

  process.mixin(self, peerInfo);

  // Allocated on demand.
  self.recvBuffer = null;

  self.readWatcher = new process.IOWatcher(function () {
    debug("\n" + self.fd + " readable");

    // If this is the first recv (recvBuffer doesn't exist) or we've used up
    // most of the recvBuffer, allocate a new one.
    if (!self.recvBuffer || 
        self.recvBuffer.length - self.recvBufferBytesUsed < 128) {
      self._allocateNewRecvBuf();
    }

    debug("recvBufferBytesUsed " + self.recvBufferBytesUsed);
    var bytesRead = read(self.fd,
                         self.recvBuffer,
                         self.recvBufferBytesUsed,
                         self.recvBuffer.length - self.recvBufferBytesUsed);
    debug("bytesRead " + bytesRead + "\n");

    if (bytesRead == 0) { 
      self.readable = false;
      self.readWatcher.stop();
      self.emit("eof");
    } else {
      var slice = self.recvBuffer.slice(self.recvBufferBytesUsed,
                                        self.recvBufferBytesUsed + bytesRead);
      self.recvBufferBytesUsed += bytesRead;
      self.emit("receive", slice);
    }
  });
  self.readWatcher.set(self.fd, true, false);
  self.readWatcher.start();

  self.writeWatcher = new process.IOWatcher(function () {
    debug(self.fd + " writable");
  });
  self.writeWatcher.set(self.fd, false, true);

  self.readable = true;
  self.writable = true;

  self._out = [];
};
process.inherits(Peer, process.EventEmitter);

Peer.prototype._allocateNewRecvBuf = function () {
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
          self.recvBuffer.length - self.recvBufferBytesUsed > 0) {
        return; // just recv the eof on the old buf.
      }
      newBufferSize = 128;
    }
  }

  self.recvBuffer = new process.Buffer(newBufferSize);
  self.recvBufferBytesUsed = 0;
};

Peer.prototype.close = function () {
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
    debug("readable " + readable);
    debug("writable " + writeable);
    while (self.fd) {
      debug("accept from " + self.fd);
      var peerInfo = accept(self.fd);
      debug("accept: " + JSON.stringify(peerInfo));
      if (!peerInfo) return;
      var peer = new Peer(peerInfo);
      self.emit("connection", peer);
    }
  });
};
process.inherits(Server, process.EventEmitter);

Server.prototype.listen = function () {
  var self = this;

  if (self.fd) throw new Error("Already running");

  if (typeof(arguments[0]) == "string" && arguments.length == 1) {
    // the first argument specifies a path
    self.fd = process.socket("UNIX");
    // TODO unlink sockfile if exists?
    // if (lstat(SOCKFILE, &tstat) == 0) {
    //   assert(S_ISSOCK(tstat.st_mode));
    //   unlink(SOCKFILE);
    // }
    bind(self.fd, arguments[0]);
  } else {
    // the first argument is the port, the second an IP
    self.fd = process.socket("TCP");
    // TODO dns resolution on arguments[1]
    bind(self.fd, arguments[0], arguments[1]);
  }
  listen(self.fd, 128); // TODO configurable backlog

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
  });
});
//server.listen(8000);
server.listen(8000);
sys.puts("server fd: " + server.fd);
