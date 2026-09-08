'use strict';

require('../common');
const assert = require('node:assert');
const { guessFileDescriptorType } = require('os');

assert.strictEqual(guessFileDescriptorType(0), 'TTY');
assert.strictEqual(guessFileDescriptorType(1), 'TTY');
assert.strictEqual(guessFileDescriptorType(2), 'TTY');

assert.strictEqual(guessFileDescriptorType(55555), 'UNKNOWN');
assert.strictEqual(guessFileDescriptorType(2 ** 31 - 1), 'UNKNOWN');

[
  -1,
  1.1,
  '1',
  [],
  {},
  () => {},
  2 ** 31,
  true,
  false,
  1n,
  Symbol(),
  undefined,
  null,
].forEach((val) => assert.throws(() => guessFileDescriptorType(val), { code: 'ERR_INVALID_FD' }));
