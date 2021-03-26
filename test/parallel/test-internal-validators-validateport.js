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
