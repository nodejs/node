'use strict';

require('../common');

// This tests that an ESM SEA entry point can use dynamic import() with a
// relative specifier resolved from the executable location.

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'dynamic-import-esm'), {
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
    stdout: /ESM SEA dynamic import executed successfully/,
  });
