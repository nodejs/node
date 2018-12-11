'use strict';
// Flags: --expose-internals
require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

assert.throws(
  function() {
    internalBinding('test');
  },
  /No such module: test/
);

internalBinding('buffer');
