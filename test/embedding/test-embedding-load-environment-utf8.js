'use strict';

// Test node::LoadEnvironment() works with non-ASCII source code. The variable
// comes from embedtest.cc.

const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

const embedtest = common.resolveBuiltBinary('embedtest');

spawnSyncAndAssert(
  embedtest,
  ['console.log(embedVars.nön_ascıı)'],
  {
    trim: true,
    stdout: '🏳️‍🌈',
  });
