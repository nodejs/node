'use strict';
const common = require('../../common');
const assert = require('assert');
const cp = require('child_process');
const debuglog = require('util').debuglog('test');

// This test ensures that during resizing of an Aliased*Array the computation
// of the new size does not overflow.

if (process.argv[2] === 'child') {
  // test
  const binding = require(`./build/${common.buildType}/binding`);

  const bigValue = BigInt('0xE0000000E0000000');
  binding.allocateAndResizeBuffer(bigValue);
  assert.fail('this should be unreachable');
} else {
  // observer
  const child = cp.spawn(`${process.execPath}`, [`${__filename}`, 'child']);
  let stderr = '';
  let stdout = '';
  child.stderr.setEncoding('utf8');
  child.stdout.setEncoding('utf8');
  child.stderr.on('data', (data) => stderr += data);
  child.stdout.on('data', (data) => stdout += data);
  child.on('exit', common.mustCall(function(code, signal) {
    debuglog(`exit with code ${code}, signal ${signal}`);
    debuglog(stdout);
    debuglog(stderr);
    assert.ok(common.nodeProcessAborted(code, signal));
  }));
}
