node.mixin(require("common.js"));

tcp = require("/tcp.js");
sys = require("/sys.js");
PORT = 23123;
var x = node.path.join(fixturesDir, "x.txt");
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
  node.fs.open(x,node.O_RDONLY, 0666).addCallback(function (fd) {
    node.fs.sendfile(client.fd, fd, 0, expected.length).addCallback(function (size) {
      assertEquals(expected.length, size);
    });
  });
});
