'use strict';
require('../common');
const assert = require('assert');

assert.doesNotThrow(function() {
  Buffer.allocUnsafe(10);
});

assert.throws(function() {
  Buffer.from(10, 'hex');
}, /^TypeError: "value" argument must not be a number$/);

assert.doesNotThrow(function() {
  Buffer.from('deadbeaf', 'hex');
});
