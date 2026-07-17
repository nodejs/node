'use strict';

// Tests that process.exitCode works correctly in embedded environments.

const common = require('../common');
const { spawnSyncAndExit } = require('../common/child_process');

spawnSyncAndExit(
  common.resolveBuiltBinary('embedtest'),
  ['process.exitCode = 8'],
  {
    status: 8,
    signal: null,
  });
