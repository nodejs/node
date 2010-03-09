require("../common");

tcp = require("tcp");
sys = require("sys");
var x = path.join(fixturesDir, "x.txt");
var expected = "xyz";

var server = tcp.createServer(function (socket) {
  socket.addListener("receive", function (data) {
    found = data;
    client.close();
    socket.close();
    server.close();
    assert.equal(expected, found);
  });
});
server.listen(PORT);

var client = tcp.createConnection(PORT);
client.addListener("connect", function () {
  fs.open(x, 'r').addCallback(function (fd) {
    fs.sendfile(client.fd, fd, 0, expected.length).addCallback(function (size) {
      assert.equal(expected.length, size);
    });
  });
});
