'use strict';

require('../common');
const assert = require('assert');

for (const fill of [
  '',
  [],
  Buffer.from(''),
  new Uint8Array(0),
  { toString: () => '' },
  { toString: () => '', length: 10 }
]) {
  for (let i = 0; i < 50; i++) {
    const buf = Buffer.alloc(100, fill);
    assert.strictEqual(buf.length, 100);
    for (let n = 0; n < buf.length; n++)
      assert.strictEqual(buf[n], 0);
  }
}
