'use strict';

// This tests the creation of a single executable application with an ESM
// entry point using "mainFormat": "module" and "useCodeCache": true.

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'esm-code-cache'));

spawnSyncAndExitWithoutError(
  outputFile,
  {
    env: {
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    },
  },
  {
    stdout: /ESM SEA with code cache executed successfully/,
    stderr: /SEA module code cache accepted/,
  });
