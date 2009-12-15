var socket = process.socket;
var bind = process.bind;
var listen = process.listen;
var accept = process.accept;
var close = process.close;

var Server = function (listener) {
  var self = this;

  if (listener) {
    self.addListener("connection", listener);
  }
  
};
process.inherits(Server, process.EventEmitter);

Server.prototype.listen = function (port, host) {
  var self = this;

  if (self.fd) throw new Error("Already running");

  self.fd = process.socket("TCP");
  // TODO dns resolution
  bind(self.fd, port, host);
  listen(self.fd, 128); // TODO configurable backlog

  self.watcher = new process.IOWatcher(self.fd, true, false, function () {
    var peerInfo;
    while (self.fd) {
      peerInfo = accept(self.fd);
      if (peerInfo === null) return;
      self.emit("connection", peerInfo);      
    }
  });

  self.watcher.start();
};

Server.prototype.close = function () {
  var self = this;
  if (!self.fd) throw new Error("Not running");
  self.watcher.stop();
  close(self.fd);
  this.watcher = null;
  this.fd = null;
};

///////////////////////////////////////////////////////

var sys = require("sys");
var server = new Server(function () {
  sys.puts("connection");
  server.close();
});
server.listen(8000);
