process.Buffer.prototype.toString = function () {
  return this.utf8Slice(0, this.length);
};

var sys = require("sys");
var net = require("./lib/net");

var server = new net.Server(function (stream) {
  sys.puts("connection (" + stream.fd + "): " 
          + stream.remoteAddress 
          + " port " 
          + stream.remotePort
          );
  sys.puts("server fd: " + server.fd);

  stream.addListener("receive", function (b) {
    stream.send("pong ascii\r\n", "ascii");
    stream.send(b);
    stream.send("pong utf8\r\n", "utf8");
  });
});
server.listen(8000);
sys.puts("server fd: " + server.fd);
