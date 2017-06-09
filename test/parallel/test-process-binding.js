'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  function() {
    process.binding('test');
  },
  /No such module: test/
);

assert.doesNotThrow(function() {
  process.binding('buffer');
}, function(err) {
  if (err instanceof Error) {
    return true;
  }
}, 'unexpected error');
