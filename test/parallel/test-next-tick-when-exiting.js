'use strict';

require('../common');
const assert = require('assert');

const isExitingDesc = Object.getOwnPropertyDescriptor(process, 'isExiting');
assert.ok(!('set' in isExitingDesc));
assert.ok('get' in isExitingDesc);
assert.strictEqual(isExitingDesc.configurable, false);
assert.strictEqual(isExitingDesc.enumerable, true);

process.on('exit', () => {
  assert.strictEqual(process.isExiting, true, 'process.isExiting was not set!');

  process.nextTick(() => {
    assert.fail('process is exiting, should not be called.');
  });
});

process.exit();
