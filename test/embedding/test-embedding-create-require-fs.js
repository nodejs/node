'use strict';

// Tests that createRequire() works correctly with local fs in embedded environments.
// The createRequire() call is in embedtest.cc.

const common = require('../common');
const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');

const fixturePath = JSON.stringify(fixtures.path('exit.js'));
spawnSyncAndExit(
  common.resolveBuiltBinary('embedtest'),
  [`require(${fixturePath})`, 92],
  {
    status: 92,
    signal: null,
  });
