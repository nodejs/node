// Flags: --experimental-worker
'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');
const { MessageChannel } = require('worker_threads');

const { port1, port2 } = new MessageChannel();

assert.throws(common.mustCall(() => {
  port1.postMessage(null, [port1]);
}), common.mustCall((err) => {
  assert.strictEqual(err.name, 'DataCloneError');
  assert.strictEqual(err.message, 'Transfer list contains source port');
  assert.strictEqual(err.code, 25);
  assert.ok(err instanceof Error);

  const DOMException = err.constructor;
  assert.ok(err instanceof DOMException);
  assert.strictEqual(DOMException.name, 'DOMException');

  return true;
}));

// The failed transfer should not affect the ports in anyway.
port2.onmessage = common.mustCall((message) => {
  assert.strictEqual(message, 2);

  assert(util.inspect(port1).includes('active: true'), util.inspect(port1));
  assert(util.inspect(port2).includes('active: true'), util.inspect(port2));

  port1.close();

  tick(10, () => {
    assert(util.inspect(port1).includes('active: false'), util.inspect(port1));
    assert(util.inspect(port2).includes('active: false'), util.inspect(port2));
  });
});
port1.postMessage(2);

function tick(n, cb) {
  if (n > 0)
    setImmediate(() => tick(n - 1, cb));
  else
    cb();
}
