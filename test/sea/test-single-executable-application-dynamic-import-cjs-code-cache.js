'use strict';

require('../common');

// This tests that a CommonJS SEA entry point can dynamically import a user
// module from the file system when explicitly enabled, even with code cache.

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
  configPath: 'sea-config-opt-in-code-cache.json',
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
