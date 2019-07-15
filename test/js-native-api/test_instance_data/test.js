'use strict';
// Flags: --expose-gc

const common = require('../../common');

// Testing api calls for instance data
const test_instance_data =
  require(`./build/${common.buildType}/test_instance_data`);

if (process.argv[2] === 'child') {
  test_instance_data.setPrintOnDelete();
  return;
}

const { spawnSync } = require('child_process');
const assert = require('assert');

// Test that instance data can be accessed from a binding.
assert.strictEqual(test_instance_data.increment(), 42);

// Test that the instance data finalizer gets called.
assert.strictEqual(
  (spawnSync(process.execPath, [__filename, 'child'], {
    stdio: ['inherit', 'pipe', 'inherit']
  }).stdout
    .toString()
    .split(/\r\n?|\n/) || [])[0],
  'deleting addon data');

// Test that the instance data can be accessed from a finalizer.
test_instance_data.objectWithFinalizer(common.mustCall());
global.gc();
