'use strict';
require('../common');
const fixtures = require('../common/fixtures');

// This is a minimum integration test for CJS transpiled from ESM that tries to load real ESM.

const { spawnSync } = require('child_process');
const result = spawnSync(process.execPath, [
  '--experimental-require-module',
  fixtures.path('es-modules', 'transpiled-cjs-require-module', 'dist', 'import-both.cjs'),
], {
  encoding: 'utf8'
});
if (!result.stdout.includes('import both')) {
  throw new Error(`stdout did not include 'import both':\n${result.stdout}`);
}

const resultNamed = spawnSync(process.execPath, [
  '--experimental-require-module',
  fixtures.path('es-modules', 'transpiled-cjs-require-module', 'dist', 'import-named.cjs'),
], {
  encoding: 'utf8'
});
if (!resultNamed.stdout.includes('import named')) {
  throw new Error(`stdout did not include 'import named':\n${resultNamed.stdout}`);
}

const resultDefault = spawnSync(process.execPath, [
  '--experimental-require-module',
  fixtures.path('es-modules', 'transpiled-cjs-require-module', 'dist', 'import-default.cjs'),
], {
  encoding: 'utf8'
});
if (!resultDefault.stdout.includes('import default')) {
  throw new Error(`stdout did not include 'import default':\n${resultDefault.stdout}`);
}
