'use strict';

// node:vfs is available without an experimental flag. The old positive flag
// remains accepted for compatibility, and --no-experimental-vfs disables it.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

// CommonJS and ESM can load node:vfs without a flag.
{
  const script =
    'const v = require("node:vfs");' +
    'const x = v.create();' +
    'x.writeFileSync("/x", "hi");' +
    'console.log(x.readFileSync("/x", "utf8"));';
  spawnSyncAndAssert(process.execPath, ['-e', script], {
    stdout: 'hi',
    stderr: /ExperimentalWarning: VirtualFileSystem is an experimental feature/,
    trim: true,
  });

  spawnSyncAndAssert(process.execPath, [
    '--input-type=module',
    '-e', 'import("node:vfs").then(({ default: v }) => console.log(typeof v.create));',
  ], {
    stdout: 'function\n',
    stderr: /ExperimentalWarning: VirtualFileSystem is an experimental feature/,
  });
}

// The old positive flag is accepted for compatibility but no longer gates the
// module.
spawnSyncAndAssert(process.execPath, [
  '--experimental-vfs',
  '-p',
  'require("node:module").builtinModules.includes("node:vfs")',
], { stdout: 'true\n', stderr: '' });

// --no-experimental-vfs disables node:vfs.
spawnSyncAndAssert(process.execPath, [
  '--no-experimental-vfs',
  '-p',
  'require("node:module").builtinModules.includes("node:vfs")',
], { stdout: 'false\n', stderr: '' });

spawnSyncAndAssert(process.execPath, [
  '--no-experimental-vfs',
  '-e', 'require("node:vfs")',
], { status: 1, stderr: /ERR_UNKNOWN_BUILTIN_MODULE/ });

// Bare `vfs` (no node: scheme) remains unavailable.
spawnSyncAndAssert(process.execPath, [
  '-e', "require('vfs')",
], { status: 1, stderr: /Cannot find module 'vfs'/ });
