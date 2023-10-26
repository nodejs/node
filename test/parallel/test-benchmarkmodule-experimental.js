'use strict';

const common = require('../common');
const assert = require('assert');

require('node:benchmark');

// This test ensures that the experimental message is emitted
// when using permission system
process.on('warning', common.mustCall((warning) => {
  assert.match(warning.message, /The benchmark module is an experimental feature/);
}, 1));
