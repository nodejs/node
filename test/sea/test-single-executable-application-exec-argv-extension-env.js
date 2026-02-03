'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the execArgvExtension "env" mode (default) in single executable applications.

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const { join } = require('path');
const assert = require('assert');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'exec-argv-extension-env'));

// Test that NODE_OPTIONS works with execArgvExtension: "env" (default behavior)
spawnSyncAndAssert(
  outputFile,
  ['user-arg1', 'user-arg2'],
  {
    env: {
      ...process.env,
      NODE_OPTIONS: '--max-old-space-size=512',
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
    },
  },
  {
    stdout: /execArgvExtension env test passed/,
    stderr(output) {
      assert.doesNotMatch(output, /This warning should not be shown in the output/);
      return true;
    },
    trim: true,
  });
