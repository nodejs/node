'use strict';
require('../common');
const assert = require('assert');

const zero = [];
const one = [ Buffer.from('asdf') ];
const long = [];
for (var i = 0; i < 10; i++) long.push(Buffer.from('asdf'));

const flatZero = Buffer.concat(zero);
const flatOne = Buffer.concat(one);
const flatLong = Buffer.concat(long);
const flatLongLen = Buffer.concat(long, 40);

assert.strictEqual(flatZero.length, 0);
assert.strictEqual(flatOne.toString(), 'asdf');

const check = new Array(10 + 1).join('asdf');

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
           err.message === '"list" argument must be an Array of Buffers';
  });
}
