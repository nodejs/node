var assert = require('assert');
var net = require('net');
var common = require('../common');

var server = net.createServer();

//test unix sockets
var fds = process.binding('net').socketpair();
var unixsocket = new net.Socket(fds[0]);
assert.ok(unixsocket.type == 'unix', 'Should be UNIX');

//test that stdin is default file
assert.ok(process.stdin.type == 'file', 'Should be File');

//test tcp sockets.
server.listen(function() {
  var client = net.createConnection(this.address().port);
  client.on('connect', function() {
    var newStream = new net.Socket(client.fd);
    assert.ok(newStream.type == 'tcp', 'Should be TCP');
    client.destroy();
    server.close();
  });
});
