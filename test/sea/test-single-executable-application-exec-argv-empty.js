'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the execArgv functionality with empty array in single executable applications.

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'exec-argv-empty'));

// Test that empty execArgv work correctly
spawnSyncAndAssert(
  outputFile,
  ['user-arg'],
  {
    env: {
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    },
  },
  {
    stdout: /empty execArgv test passed/,
  });
