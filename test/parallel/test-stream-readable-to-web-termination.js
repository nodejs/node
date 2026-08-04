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
