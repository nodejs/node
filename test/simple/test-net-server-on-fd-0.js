var common = require('../common');
var assert = require('assert');
var net = require('net');

process.stdin.destroy();

var accepted = null;
var server = net.createServer(function(socket) {
  console.log('accepted');
  accepted = socket;
  socket.end();
  server.close();
});


server.listen(common.PORT, function() {
  console.log('listening...');
  assert.equal(server.fd, 0);

  net.createConnection(common.PORT);
});


process.on('exit', function() {
  assert.ok(accepted);
});

