'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const { spawnSyncAndAssert, spawnSyncAndExit } = require('../common/child_process');

// --allow-fs-vfs requires the permission model.
spawnSyncAndExit(
  process.execPath,
  ['--allow-fs-vfs', '-e', ''],
  {
    status: 1,
    signal: null,
    stderr: /--permission is required/,
  },
);

// Mounting a VFS is refused under --permission without --allow-fs-vfs, but
// audit mode only observes and lets the mount through.
const mount = `
  require('node:vfs').create().mount();
  console.log('mounted');
`;

spawnSyncAndExit(
  process.execPath,
  ['--experimental-vfs', '--permission', '-e', mount],
  {
    status: 1,
    signal: null,
    stderr: /ERR_INVALID_STATE/,
  },
);

spawnSyncAndAssert(
  process.execPath,
  ['--experimental-vfs', '--permission-audit', '-e', mount],
  {
    stdout: /^mounted$/m,
  },
);

if (process.config.variables.node_without_node_options) {
  common.skip('missing NODE_OPTIONS support');
}

// A child process inherits --allow-fs-vfs along with --permission.
const child = `
  const { spawnSync } = require('child_process');
  const { stdout } = spawnSync(process.execPath, [
    '-p', 'process.env.NODE_OPTIONS',
  ], { encoding: 'utf8' });
  process.stdout.write(stdout);
`;

spawnSyncAndAssert(
  process.execPath,
  ['--permission', '--allow-fs-vfs', '--allow-child-process', '-e', child],
  {
    stdout: /(^|\s)--allow-fs-vfs(\s|$)/,
  },
);
