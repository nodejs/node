'use strict';
// This tests building SEA using --build-sea.

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const { join } = require('path');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');

const { spawnSyncAndAssert } = require('../common/child_process');
tmpdir.refresh();
const outputFile = buildSEA(fixtures.path('sea', 'basic'));

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
