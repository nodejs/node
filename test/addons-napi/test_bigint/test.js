'use strict';
const common = require('../../common');
const assert = require('assert');
const {
  TestInt64,
  TestUint64,
  TestWords,
} = require(`./build/${common.buildType}/test_bigint`);

[
  0n,
  -0n,
  1n,
  -1n,
  100n,
  2121n,
  -1233n,
  986583n,
  -976675n,
  98765432213456789876546896323445679887645323232436587988766545658n,
  -4350987086545760976737453646576078997096876957864353245245769809n,
].forEach((num) => {
  if (num > -(2n ** 63n) && num < 2n ** 63n) {
    assert.strictEqual(num, TestInt64(num));
  }
  if (num >= 0 && num < 2n ** 64n) {
    assert.strictEqual(num, TestUint64(num));
  }
  assert.strictEqual(num, TestWords(num));
});
