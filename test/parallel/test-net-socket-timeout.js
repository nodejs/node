var common = require('../common');
var net = require('net');
var assert = require('assert');

var timedout = false;

var server = net.Server();
server.listen(common.PORT, function() {
  var socket = net.createConnection(common.PORT);
  socket.setTimeout(100, function() {
    timedout = true;
    socket.destroy();
    server.close();
    clearTimeout(timer);
  });
  var timer = setTimeout(function() {
    process.exit(1);
  }, 200);
});

process.on('exit', function() {
  assert.ok(timedout);
});
