'use strict';

require('../common');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

const { child } = spawnSyncAndExit(
  process.execPath,
  [ '--no-warnings', '--run'],
  { cwd: fixtures.path('run-script'), encoding: 'utf8' },
  { status: 9, signal: null },
);
console.log(child.stderr);
