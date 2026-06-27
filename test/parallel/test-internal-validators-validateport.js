// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { validatePort } = require('internal/validators');

for (let n = 0; n <= 0xFFFF; n++) {
  validatePort(n);
  validatePort(`${n}`);
  validatePort(`0x${n.toString(16)}`);
  validatePort(`0o${n.toString(8)}`);
  validatePort(`0b${n.toString(2)}`);
}

[
  -1, 'a', {}, [], false, true,
  0xFFFF + 1, Infinity, -Infinity, NaN,
  undefined, null, '', ' ', 1.1, '0x',
  '-0x1', '-0o1', '-0b1', '0o', '0b',
].forEach((i) => assert.throws(() => validatePort(i), {
  code: 'ERR_SOCKET_BAD_PORT'
}));

// When allowZero is false, every form of zero must be rejected, including
// the string forms that coerce to 0. Refs: the zero check must coerce the
// value the same way the rest of the validation does (`+port`).
[
  0, '0', ' 0 ', '00', '0x0', '0o0', '0b0',
].forEach((i) => assert.throws(() => validatePort(i, 'Port', false), {
  code: 'ERR_SOCKET_BAD_PORT'
}));

// With allowZero left at its default (true), those same values are accepted.
[
  0, '0', ' 0 ', '00', '0x0', '0o0', '0b0',
].forEach((i) => assert.strictEqual(validatePort(i), 0));
