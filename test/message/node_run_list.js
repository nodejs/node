'use strict';

require('../common');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

spawnSyncAndExit(
  process.execPath,
  [ '--no-warnings', '--run'],
  { cwd: fixtures.path('run-script'), encoding: 'utf8', stdio: ['ignore', 'ignore', 'inherit'] },
  { status: 9, signal: null },
);
