var common = require('../common');
var assert = require('assert');

var net = require('net');
var util = require('util');
var x = path.join(common.fixturesDir, 'x.txt');
var expected = 'xyz';

var server = net.createServer(function(socket) {
  socket.addListener('receive', function(data) {
    found = data;
    client.close();
    socket.close();
    server.close();
    assert.equal(expected, found);
  });
});
server.listen(common.PORT);

var client = net.createConnection(common.PORT);
client.addListener('connect', function() {
  fs.open(x, 'r').addCallback(function(fd) {
    fs.sendfile(client.fd, fd, 0, expected.length)
      .addCallback(function(size) {
          assert.equal(expected.length, size);
        });
  });
});
