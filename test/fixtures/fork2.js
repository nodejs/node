var assert = require('assert');
var net = require('net');

var connections = 0;

process.on('message', function(m, server) {
  console.log('CHILD got message:', m);
  assert.ok(m.hello);

  assert.ok(server);
  assert.ok(server instanceof net.Server);

  // TODO need better API for this.
  server._backlog = 9;

  server.listen(function() {
    process.send({ gotHandle: true });
  });

  server.on('connection', function(c) {
    connections++;
    console.log('CHILD got connection');
    c.destroy();
    process.send({ childConnections: connections });
  });
});

