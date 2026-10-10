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
