var common = require('../common');
net = require('net');
assert = require('assert');

var address;

var server = net.createServer(function (socket) {
});

server.listen(function() {
  address = server.address();
  console.log("opened server on %j", address);
  server.close();
});

process.on('exit', function () {
  assert.ok(address.port > 100);
});

