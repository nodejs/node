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

var Peer = function (peerInfo) {
  process.EventEmitter.call();

  var self = this;

  self.fd = peerInfo.fd;
  self.remoteAddress = peerInfo.remoteAddress;
  self.remotePort = peerInfo.remotePort;

  self.readWatcher = new process.IOWatcher(function () {
    debug(self.fd + " readable");
  });
  self.readWatcher.set(self.fd, true, false);
  self.readWatcher.start();

  self.writeWatcher = new process.IOWatcher(function () {
    debug(self.fd + " writable");
  });
  self.writeWatcher.set(self.fd, false, true);

  self.readable = true;
  self.writable = true;
};
process.inherits(Peer, process.EventEmitter);

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

Server.prototype.listen = function (port, host) {
  var self = this;

  if (self.fd) throw new Error("Already running");

  self.fd = process.socket("TCP");
  // TODO dns resolution
  bind(self.fd, port, host);
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

var sys = require("sys");
var server = new Server(function (peer) {
  sys.puts("connection (" + peer.fd + "): " 
          + peer.remoteAddress 
          + " port " 
          + peer.remotePort
          );
  sys.puts("server fd: " + server.fd);
  peer.close();
});
server.listen(8000);
sys.puts("server fd: " + server.fd);
