'use strict';

require('../common');

// This tests that a CommonJS SEA entry point can use dynamic import() to
// load a user module from the file system.

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'dynamic-import-cjs'), {
  configPath: 'sea-config-opt-in.json',
});

spawnSyncAndExitWithoutError(
  outputFile,
  {
    env: {
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    },
  },
  {
    stdout: /CJS SEA dynamic import executed successfully/,
  });
