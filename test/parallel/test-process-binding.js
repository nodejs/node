'use strict';
// Flags: --expose_internals
require('../common');
var assert = require('assert');

assert.throws(
  function() {
    require('binding/test');
  },
  /No such module: test/
);

assert.doesNotThrow(function() {
  require('binding/buffer');
}, function(err) {
  if ( (err instanceof Error) ) {
    return true;
  }
}, 'unexpected error');
