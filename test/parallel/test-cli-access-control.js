'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

assert.strictEqual(
  spawnSync(process.execPath, ['-p', 'process.accessControl'])
    .stdout.toString().trim(), 'undefined');

{
  const { stdout, stderr } = spawnSync(
    process.execPath,
    ['--experimental-access-control', '-p', 'process.accessControl.apply']);
  assert.strictEqual(stdout.toString().trim(), '[Function: apply]');
  assert.strictEqual(
    stderr.toString().trim(),
    'Warning: Access control through disabling features is experimental and ' +
    'may be changed at any time.');
}

{
  const { stdout, stderr } = spawnSync(
    process.execPath,
    ['--disable=fsRead', '-p', 'process.accessControl.apply']);
  assert.strictEqual(stdout.toString().trim(), '[Function: apply]');
  assert.strictEqual(
    stderr.toString().trim(),
    'Warning: Access control through disabling features is experimental and ' +
    'may be changed at any time.');
}

{
  const { stderr } = spawnSync(process.execPath, ['--disable=unknown']);
  assert.strictEqual(
    stderr.toString().trim(),
    `${process.execPath}: unknown --disable entry unknown`);
}

{
  const { stderr } = spawnSync(
    process.execPath,
    ['--disable=fsRead', '-p', 'fs.readFileSync(process.execPath)']);
  assert.ok(
    stderr.toString().includes('Access to this API has been restricted'),
    stderr);
}
