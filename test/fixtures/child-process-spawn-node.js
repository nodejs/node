var assert = require('assert');

function onmessage(m) {
  console.log('CHILD got message:', m);
  assert.ok(m.hello);
  process.removeListener('message', onmessage);
}

process.on('message', onmessage);
process.send({ foo: 'bar' });
