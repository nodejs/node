'use strict';

const common = require('../common');
const assert = require('assert');

process.on('exit', () => {
  assert.strictEqual(process._exiting, true);

  process.nextTick(common.mustNotCall());
});

process.exit();
