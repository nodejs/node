'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const kMinPort = 1024;
const kMaxPort = 65535;

function check(value, expected) {
  process.debugPort = value;
  assert.strictEqual(process.debugPort, expected);
}

function checkError(value, expected) {
  const previous = process.debugPort;
  assert.throws(() => {
    process.debugPort = value;
  }, expected);
  assert.strictEqual(process.debugPort, previous);
}

// Expected usage with numbers.
check(0, 0);
check(-0, 0);
check(kMinPort, kMinPort);
check(kMinPort + 1, kMinPort + 1);
check(kMaxPort - 1, kMaxPort - 1);
check(kMaxPort, kMaxPort);

// Values that are not safe integers do not coerce.
[
  '',
  `${kMinPort}`,
  false,
  true,
  null,
  undefined,
  1n,
  Symbol(),
  function() {},
  {},
  { valueOf: common.mustNotCall() },
  [],
  new Number(kMinPort),
].forEach((value) => {
  checkError(value, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'process.debugPort must be 0 or in range 1024 to 65535',
  });
});

// Non-finite and fractional numbers are not safe integers.
[
  NaN,
  Infinity,
  -Infinity,
  -0.5,
  kMinPort + 0.5,
  kMaxPort + 0.5,
].forEach((value) => {
  checkError(value, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'process.debugPort must be 0 or in range 1024 to 65535',
  });
});

// Verify port bounds checking.
[
  -1,
  1,
  kMinPort - 1,
  kMaxPort + 1,
  2 ** 32,
  2 ** 32 + kMinPort,
  -(2 ** 32) + kMinPort,
].forEach((value) => {
  checkError(value, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'process.debugPort must be 0 or in range 1024 to 65535',
  });
});
