'use strict';

const common = require('../common');
const assert = require('assert');

process.on('exit', () => {
  // process._exiting was not set!
  assert.strictEqual(process._exiting, true);

  process.nextTick(common.mustNotCall());
});

process.exit();
