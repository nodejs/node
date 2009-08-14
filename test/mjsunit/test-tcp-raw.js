include("mjsunit.js");
PORT = 23123;

var echoServer = node.tcp.createServer(function (connection) {
  connection.addListener("receive", function (chunk) {
    connection.send(chunk, "raw");
  });
  connection.addListener("eof", function () {
    connection.close();
  });
});
echoServer.listen(PORT);

var recv = [];
var j = 0;

function onLoad () {
  var c = node.tcp.createConnection(PORT);

  c.addListener("receive", function (chunk) {
    if (++j < 256) {
      c.send([j], "raw");
    } else {
      c.close();
    }
    for (var i = 0; i < chunk.length; i++) {
      recv.push(chunk[i]);
    }
  });

  c.addListener("connect", function () {
    c.send([j], "raw");
  });

  c.addListener("close", function () {
    p(recv);
    echoServer.close();
  });
};

function onExit () {
  var expected = [];
  for (var i = 0; i < 256; i++) {
    expected.push(i);
  }
  assertEquals(expected, recv);
}
