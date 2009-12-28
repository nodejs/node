process.Buffer.prototype.toString = function () {
  return this.utf8Slice(0, this.length);
};

var sys = require("sys");
var net = require("./lib/net");

var server = new net.Server(function (socket) {
  sys.puts("connection (" + socket.fd + "): " 
          + socket.remoteAddress 
          + " port " 
          + socket.remotePort
          );
  sys.puts("server fd: " + server.fd);

  socket.addListener("data", function (b) {
    socket.send("pong ascii\r\n", "ascii");
    socket.send(b);
    socket.send("pong utf8\r\n", "utf8");
  });

  socket.addListener("eof", function () {
    sys.puts("server peer eof");
    socket.close();
  });

  socket.addListener('drain', function () {
    sys.puts("server-side socket drain");
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

c.addListener('data', function (d) {
  sys.puts("!!!client got: " + JSON.stringify(d.toString()));
  c.close();
});

c.addListener('dataEnd', function (d) {
  sys.puts("!!!client eof");
});
