common = require("../common");
assert = common.assert
tcp = require("tcp");

binaryString = "";
for (var i = 255; i >= 0; i--) {
  var s = "'\\" + i.toString(8) + "'";
  S = eval(s);
  common.error( s
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
  connection.addListener("data", function (chunk) {
    common.error("recved: " + JSON.stringify(chunk));
    connection.write(chunk, "binary");
  });
  connection.addListener("end", function () {
    connection.end();
  });
});
echoServer.listen(common.PORT);

var recv = "";
var j = 0;

var c = tcp.createConnection(common.PORT);

c.setEncoding("binary");
c.addListener("data", function (chunk) {
  if (j < 256) {
    common.error("write " + j);
    c.write(String.fromCharCode(j), "binary");
    j++;
  } else {
    c.end();
  }
  recv += chunk;
});

c.addListener("connect", function () {
  c.write(binaryString, "binary");
});

c.addListener("close", function () {
  p(recv);
  echoServer.close();
});

process.addListener("exit", function () {
  console.log("recv: " + JSON.stringify(recv));

  assert.equal(2*256, recv.length);

  var a = recv.split("");

  var first = a.slice(0,256).reverse().join("");
  console.log("first: " + JSON.stringify(first));

  var second = a.slice(256,2*256).join("");
  console.log("second: " + JSON.stringify(second));

  assert.equal(first, second);
});
