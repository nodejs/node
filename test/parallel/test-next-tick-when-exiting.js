'use strict';

require('../common');
const assert = require('assert');

process.on('exit', () => {
  assert.strictEqual(process._exiting, true, 'process._exiting was not set!');

  process.nextTick(() => {
    assert.fail('process is exiting, should not be called.');
  });
});

process.exit();
