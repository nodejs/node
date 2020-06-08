'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs').promises;
const vm = require('vm');
const { MessageChannel, moveMessagePortToContext } = require('worker_threads');
const { once } = require('events');

(async function() {
  const fh = await fs.open(__filename);

  const { port1, port2 } = new MessageChannel();

  assert.throws(() => {
    port1.postMessage(fh);
  }, {
    // See the TODO about error code in node_messaging.cc.
    code: 'ERR_MISSING_MESSAGE_PORT_IN_TRANSFER_LIST'
  });

  // Check that transferring FileHandle instances works.
  assert.notStrictEqual(fh.fd, -1);
  port1.postMessage(fh, [ fh ]);
  assert.strictEqual(fh.fd, -1);

  const [ fh2 ] = await once(port2, 'message');
  assert.strictEqual(Object.getPrototypeOf(fh2), Object.getPrototypeOf(fh));

  assert.deepStrictEqual(await fh2.readFile(), await fs.readFile(__filename));
  await fh2.close();

  assert.rejects(() => fh.readFile(), { code: 'EBADF' });
})().then(common.mustCall());

(async function() {
  // Check that there is no crash if the message is never read.
  const fh = await fs.open(__filename);

  const { port1 } = new MessageChannel();

  assert.notStrictEqual(fh.fd, -1);
  port1.postMessage(fh, [ fh ]);
  assert.strictEqual(fh.fd, -1);
})().then(common.mustCall());

(async function() {
  // Check that in the case of a context mismatch the message is discarded.
  const fh = await fs.open(__filename);

  const { port1, port2 } = new MessageChannel();

  const ctx = vm.createContext();
  const port2moved = moveMessagePortToContext(port2, ctx);
  port2moved.onmessage = common.mustCall((msgEvent) => {
    assert.strictEqual(msgEvent.data, 'second message');
    port1.close();
  });
  // TODO(addaleax): Switch this to a 'messageerror' event once MessagePort
  // implements EventTarget fully and in a cross-context manner.
  port2moved.emit = common.mustCall((name, err) => {
    assert.strictEqual(name, 'messageerror');
    assert.strictEqual(err.code, 'ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE');
  });
  port2moved.start();

  assert.notStrictEqual(fh.fd, -1);
  port1.postMessage(fh, [ fh ]);
  assert.strictEqual(fh.fd, -1);

  port1.postMessage('second message');
})().then(common.mustCall());
