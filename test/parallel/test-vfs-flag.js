'use strict';

// Both vfs and node:vfs are gated behind --experimental-vfs.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

// Without the flag, or when explicitly disabled, neither specifier is exposed.
for (const flags of [[], ['--no-experimental-vfs']]) {
  for (const [id, requireError, importError] of [
    ['vfs', 'MODULE_NOT_FOUND', 'ERR_MODULE_NOT_FOUND'],
    ['node:vfs', 'ERR_UNKNOWN_BUILTIN_MODULE', 'ERR_UNKNOWN_BUILTIN_MODULE'],
  ]) {
    spawnSyncAndAssert(process.execPath, [
      ...flags,
      '-e',
      `const assert = require('node:assert');
       const { isBuiltin } = require('node:module');
       assert.strictEqual(isBuiltin('${id}'), false);
       assert.strictEqual(process.getBuiltinModule('${id}'), undefined);
       assert.throws(() => require('${id}'), { code: '${requireError}' });`,
    ], { status: 0 });

    spawnSyncAndAssert(process.execPath, [
      ...flags,
      '--input-type=module',
      '-e',
      `import assert from 'node:assert';
       await assert.rejects(import('${id}'), { code: '${importError}' });`,
    ], { status: 0 });
  }
}

// With the flag, both specifiers resolve to the same CommonJS exports.
for (const id of ['vfs', 'node:vfs']) {
  const script =
    'const assert = require("node:assert");' +
    `const v = require('${id}');` +
    'assert.strictEqual(v, require("node:vfs"));' +
    `assert.strictEqual(require.resolve('${id}'), '${id}');` +
    `assert.strictEqual(require('node:module').isBuiltin('${id}'), true);` +
    `assert.strictEqual(process.getBuiltinModule('${id}'), v);` +
    'const x = v.create();' +
    'x.writeFileSync("/x", "hi");' +
    'console.log(x.readFileSync("/x", "utf8"));';
  spawnSyncAndAssert(process.execPath, ['--experimental-vfs', '-e', script], {
    stdout: 'hi',
    trim: true,
  });
}

// Static and dynamic ESM imports share the same module and named exports.
{
  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs',
    '--input-type=module',
    '-e',
    `import assert from 'node:assert';
     import vfs, { create } from 'vfs';
     import nodeVfs, { create as nodeCreate } from 'node:vfs';
     assert.strictEqual(vfs, nodeVfs);
     assert.strictEqual(create, nodeCreate);
     assert.strictEqual(await import('vfs'), await import('node:vfs'));`,
  ], { status: 0 });
}

// Module.builtinModules lists the bare name when --experimental-vfs is active.
for (const [flag, expected] of [
  ['--experimental-vfs', 'true\n'],
  ['--no-experimental-vfs', 'false\n'],
]) {
  spawnSyncAndAssert(process.execPath, [
    flag,
    '-p',
    'require("node:module").builtinModules.includes("vfs")',
  ], { stdout: expected, stderr: '' });
}
