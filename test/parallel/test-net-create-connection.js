var common = require('../common');
var assert = require('assert');
var net = require('net');

var tcpPort = common.PORT;
var clientConnected = 0;
var serverConnected = 0;

var server = net.createServer(function(socket) {
  socket.end();
  if (++serverConnected === 4) {
    server.close();
  }
});
server.listen(tcpPort, 'localhost', function() {
  function cb() {
    ++clientConnected;
  }

  net.createConnection(tcpPort).on('connect', cb);
  net.createConnection(tcpPort, 'localhost').on('connect', cb);
  net.createConnection(tcpPort, cb);
  net.createConnection(tcpPort, 'localhost', cb);

  assert.throws(function () {
    net.createConnection({
      port: 'invalid!'
    }, cb);
  });
});

process.on('exit', function() {
  assert.equal(clientConnected, 4);
});

