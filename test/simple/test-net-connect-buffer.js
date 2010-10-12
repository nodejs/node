var common = require('../common');
var assert = require('assert');
var net = require('net');

var tcpPort = common.PORT;

var tcp = net.Server(function (s) {
  tcp.close();

  console.log("tcp server connection");

  var buf = "";
  s.on('data', function (d) {
    buf += d;
  });

  s.on('end', function () {
    assert.equal("foobar", buf);
    console.log("tcp socket disconnect");
    s.end();
  });

  s.on('error', function (e) {
    console.log("tcp server-side error: " + e.message);
    process.exit(1);
  });
});
tcp.listen(tcpPort, startClient);

function startClient () {
  var socket = net.Stream();

  console.log("Connecting to socket");

  socket.connect(tcpPort);

  socket.on('connect', function () {
    console.log('socket connected');
  });

  assert.equal("opening", socket.readyState);

  assert.equal(false, socket.write("foo"));
  socket.end("bar");

  assert.equal("opening", socket.readyState);
}
