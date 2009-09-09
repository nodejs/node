include("mjsunit.js");
PORT = 23123;

binaryString = "";
for (var i = 0; i < 256; i++) {
  var j = 255 - i;
  var s = "'\\" + j.toString(8) + "'";
  S = eval(s);
  puts(s + " " + JSON.stringify(S) + " " + S.charCodeAt(0));
  node.assert(S.charCodeAt(0) == j);
  binaryString += S;
}

var echoServer = node.tcp.createServer(function (connection) {
  connection.setEncoding("raws");
  connection.addListener("receive", function (chunk) {
    puts("recved: " + JSON.stringify(chunk));
    connection.send(chunk, "raws");
  });
  connection.addListener("eof", function () {
    connection.close();
  });
});
echoServer.listen(PORT);

var recv = "";
var j = 0;

var c = node.tcp.createConnection(PORT);

c.setEncoding("raws");
c.addListener("receive", function (chunk) {
  if (j++ < 256) {
    c.send([j]);
  } else {
    c.close();
  }
  recv += chunk;
});

c.addListener("connect", function () {
  c.send(binaryString, "raws");
});

c.addListener("close", function () {
  p(recv);
  echoServer.close();
});

process.addListener("exit", function () {
  assertEquals(2*256, recv.length);
  for (var i = 0; i < 256; i++) {
    assertEquals(i, recv.charCodeAt(255+i));
    assertEquals(i, recv.charCodeAt(255-i));
  }
});
