'use strict';
require('../common');
const assert = require('assert');

// calling .exit() from within "exit" should not overflow the call stack
let nexits = 0;

process.on('exit', function(code) {
  assert.equal(nexits++, 0);
  assert.equal(code, 0);
  process.exit();
});

// "exit" should be emitted unprovoked
