'use strict';
const common = require('../common');
const assert = require('assert');
const { Duplex, Readable } = require('stream');
const { setTimeout: delay } = require('timers/promises');

{
  const r = Readable.from([]);
  // Cancelling reader while closing should not cause uncaught exceptions
  r.on('close', () => reader.cancel());

  const reader = Readable.toWeb(r).getReader();
  reader.read();
}

{
  const duplex = new Duplex({
    read() {
      this.push(Buffer.from('x'));
      this.push(null);
    },
    write(_chunk, _encoding, callback) {
      callback();
    },
  });

  const reader = Readable.toWeb(duplex).getReader();

  (async () => {
    const result = await reader.read();
    assert.deepStrictEqual(result, {
      value: new Uint8Array(Buffer.from('x')),
      done: false,
    });

    const closeResult = await Promise.race([
      reader.read(),
      delay(common.platformTimeout(100)).then(() => 'timeout'),
    ]);

    assert.notStrictEqual(closeResult, 'timeout');
    assert.deepStrictEqual(closeResult, { value: undefined, done: true });
  })().then(common.mustCall());
}

// Cancelling a web ReadableStream while the underlying Readable is actively
// producing data should not throw ERR_INVALID_STATE. The 'data' handler in
// newReadableStreamFromStreamReadable must check wasCanceled before calling
// controller.enqueue(). See: https://github.com/nodejs/node/issues/54205
{
  const readable = new Readable({
    read() {
      this.push(Buffer.alloc(1024));
    },
  });

  const webStream = Readable.toWeb(readable);
  const reader = webStream.getReader();

  (async () => {
    await reader.read();
    await reader.read();
    reader.releaseLock();
    await webStream.cancel();
  })().then(common.mustCall());
}
