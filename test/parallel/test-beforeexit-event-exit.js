'use strict';
require('../common');
const assert = require('assert');

process.on('beforeExit', function() {
  assert.fail('exit should not allow this to occur');
});

process.exit();
