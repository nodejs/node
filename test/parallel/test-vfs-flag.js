'use strict';

// node:vfs is gated behind --experimental-vfs. Without the flag the
// module is not exposed; bare `vfs` (without the node: scheme) is also
// blocked.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

// Without the flag, requiring node:vfs throws ERR_UNKNOWN_BUILTIN_MODULE.
{
  spawnSyncAndAssert(process.execPath, [
    '-e', 'require("node:vfs")',
  ], { status: 1, stderr: /ERR_UNKNOWN_BUILTIN_MODULE/ });
}

// Without the flag, importing node:vfs throws ERR_UNKNOWN_BUILTIN_MODULE.
{
  spawnSyncAndAssert(process.execPath, [
    '--input-type=module',
    '-e', 'import("node:vfs").catch((e) => { console.error(e.code); process.exit(1); });',
  ], {
    status: 1,
    stderr: /ERR_UNKNOWN_BUILTIN_MODULE/,
  });
}

// With the flag, node:vfs loads and works.
{
  const script =
    'const v = require("node:vfs");' +
    'const x = v.create();' +
    'x.writeFileSync("/x", "hi");' +
    'console.log(x.readFileSync("/x", "utf8"));';
  spawnSyncAndAssert(process.execPath, ['--experimental-vfs', '-e', script], {
    stdout: 'hi',
    trim: true,
  });
}

// Bare `vfs` (no node: scheme) is always blocked.
{
  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs',
    '-e', "require('vfs')",
  ], { status: 1, stderr: /Cannot find module 'vfs'/ });
}

// Module.builtinModules reflects whether --experimental-vfs is active.
for (const [flag, expected] of [
  ['--experimental-vfs', 'true\n'],
  ['--no-experimental-vfs', 'false\n'],
]) {
  spawnSyncAndAssert(process.execPath, [
    flag,
    '-p',
    'require("node:module").builtinModules.includes("node:vfs")',
  ], { stdout: expected, stderr: '' });
}
