var assert = require('assert');
var net = require('net');

var connections = 0;

process.on('message', function(m, serverHandle) {
  console.log('CHILD got message:', m);
  assert.ok(m.hello);

  assert.ok(serverHandle);

  var server = new net.Server(function(c) {
    connections++;
    console.log('CHILD got connection');
    c.destroy();
    process.send({ childConnections: connections });
  });

  server.listen(serverHandle, /* backlog */ 9, function() {
    process.send({ gotHandle: true });
  });
});

