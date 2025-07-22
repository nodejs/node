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
    constructor: DOMException,
    name: 'DataCloneError',
    code: 25,
  });

  // Check that transferring FileHandle instances works.
  assert.notStrictEqual(fh.fd, -1);
  port1.postMessage(fh, [ fh ]);
  assert.strictEqual(fh.fd, -1);

  const [ fh2 ] = await once(port2, 'message');
  assert.strictEqual(Object.getPrototypeOf(fh2), Object.getPrototypeOf(fh));

  assert.deepStrictEqual(await fh2.readFile(), await fs.readFile(__filename));
  await fh2.close();

  await assert.rejects(() => fh.readFile(), { code: 'EBADF' });
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
  port2moved.onmessageerror = common.mustCall((event) => {
    assert.strictEqual(event.data.code,
                       'ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE');
  });
  port2moved.start();

  assert.notStrictEqual(fh.fd, -1);
  port1.postMessage(fh, [ fh ]);
  assert.strictEqual(fh.fd, -1);

  port1.postMessage('second message');
  await assert.rejects(() => fh.read(), {
    code: 'EBADF',
    message: 'The FileHandle has been transferred',
    syscall: 'read'
  });
})().then(common.mustCall());

(async function() {
  // Check that a FileHandle with a read in progress cannot be transferred.
  const fh = await fs.open(__filename);

  const { port1 } = new MessageChannel();

  const readPromise = fh.readFile();
  assert.throws(() => {
    port1.postMessage(fh, [fh]);
  }, {
    message: 'Cannot transfer FileHandle while in use',
    name: 'DataCloneError'
  });

  assert.deepStrictEqual(await readPromise, await fs.readFile(__filename));
})().then(common.mustCall());

(async function() {
  // Check that filehandles with a close in progress cannot be transferred.
  const fh = await fs.open(__filename);

  const { port1 } = new MessageChannel();

  const closePromise = fh.close();
  assert.throws(() => {
    port1.postMessage(fh, [fh]);
  }, {
    message: 'Cannot transfer FileHandle while in use',
    name: 'DataCloneError'
  });
  await closePromise;
})().then(common.mustCall());
