'use strict';
require('../common');
var assert = require('assert');

process.on('exit', function(code) {
  console.error('Exiting with code=%d', code);
});

assert.equal(1, 2);
