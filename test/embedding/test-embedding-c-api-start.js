'use strict';

// Tests that we can run the C-API node_embed_start.

const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

spawnSyncAndAssert(
  common.resolveBuiltBinary('embedtest'),
  ['c-api-nodejs-main', '--eval', 'console.log("Hello World")'],
  {
    trim: true,
    stdout: 'Hello World',
  },
);
