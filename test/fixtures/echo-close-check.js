var common = require('../common');
var assert = require('assert');
var net = require('net');

process.stdout.write('hello world\r\n');

var stdin = process.openStdin();

stdin.on('data', function(data) {
  process.stdout.write(data.toString());
});

stdin.on('end', function() {
  // If stdin's fd will be closed - createServer may get it
  var server = net.createServer(function() {
  }).listen(common.PORT, function() {
    assert(typeof server._handle.fd !== 'number' || server._handle.fd > 2);
    server.close();
  });
});
