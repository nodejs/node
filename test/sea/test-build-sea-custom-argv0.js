'use strict';
// This tests --build-sea with a custom argv0 value.

require('../common');

const { skipIfBuildSEAIsNotSupported } = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const { resolve } = require('path');
const fixtures = require('../common/fixtures');
const { rmSync } = require('fs');

const { spawnSyncAndAssert } = require('../common/child_process');
const fixturesDir = fixtures.path('sea', 'basic');

spawnSyncAndAssert(
  process.execPath,
  ['--build-sea', resolve(fixturesDir, 'sea-config.json')], {
    cwd: fixturesDir,
    argv0: 'argv0',
  }, {
    stdout: /Generated single executable/,
  });

rmSync(resolve(fixturesDir, 'sea'));
