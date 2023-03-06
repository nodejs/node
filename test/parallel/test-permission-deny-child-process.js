// Flags: --experimental-permission --allow-fs-read=* --allow-child-process
'use strict';

const common = require('../common');
common.skipIfWorker();
const assert = require('assert');
const childProcess = require('child_process');

if (process.argv[2] === 'child') {
  process.exit(0);
}

{
  // doesNotThrow
  const spawn = childProcess.spawn(process.execPath, ['--version']);
  spawn.kill();
  const exec = childProcess.exec(process.execPath, ['--version']);
  exec.kill();
  const fork = childProcess.fork(__filename, ['child']);
  fork.kill();
  const execFile = childProcess.execFile(process.execPath, ['--version']);
  execFile.kill();

  assert.ok(process.permission.deny('child'));

  // When a permission is set by API, the process shouldn't be able
  // to spawn
  assert.throws(() => {
    childProcess.spawn(process.execPath, ['--version']);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
  assert.throws(() => {
    childProcess.spawnSync(process.execPath, ['--version']);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
  assert.throws(() => {
    childProcess.exec(process.execPath, ['--version']);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
  assert.throws(() => {
    childProcess.execSync(process.execPath, ['--version']);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
  assert.throws(() => {
    childProcess.fork(__filename, ['child']);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
  assert.throws(() => {
    childProcess.execFile(process.execPath, ['--version']);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
  assert.throws(() => {
    childProcess.execFileSync(process.execPath, ['--version']);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'ChildProcess',
  }));
}
