'use strict';

// Tests that createRequire() from embedded environments still cannot access internals.
// The createRequire() call is in embedtest.cc.

const common = require('../common');
const { spawnSyncAndExit } = require('../common/child_process');

spawnSyncAndExit(
  common.resolveBuiltBinary('embedtest'),
  ['require("lib/internal/test/binding")'],
  {
    status: 1,
    signal: null,
  });
