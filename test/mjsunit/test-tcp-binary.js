process.mixin(require("./common"));
tcp = require("tcp");
PORT = 23123;

binaryString = "";
for (var i = 255; i >= 0; i--) {
  var s = "'\\" + i.toString(8) + "'";
  S = eval(s);
  error( s
       + " "
       + JSON.stringify(S)
       + " "
       + JSON.stringify(String.fromCharCode(i)) 
       + " "
       + S.charCodeAt(0)
       );
  process.assert(S.charCodeAt(0) == i);
  process.assert(S == String.fromCharCode(i));
  binaryString += S;
}

var echoServer = tcp.createServer(function (connection) {
  connection.setEncoding("binary");
  connection.addListener("receive", function (chunk) {
    error("recved: " + JSON.stringify(chunk));
    connection.send(chunk, "binary");
  });
  connection.addListener("eof", function () {
    connection.close();
  });
});
echoServer.listen(PORT);

var recv = "";
var j = 0;

var c = tcp.createConnection(PORT);

c.setEncoding("binary");
c.addListener("receive", function (chunk) {
  if (j < 256) {
    error("send " + j);
    c.send(String.fromCharCode(j), "binary");
    j++;
  } else {
    c.close();
  }
  recv += chunk;
});

c.addListener("connect", function () {
  c.send(binaryString, "binary");
});

c.addListener("close", function () {
  p(recv);
  echoServer.close();
});

process.addListener("exit", function () {
  puts("recv: " + JSON.stringify(recv));

  assert.equal(2*256, recv.length);

  var a = recv.split("");

  var first = a.slice(0,256).reverse().join("");
  puts("first: " + JSON.stringify(first));

  var second = a.slice(256,2*256).join("");
  puts("second: " + JSON.stringify(second));

  assert.equal(first, second);
});
