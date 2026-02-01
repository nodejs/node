'use strict';

const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

const commonjsDir = fixtures.path('es-modules', 'extensionless-esm-commonjs');

spawnSyncAndAssert(process.execPath, ['--no-experimental-detect-module', './script'], {
  cwd: commonjsDir,
  encoding: 'utf8',
}, {
  status: 1,
  stderr: /.+/,
  trim: true,
});

const moduleDir = fixtures.path('es-modules', 'extensionless-esm-module');

spawnSyncAndAssert(process.execPath, ['--no-experimental-detect-module', './script'], {
  cwd: moduleDir,
  encoding: 'utf8',
}, {
  stdout: /script STARTED[\s\S]*v\d+\./,
  trim: true,
});
