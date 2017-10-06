'use strict';

const common = require('../common');
const { spawn } = require('child_process');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();
const tmpDir = common.tmpDir;

const entry = path.join(tmpDir, 'entry.mjs');
const real = path.join(tmpDir, 'index.mjs');
const link_absolute_path = path.join(tmpDir, 'absolute');
const link_relative_path = path.join(tmpDir, 'relative');
const link_ignore_extension = path.join(tmpDir,
                                        'ignore_extension.json');
const link_directory = path.join(tmpDir, 'directory');

fs.writeFileSync(real, 'export default [];');
fs.writeFileSync(entry, `
import assert from 'assert';
import real from './index.mjs';
import absolute from './absolute';
import relative from './relative';
import ignoreExtension from './ignore_extension.json';
import directory from './directory';

assert.strictEqual(absolute, real);
assert.strictEqual(relative, real);
assert.strictEqual(ignoreExtension, real);
assert.strictEqual(directory, real);
`);

try {
  fs.symlinkSync(real, link_absolute_path);
  fs.symlinkSync(path.basename(real), link_relative_path);
  fs.symlinkSync(real, link_ignore_extension);
  fs.symlinkSync(path.dirname(real), link_directory);
} catch (err) {
  if (err.code !== 'EPERM') throw err;
  common.skip('insufficient privileges for symlinks');
}

spawn(process.execPath, ['--experimental-modules', entry],
      { stdio: 'inherit' }).on('exit', (code) => {
  assert.strictEqual(code, 0);
});
