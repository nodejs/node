'use strict';

const common = require('../common');
const assert = require('assert');

const b = Buffer.from('abcdf');
const c = Buffer.from('abcdf');
const d = Buffer.from('abcde');
const e = Buffer.from('abcdef');

assert.ok(b.equals(c));
assert.ok(!c.equals(d));
assert.ok(!d.equals(e));
assert.ok(d.equals(d));
assert.ok(d.equals(new Uint8Array([0x61, 0x62, 0x63, 0x64, 0x65])));

common.expectsError(
  () => Buffer.alloc(1).equals('abc'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "otherBuffer" argument must be one of type ' +
    'Buffer or Uint8Array. Received type string'
  }
);
