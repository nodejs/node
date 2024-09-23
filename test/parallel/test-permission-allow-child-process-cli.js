// Flags: --experimental-permission --allow-child-process --allow-fs-read=*
'use strict';

const common = require('../common');
common.skipIfWorker();
const assert = require('assert');
const childProcess = require('child_process');

if (process.argv[2] === 'child') {
  process.exit(0);
}

// Guarantee the initial state
{
  assert.ok(process.permission.has('child'));
}

// The execPath might contain chars that should be escaped in a shell context.
// On non-Windows, we can pass the path via the env; `"` is not a valid char on
// Windows, so we can simply pass the path.
const execNode = (args) => childProcess.execSync(
  `"${common.isWindows ? process.execPath : '$NODE'}" ${args}`,
  common.isWindows ? undefined : { env: { ...process.env, NODE: process.execPath } },
);

// When a permission is set by cli, the process shouldn't be able
// to spawn unless --allow-child-process is sent
{
  // doesNotThrow
  childProcess.spawnSync(process.execPath, ['--version']);
  execNode('--version');
  childProcess.fork(__filename, ['child']);
  childProcess.execFileSync(process.execPath, ['--version']);
}
