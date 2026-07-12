'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the execArgvExtension "none" mode in single executable applications.

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'exec-argv-extension-none'));

// Test that NODE_OPTIONS is ignored with execArgvExtension: "none"
spawnSyncAndAssert(
  outputFile,
  ['user-arg1', 'user-arg2'],
  {
    env: {
      ...process.env,
      NODE_OPTIONS: '--max-old-space-size=2048',
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
    },
  },
  {
    stdout: /execArgvExtension none test passed/,
  });
