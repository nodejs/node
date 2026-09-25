// Flags: --permission --allow-child-process --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const childProcess = require('child_process');

{
  assert.ok(process.permission.has('child'));
  const { status } = childProcess.spawnSync(process.execPath, ['--version']);
  assert.strictEqual(status, 0);
}

process.permission.drop('child');
{
  assert.ok(!process.permission.has('child'));
  assert.throws(() => {
    childProcess.spawnSync(process.execPath, ['--version']);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
  assert.throws(() => {
    childProcess.execSync(`"${process.execPath}" --version`);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
}

process.permission.drop('child');
assert.ok(!process.permission.has('child'));
