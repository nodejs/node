'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  function() {
    process.binding('test');
  },
  /No such module: test/
);

process.binding('buffer');
