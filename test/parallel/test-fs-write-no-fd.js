'use strict';

require('../common');
const fs = require('fs');
const assert = require('assert');

assert.throws(function() {
  fs.write(null, Buffer.allocUnsafe(1), 0, 1, () => {});
}, /TypeError: file descriptor must be a unsigned 32-bit integer/);

assert.throws(function() {
  fs.write(undefined, '1', 0, 1, () => {});
}, /TypeError: file descriptor must be a unsigned 32-bit integer/);
