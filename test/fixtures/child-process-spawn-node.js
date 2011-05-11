var assert = require('assert');

process.on('message', function(m) {
  console.log('CHILD got message:', m);
  assert.ok(m.hello);
  // Note that we have to force exit.
  process.exit();
});

process.send({ foo: 'bar' });
