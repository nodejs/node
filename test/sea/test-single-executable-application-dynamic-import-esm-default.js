'use strict';

require('../common');

// This tests that an ESM SEA entry point cannot dynamically import a user
// module from the file system without explicit configuration.

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'dynamic-import-esm'));

spawnSyncAndAssert(
  outputFile,
  {
    env: {
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    },
  },
  {
    status: 1,
    stderr: /ERR_UNKNOWN_BUILTIN_MODULE/,
  });
