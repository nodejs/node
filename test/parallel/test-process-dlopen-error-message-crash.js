'use strict';

// This is a regression test for some scenarios in which node would pass
// unsanitized user input to a printf-like formatting function when dlopen
// fails, potentially crashing the process.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const assert = require('assert');
const fs = require('fs');

// This error message should not be passed to a printf-like function.
assert.throws(() => {
  process.dlopen({ exports: {} }, 'foo-%s.node');
}, ({ name, code, message }) => {
  assert.strictEqual(name, 'Error');
  assert.strictEqual(code, 'ERR_DLOPEN_FAILED');
  if (!common.isAIX && !common.isIBMi) {
    assert.match(message, /foo-%s\.node/);
  }
  return true;
});

const notBindingDir = 'test/addons/not-a-binding';
const notBindingPath = `${notBindingDir}/build/Release/binding.node`;
const strangeBindingPath = `${tmpdir.path}/binding-%s.node`;
// Ensure that the addon directory exists, but skip the remainder of the test if
// the addon has not been compiled.
fs.accessSync(notBindingDir);
try {
  fs.copyFileSync(notBindingPath, strangeBindingPath);
} catch (err) {
  if (err.code !== 'ENOENT') throw err;
  common.skip(`addon not found: ${notBindingPath}`);
}

// This error message should also not be passed to a printf-like function.
assert.throws(() => {
  process.dlopen({ exports: {} }, strangeBindingPath);
}, {
  name: 'Error',
  code: 'ERR_DLOPEN_FAILED',
  message: /^Module did not self-register: '.*binding-%s\.node'\.$/
});
