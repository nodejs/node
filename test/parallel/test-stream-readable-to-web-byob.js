'use strict';
require('../common');
const { Readable } = require('stream');
const assert = require('assert');
const common = require('../common');

let count = 0;

const nodeStream = new Readable({
  read(size) {
    if (this.destroyed) {
      return;
    }
    // Simulate a stream that pushes sequences of 16 bytes
    const buffer = Buffer.alloc(size);
    for (let i = 0; i < size; i++) {
      buffer[i] = count++ % 16;
    }
    this.push(buffer);
  }
});

// Test validation of 'type' option
assert.throws(
  () => {
    Readable.toWeb(nodeStream, { type: 'wrong type' });
  },
  {
    code: 'ERR_INVALID_ARG_VALUE'
  }
);

// Test normal operation with ReadableByteStream
const webStream = Readable.toWeb(nodeStream, { type: 'bytes' });
const reader = webStream.getReader({ mode: 'byob' });
const expected = Buffer.alloc(16);
for (let i = 0; i < 16; i++) {
  expected[i] = count++;
}

for (let i = 0; i < 1000; i++) {
  // Read 16 bytes of data from the stream
  const receive = new Uint8Array(16);
  reader.read(receive).then(common.mustCall((result) => {
    // Verify the data received
    assert.ok(!result.done);
    assert.deepStrictEqual(receive, expected);
  }));
}
