'use strict';
// Flags: --expose-internals
require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

assert.throws(
  function() {
    process.binding('test');
  },
  /No such module: test/
);

internalBinding('buffer');
