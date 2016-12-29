'use strict';

require('../common');
const assert = require('assert');
const Buffer = require('buffer').Buffer;

assert.throws(
  () => { return new Buffer(42, 'utf8'); },
  /first argument must be a string/
);
