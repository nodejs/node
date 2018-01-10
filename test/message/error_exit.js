'use strict';
require('../common');
const assert = require('assert');

process.on('exit', function(code) {
  console.error(`Exiting with code=${code}`);
});

assert.strictEqual(1, 2);
