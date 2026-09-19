'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (!common.isLinux) {
  common.skip('/proc/<pid>/environ is specific to Linux');
}

const assert = require('assert');
const fs = require('fs');
const { spawn } = require('child_process');
const { spawnSyncAndAssert } = require('../common/child_process');

const env = {
  ...process.env,
  PERMISSION_ENV_SECRET: 'secret-value',
  PERMISSION_ENV_ALLOWED: 'allowed-value',
};

// /proc/<pid>/environ exposes the environment a process started with, so
// reading it is denied while the env scope is restricted, even when the file
// system scope would allow it.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-fs-read=*', '-e',
    `
    const assert = require('assert');
    const fs = require('fs');
    for (const path of [
      '/proc/self/environ',
      '/proc/thread-self/environ',
      '/proc/' + process.pid + '/environ',
      '/proc/' + process.ppid + '/environ',
    ]) {
      assert.throws(() => fs.readFileSync(path), {
        code: 'ERR_ACCESS_DENIED',
        permission: 'FileSystemRead',
        resource: path,
      });
    }
    `,
  ],
  { env },
  {});

// Reading it is allowed when every variable is accessible.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-fs-read=*', '--allow-env=*', '-e',
    'require("fs").readFileSync("/proc/self/environ")',
  ],
  { env },
  {});

// Other processes do not find the removed variables there either, because
// they are overwritten in the initial environment block.
{
  const child = spawn(
    process.execPath,
    [
      '--permission', '--allow-env=PERMISSION_ENV_ALLOWED', '-e',
      'console.log("ready"); process.stdin.once("data", () => {});',
    ],
    { env, stdio: ['pipe', 'pipe', 'inherit'] });

  child.stdout.once('data', common.mustCall(() => {
    const environ = fs.readFileSync(`/proc/${child.pid}/environ`, 'latin1');
    assert.doesNotMatch(environ, /PERMISSION_ENV_SECRET|secret-value/);
    assert.match(environ, /PERMISSION_ENV_ALLOWED=allowed-value/);
    child.stdin.end('done');
  }));

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}
