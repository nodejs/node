var assert = require('assert');

process.on('message', function(m, server) {
  console.log('CHILD got message:', m);
  assert.ok(m.hello);
  assert.ok(server);
  process.send({ gotHandle: true });
});

