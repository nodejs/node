'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the execArgv functionality with multiple arguments in single executable applications.

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');
const assert = require('assert');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'exec-argv'));

// Test that multiple execArgv are properly applied
spawnSyncAndAssert(
  outputFile,
  ['user-arg1', 'user-arg2'],
  {
    env: {
      ...process.env,
      NODE_NO_WARNINGS: '0',
      COMMON_DIRECTORY: join(__dirname, '..', 'common'),
      NODE_DEBUG_NATIVE: 'SEA',
    },
  },
  {
    stdout: /multiple execArgv test passed/,
    stderr(output) {
      assert.doesNotMatch(output, /This warning should not be shown in the output/);
      return true;
    },
    trim: true,
  });
