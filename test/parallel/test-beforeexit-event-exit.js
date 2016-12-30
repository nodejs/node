'use strict';
require('../common');
const assert = require('assert');

process.on('beforeExit', function() {
  assert(false, 'exit should not allow this to occur');
});

process.exit();
