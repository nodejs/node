'use strict';

// Tests node::LoadEnvironment() with main_script_source_utf8 works.

const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

const embedtest = common.resolveBuiltBinary('embedtest');

spawnSyncAndAssert(
  embedtest,
  ['console.log(42)'],
  {
    trim: true,
    stdout: '42',
  });
