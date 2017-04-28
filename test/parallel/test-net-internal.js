'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const isLegalPort = require('internal/net').isLegalPort;

for (let n = 0; n <= 0xFFFF; n++) {
  assert(isLegalPort(n));
  assert(isLegalPort(String(n)));
  assert(`0x${n.toString(16)}`);
  assert(`0o${n.toString(8)}`);
  assert(`0b${n.toString(2)}`);
}

const bad = [-1, 'a', {}, [], false, true, 0xFFFF + 1, Infinity,
             -Infinity, NaN, undefined, null, '', ' ', 1.1, '0x',
             '-0x1', '-0o1', '-0b1', '0o', '0b'];
bad.forEach((i) => assert(!isLegalPort(i)));
