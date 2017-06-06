// Flags: --experimental-modules
'use strict';

const common = require('../common');
const { spawn } = require('child_process');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();
const tmpDir = common.tmpDir;

const entry = path.join(tmpDir, 'entry.js');
const real = path.join(tmpDir, 'real.js');
const link_absolute_path = path.join(tmpDir, 'link.js');

fs.writeFileSync(entry, `
const assert = require('assert');
global.x = 0;
require('./real.js');
assert.strictEqual(x, 1);
require('./link.js');
assert.strictEqual(x, 2);
`);
fs.writeFileSync(real, 'x++;');

try {
  fs.symlinkSync(real, link_absolute_path);
} catch (err) {
  if (err.code !== 'EPERM') throw err;
  common.skip('insufficient privileges for symlinks');
}

spawn(process.execPath,
      ['--experimental-modules', '--preserve-symlinks', entry],
      { stdio: 'inherit' }).on('exit', (code) => {
  assert.strictEqual(code, 0);
});
