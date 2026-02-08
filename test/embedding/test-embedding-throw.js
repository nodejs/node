'use strict';

// Tests that uncaught exceptions are handled correctly in embedded environments.

const common = require('../common');
const { spawnSyncAndExit } = require('../common/child_process');

spawnSyncAndExit(
  common.resolveBuiltBinary('embedtest'),
  ['throw new Error()'],
  {
    status: 1,
    signal: null,
  });
