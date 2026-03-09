'use strict';

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

spawnSyncAndAssert(process.execPath, [
  fixtures.path('es-modules', 'extensionless-esm-commonjs', 'script'),
], {
  status: 1,
  stderr: /SyntaxError: Cannot use import statement outside a module/,
});

spawnSyncAndAssert(process.execPath, [
  fixtures.path('es-modules', 'extensionless-esm-module', 'script'),
], {
  stdout: /script STARTED[\s\S]*v\d+\./,
  trim: true,
});

// CJS extensionless file inside a type: "module" package should work
// when require()'d. Regression test for https://github.com/nodejs/node/issues/61971
spawnSyncAndAssert(process.execPath, [
  '-e', `const m = require(${JSON.stringify(
    fixtures.path('es-modules', 'extensionless-cjs-module', 'index')
  )}); if (m.hello !== 'world') throw new Error('expected CJS exports, got: ' + JSON.stringify(m))`,
], {
  status: 0,
});
