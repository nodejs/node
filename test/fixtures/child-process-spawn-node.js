const assert = require('assert');
const debug = require('util').debuglog('test');

function onmessage(m) {
  debug('CHILD got message:', m);
  assert.ok(m.hello);
  process.removeListener('message', onmessage);
}

process.on('message', onmessage);
process.send({ foo: 'bar' });
