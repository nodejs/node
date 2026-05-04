'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createHmac } = require('crypto');

common.expectWarning(
  'DeprecationWarning',
  'Calling digest() on an already-finalized Hmac instance is deprecated.',
  'DEP0200',
);

const hmac = createHmac('sha256', 'key').update('data');
hmac.digest();
const second = hmac.digest();
assert.deepStrictEqual(second, Buffer.from(''));