'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  () => { new URL('a\0b'); },
  { input: 'a\0b' }
);
