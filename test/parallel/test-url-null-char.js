'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  () => { new URL('a\0b'); },
  { code: 'ERR_INVALID_URL', input: 'a\0b' }
);
