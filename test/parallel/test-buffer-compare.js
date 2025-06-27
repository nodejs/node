'use strict';

require('../common');
const assert = require('assert');

const b = Buffer.alloc(1, 'a');
const c = Buffer.alloc(1, 'c');
const d = Buffer.alloc(2, 'aa');
const e = new Uint8Array([ 0x61, 0x61 ]); // ASCII 'aa', same as d

assert.strictEqual(b.compare(c), -1);
assert.strictEqual(c.compare(d), 1);
assert.strictEqual(d.compare(b), 1);
assert.strictEqual(d.compare(e), 0);
assert.strictEqual(b.compare(d), -1);
assert.strictEqual(b.compare(b), 0);

assert.strictEqual(Buffer.compare(b, c), -1);
assert.strictEqual(Buffer.compare(c, d), 1);
assert.strictEqual(Buffer.compare(d, b), 1);
assert.strictEqual(Buffer.compare(b, d), -1);
assert.strictEqual(Buffer.compare(c, c), 0);
assert.strictEqual(Buffer.compare(e, e), 0);
assert.strictEqual(Buffer.compare(d, e), 0);
assert.strictEqual(Buffer.compare(d, b), 1);

assert.strictEqual(Buffer.compare(Buffer.alloc(0), Buffer.alloc(0)), 0);
assert.strictEqual(Buffer.compare(Buffer.alloc(0), Buffer.alloc(1)), -1);
assert.strictEqual(Buffer.compare(Buffer.alloc(1), Buffer.alloc(0)), 1);

assert.throws(() => Buffer.compare(Buffer.alloc(1), 'abc'), {
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "buf2" argument must be an instance of Buffer or Uint8Array. ' +
           "Received type string ('abc')"
});
assert.throws(() => Buffer.compare('abc', Buffer.alloc(1)), {
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "buf1" argument must be an instance of Buffer or Uint8Array. ' +
           "Received type string ('abc')"
});

assert.throws(() => Buffer.alloc(1).compare('abc'), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The "target" argument must be an instance of ' +
           "Buffer or Uint8Array. Received type string ('abc')"
});
