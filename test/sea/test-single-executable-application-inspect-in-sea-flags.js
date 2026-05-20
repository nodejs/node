'use strict';

// This tests that the debugger flag --inspect passed directly to a single executable
// application would not be consumed by Node.js but passed to the application
// instead.

require('../common');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'inspect-in-sea-flags'));

// Spawn the SEA with inspect option
spawnSyncAndAssert(
  outputFile,
  ['--inspect=0'],
  {
    env: {
      ...process.env,
    },
  },
  {
    stdout(data) {
      assert.match(data, /--inspect=0/);
      return true;
    },
    stderr(data) {
      assert.doesNotMatch(data, /Debugger listening/);
      return true;
    },
    trim: true,
  },
);
