'use strict';
const common = require('../common');
const assert = require('assert');

const zero = [];
const one = [ Buffer.from('asdf') ];
const long = [];
for (let i = 0; i < 10; i++) long.push(Buffer.from('asdf'));

const flatZero = Buffer.concat(zero);
const flatOne = Buffer.concat(one);
const flatLong = Buffer.concat(long);
const flatLongLen = Buffer.concat(long, 40);

assert.strictEqual(flatZero.length, 0);
assert.strictEqual(flatOne.toString(), 'asdf');

const check = 'asdf'.repeat(10);

// A special case where concat used to return the first item,
// if the length is one. This check is to make sure that we don't do that.
assert.notStrictEqual(flatOne, one[0]);
assert.strictEqual(flatLong.toString(), check);
assert.strictEqual(flatLongLen.toString(), check);

assertWrongList();
assertWrongList(null);
assertWrongList(Buffer.from('hello'));
assertWrongList([42]);
assertWrongList(['hello', 'world']);
assertWrongList(['hello', Buffer.from('world')]);

function assertWrongList(value) {
  assert.throws(() => {
    Buffer.concat(value);
  }, function(err) {
    return err instanceof TypeError &&
           err.message === '"list" argument must be an Array of Buffer ' +
                           'or Uint8Array instances';
  });
}

const random10 = common.hasCrypto ?
  require('crypto').randomBytes(10) :
  Buffer.alloc(10, 1);
const empty = Buffer.alloc(0);

assert.notDeepStrictEqual(random10, empty);
assert.notDeepStrictEqual(random10, Buffer.alloc(10));

assert.deepStrictEqual(Buffer.concat([], 100), empty);
assert.deepStrictEqual(Buffer.concat([random10], 0), empty);
assert.deepStrictEqual(Buffer.concat([random10], 10), random10);
assert.deepStrictEqual(Buffer.concat([random10, random10], 10), random10);
assert.deepStrictEqual(Buffer.concat([empty, random10]), random10);
assert.deepStrictEqual(Buffer.concat([random10, empty, empty]), random10);

// The tail should be zero-filled
assert.deepStrictEqual(Buffer.concat([empty], 100), Buffer.alloc(100));
assert.deepStrictEqual(Buffer.concat([empty], 4096), Buffer.alloc(4096));
assert.deepStrictEqual(
    Buffer.concat([random10], 40),
    Buffer.concat([random10, Buffer.alloc(30)]));

assert.deepStrictEqual(Buffer.concat([new Uint8Array([0x41, 0x42]),
                                      new Uint8Array([0x43, 0x44])]),
                       Buffer.from('ABCD'));
