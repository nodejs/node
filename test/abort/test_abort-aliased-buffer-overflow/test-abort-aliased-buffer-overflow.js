'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// This test ensures that during resizing of an Aliased*Array the computation
// of the new size does not overflow.

if (process.argv[2] === 'child') {
  // test
  const binding = require(`./build/${common.buildType}/binding`);

  const bigValue = BigInt('0xE000 0000 E000 0000');
  binding.AllocateAndResizeBuffer(bigValue);
  assert.fail('this should be unreachable');
} else {
  // observer
  const child = cp.spawn(`${process.execPath}`, [`${__filename}`, 'child']);
  child.on('exit', common.mustCall(function(code, signal) {
    if (common.isWindows) {
      assert.strictEqual(code, 134);
      assert.strictEqual(signal, null);
    } else {
      assert.strictEqual(code, null);
      assert.strictEqual(signal, 'SIGABRT');
    }
  }));
}
