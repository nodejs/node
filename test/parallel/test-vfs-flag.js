'use strict';

// node:vfs is available without an experimental flag. The old flag remains a
// no-op for compatibility, and the module remains available with --no- form.

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

// The old flag is accepted for compatibility but no longer gates the module.
for (const flag of ['--experimental-vfs', '--no-experimental-vfs']) {
  spawnSyncAndAssert(process.execPath, [
    flag,
    '-p',
    'require("node:module").builtinModules.includes("node:vfs")',
  ], { stdout: 'true\n', stderr: '' });
}

// Bare `vfs` (no node: scheme) remains unavailable.
spawnSyncAndAssert(process.execPath, [
  '-e', "require('vfs')",
], { status: 1, stderr: /Cannot find module 'vfs'/ });
