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

  stream.addListener('drain', function () {
    sys.puts("server-side socket drain");
  });

  stream.addListener("eof", function () {
    sys.puts("server peer eof");
    stream.close();
  });
});
server.listen(8000);
sys.puts("server fd: " + server.fd);


var c = net.createConnection(8000);
c.addListener('connect', function () {
  sys.puts("!!!client connected");
  c.send("hello\n");
});

c.addListener('drain', function () {
  sys.puts("!!!client drain");
});

c.addListener('receive', function (d) {
  sys.puts("!!!client got: " + JSON.stringify(d.toString()));
});

