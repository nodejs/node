'use strict';

const common = require('../common');
const { spawn } = require('child_process');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const tmpDir = tmpdir.path;

fs.mkdirSync(path.join(tmpDir, 'nested'));
fs.mkdirSync(path.join(tmpDir, 'nested2'));

const entry = path.join(tmpDir, 'nested', 'entry.js');
const entry_link_absolute_path = path.join(tmpDir, 'link.js');
const submodule = path.join(tmpDir, 'nested2', 'submodule.js');
const submodule_link_absolute_path = path.join(tmpDir, 'submodule_link.js');

fs.writeFileSync(entry, `
const assert = require('assert');

// this import only resolves with --preserve-symlinks-main set
require('./submodule_link.js');
`);
fs.writeFileSync(submodule, '');

try {
  fs.symlinkSync(entry, entry_link_absolute_path);
  fs.symlinkSync(submodule, submodule_link_absolute_path);
} catch (err) {
  if (err.code !== 'EPERM') throw err;
  common.skip('insufficient privileges for symlinks');
}

function doTest(flags, done) {
  // Invoke the main file via a symlink.  In this case --preserve-symlinks-main
  // dictates that it'll resolve relative imports in the main file relative to
  // the symlink, and not relative to the symlink target; the file structure set
  // up above requires this to not crash when loading ./submodule_link.js
  spawn(process.execPath, [
    '--preserve-symlinks',
    '--preserve-symlinks-main',
    entry_link_absolute_path,
  ], { stdio: 'inherit' })
    .on('exit', (code) => {
      assert.strictEqual(code, 0);
      done();
    });
}

// First test the commonjs module loader
doTest([], () => {
  // Now test the new loader
  doTest([], () => {});
});
