process.mixin(require("../common"));
net = require("net");

process.Buffer.prototype.toString = function () {
  return this.utf8Slice(0, this.length);
};


path = process.ARGV[2];
greeting = process.ARGV[3];

receiver = net.createServer(function(socket) {
  socket.addListener("fd", function(fd) {
    var peerInfo = process.getpeername(fd);
    peerInfo.fd = fd;
    var passedSocket = new net.Socket(peerInfo);

    passedSocket.addListener("eof", function() {
      passedSocket.close();
    });

    passedSocket.addListener("data", function(data) {
      passedSocket.send("[echo] " + data);
    });
    passedSocket.addListener("close", function() {
      receiver.close();
    });
    passedSocket.send("[greeting] " + greeting);
  });
});

/* To signal the test runne we're up and listening */
receiver.addListener("listening", function() {
  print("ready");
});

receiver.listen(path);
