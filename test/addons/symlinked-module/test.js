'use strict';
const common = require('../../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

// This test verifies that symlinked native modules can be required multiple
// times without error. The symlinked module and the non-symlinked module
// should be the same instance. This expectation was not previously being
// tested and ended up being broken by https://github.com/nodejs/node/pull/5950.

// This test should pass in Node.js v4 and v5. This test will pass in Node.js
// with https://github.com/nodejs/node/pull/5950 reverted.

const tmpdir = require('../../common/tmpdir');
tmpdir.refresh();

const addonPath = path.join(__dirname, 'build', common.buildType);
const addonLink = path.join(tmpdir.path, 'addon');

try {
  fs.symlinkSync(addonPath, addonLink, 'dir');
} catch (err) {
  if (err.code !== 'EPERM') throw err;
  common.skip('module identity test (no privs for symlinks)');
}

const sub = require('./submodule');
[addonPath, addonLink].forEach((i) => {
  const mod = require(path.join(i, 'binding.node'));
  assert.notStrictEqual(mod, null);
  assert.strictEqual(mod.hello(), 'world');
  sub.test(i); // Should not throw.
});
