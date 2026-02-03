'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the creation of a single executable application which uses the
// V8 code cache.

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'use-code-cache'));

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
