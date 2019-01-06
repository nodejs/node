'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');
const { MessageChannel } = require('worker_threads');
const tick = require('../common/tick');

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

  const inspectedPort1 = util.inspect(port1);
  const inspectedPort2 = util.inspect(port2);
  assert(inspectedPort1.includes('active: true'), inspectedPort1);
  assert(inspectedPort2.includes('active: true'), inspectedPort2);

  port1.close();

  tick(10, () => {
    const inspectedPort1 = util.inspect(port1);
    const inspectedPort2 = util.inspect(port2);
    assert(inspectedPort1.includes('active: false'), inspectedPort1);
    assert(inspectedPort2.includes('active: false'), inspectedPort2);
  });
});
port1.postMessage(2);
