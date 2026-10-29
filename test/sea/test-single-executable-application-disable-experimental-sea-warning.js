'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the creation of a single executable application which has the
// experimental SEA warning disabled.

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'disable-experimental-warning'));

spawnSyncAndAssert(
  outputFile,
  [ '-a', '--b=c', 'd' ],
  {
    env: {
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    },
  },
  {
    stdout: 'Hello, world! 😊\n',
  });
