'use strict';
require('../common');
var assert = require('assert');
var os = require('os');

process.on('beforeExit', function() {
  assert(false, 'exit should not allow this to occur');
});

os._exit();
