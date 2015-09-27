'use strict';
require('../common');
var assert = require('assert');

process.on('beforeExit', function() {
  assert(false, 'exit should not allow this to occur');
});

process.exit();
