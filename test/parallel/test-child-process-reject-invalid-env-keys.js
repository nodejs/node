'use strict';
const common = require('../common');

const assert = require('assert');
const { exec, execFile, execFileSync, execSync, fork, spawn, spawnSync } = require('child_process');

const invalidKeys = ['', 'AAA=BBB', 'AAA=', 'A=', '=A=B'];
if (!common.isWindows) {
  // A leading '=' is only valid on Windows, where it is used by per-drive
  // variables such as '=C:'.
  invalidKeys.push('=C:');
} else {
  // A lone '=' is not a valid name on Windows either.
  invalidKeys.push('=');
}

const expected = {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
  message: /must be a valid environment variable name/,
};

for (const key of invalidKeys) {
  const options = { env: { [key]: 'value' } };
  assert.throws(() => spawn(process.execPath, options), expected);
  assert.throws(() => spawnSync(process.execPath, options), expected);
  assert.throws(() => exec(process.execPath, options, common.mustNotCall()), expected);
  assert.throws(() => execSync(process.execPath, options), expected);
  assert.throws(() => execFile(process.execPath, options, common.mustNotCall()), expected);
  assert.throws(() => execFileSync(process.execPath, options), expected);
  assert.throws(() => fork(__filename, options), expected);
}

if (common.isWindows) {
  const env = { ...process.env, '=C:': 'C:\\foo', '=D:': 'D:\\bar' };
  const child = spawnSync(process.execPath, ['-e', ''], { env });
  assert.strictEqual(child.status, 0);
  execFileSync(process.execPath, ['-e', ''], { env });
}
