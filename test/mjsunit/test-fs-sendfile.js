process.mixin(require("common.js"));

tcp = require("/tcp.js");
sys = require("/sys.js");
PORT = 23123;
var x = process.path.join(fixturesDir, "x.txt");
var expected = "xyz";

var server = tcp.createServer(function (socket) {
  socket.addListener("receive", function (data) {
    found = data;
    client.close();
    socket.close();
    server.close();
    assertEquals(expected, found);
  });
});
server.listen(PORT);

var client = tcp.createConnection(PORT);
client.addListener("connect", function () {
  posix.open(x,process.O_RDONLY, 0666).addCallback(function (fd) {
    posix.sendfile(client.fd, fd, 0, expected.length).addCallback(function (size) {
      assertEquals(expected.length, size);
    });
  });
});
