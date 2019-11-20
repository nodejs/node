'use strict';

// This test ensures that the _writeableState.bufferedRequestCount and
// the actual buffered request count are the same.

const common = require('../common');
const Stream = require('stream');
const assert = require('assert');

class StreamWritable extends Stream.Writable {
  constructor() {
    super({ objectMode: true });
  }

  // Refs: https://github.com/nodejs/node/issues/6758
  // We need a timer like on the original issue thread.
  // Otherwise the code will never reach our test case.
  _write(chunk, encoding, cb) {
    setImmediate(cb);
  }
}

const testStream = new StreamWritable();
testStream.cork();

for (let i = 1; i <= 5; i++) {
  testStream.write(i, common.mustCall(() => {
    assert.strictEqual(
      testStream._writableState.bufferedRequestCount,
      testStream._writableState.getBuffer().length
    );
  }));
}

testStream.end();
