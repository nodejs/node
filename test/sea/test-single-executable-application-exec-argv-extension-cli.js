'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the execArgvExtension "cli" mode in single executable applications.

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'exec-argv-extension-cli'));

// Test that --node-options works with execArgvExtension: "cli"
spawnSyncAndAssert(
  outputFile,
  ['--node-options=--max-old-space-size=1024', 'user-arg1', 'user-arg2'],
  {
    env: {
      ...process.env,
      NODE_OPTIONS: '--max-old-space-size=2048', // Should be ignored
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
    },
  },
  {
    stdout: /execArgvExtension cli test passed/,
  });
