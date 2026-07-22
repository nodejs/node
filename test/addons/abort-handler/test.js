'use strict';
const common = require('../../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const bindingPath = path.resolve(
  __dirname, 'build', common.buildType, 'binding.node');

if (!fs.existsSync(bindingPath))
  common.skip('binding not built yet');

if (process.argv[2] === 'child') {
  const binding = require(bindingPath);
  binding.installAbortHandler();
  process.abort();
  return;
}

// We do not want to generate core files / actually crash the process when
// running this test as a regular addon test. It is also required as an
// abort test with ALLOW_CRASHES set (see ../../abort/test-addon-abort-handler.js).
if (!process.env.ALLOW_CRASHES)
  common.skip('test needs ALLOW_CRASHES to spawn a crashing child');

const result = spawnSync(process.execPath, [__filename, 'child']);

const stderr = result.stderr.toString();
assert.ok(
  stderr.includes('CUSTOM_ABORT_HANDLER_RAN'),
  `Expected custom abort handler marker in stderr, got:\n${stderr}`);
assert.ok(
  !stderr.includes('Native stack trace'),
  `Expected the custom handler to replace the default dump, got:\n${stderr}`);

if (common.isWindows) {
  assert.strictEqual(result.status, 134);
  assert.strictEqual(result.signal, null);
} else {
  assert.strictEqual(result.status, null);
  assert.strictEqual(result.signal, 'SIGABRT');
}
