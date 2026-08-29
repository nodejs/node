'use strict';

const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

const experimentalBuiltins = [
  ['dtls', '--experimental-dtls', common.hasDtls],
  ['ffi', '--experimental-ffi', common.hasFFI],
  ['quic', '--experimental-quic', common.hasQuic],
  ['vfs', '--experimental-vfs', true],
].filter(([, , available]) => available);

for (const [id, flag] of experimentalBuiltins) {
  const builtin = `node:${id}`;

  spawnSyncAndAssert(process.execPath, [
    '-e', `const m = require('node:module'); if (m.builtinModules.includes('${builtin}')) process.exit(1); try { require('${builtin}'); } catch (e) { if (e.code === 'ERR_UNKNOWN_BUILTIN_MODULE') process.exit(0); } process.exit(1);`,
  ], { status: 0 });

  spawnSyncAndAssert(process.execPath, [
    flag,
    '-e', `const m = require('node:module'); if (!m.builtinModules.includes('${builtin}')) process.exit(1); require('${builtin}');`,
  ], { status: 0 });
}

const schemeOnlyBuiltins = ['node:test', 'node:sea'];
if (common.hasSQLite) {
  schemeOnlyBuiltins.push('node:sqlite');
}

for (const id of schemeOnlyBuiltins) {
  spawnSyncAndAssert(process.execPath, [
    '-e', `const m = require('node:module'); if (!m.builtinModules.includes('${id}')) process.exit(1); require('${id}');`,
  ], { status: 0 });
}
