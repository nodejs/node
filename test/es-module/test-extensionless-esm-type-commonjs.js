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
