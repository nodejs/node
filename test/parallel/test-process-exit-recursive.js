'use strict';
require('../common');
const assert = require('assert');

// recursively calling .exit() should not overflow the call stack
let nexits = 0;

process.on('exit', function(code) {
  assert.strictEqual(nexits++, 0);
  assert.strictEqual(code, 1);

  // now override the exit code of 1 with 0 so that the test passes
  process.exit(0);
});

process.exit(1);
