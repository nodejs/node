import '../common/index.mjs';
import { spawnSyncAndAssert, spawnSyncAndExit } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';

const map = fixtures.path('package-map/traversal/map.json');
const cwd = fixtures.path('package-map/traversal/app');

function runESM(specifier) {
  return [
    '--no-warnings',
    '--experimental-package-map', map,
    '--input-type=module',
    '--eval', `import x from '${specifier}'; console.log(x);`,
  ];
}

function runCJS(specifier) {
  return [
    '--no-warnings',
    '--experimental-package-map', map,
    '--eval', `console.log(require('${specifier}'));`,
  ];
}

for (const specifier of [
  'dep/../secret.mjs',
  'dep/%2e%2e/secret.mjs',
  'dep/sub/../../secret.mjs',
]) {
  spawnSyncAndExit(process.execPath, runESM(specifier), { cwd }, {
    status: 1,
    signal: null,
    stderr: /ERR_INVALID_MODULE_SPECIFIER/,
  });
}

for (const specifier of [
  'dep/sub/file.mjs',
  'dep/./sub/file.mjs',
  'dep/sub/../sub/file.mjs',
]) {
  spawnSyncAndAssert(process.execPath, runESM(specifier), { cwd }, {
    stdout: /dep-sub/,
    trim: true,
  });
}

for (const specifier of [
  'dep/../secret.cjs',
  'dep/%2e%2e/secret.cjs',
  'dep/sub/../../secret.cjs',
]) {
  spawnSyncAndExit(process.execPath, runCJS(specifier), { cwd }, {
    status: 1,
    signal: null,
    stderr: /ERR_INVALID_MODULE_SPECIFIER/,
  });
}

for (const specifier of [
  'dep/sub/file.cjs',
  'dep/./sub/file.cjs',
  'dep/sub/../sub/file.cjs',
]) {
  spawnSyncAndAssert(process.execPath, runCJS(specifier), { cwd }, {
    stdout: /dep-sub-cjs/,
    trim: true,
  });
}
