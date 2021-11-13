'use strict';

const common = require('../common');

const assert = require('assert');
const async_hooks = require('async_hooks');
const fs = require('fs');

let numFsReqCallbacks = 0;
async_hooks.createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (type === 'FSREQCALLBACK')
      ++numFsReqCallbacks;
  },
  destroy(asyncId) {
    --numFsReqCallbacks;
  }
}).enable();

for (let i = 0; i < 12; i++)
  fs.open(__filename, 'r', common.mustCall());

assert.strictEqual(numFsReqCallbacks, 12);
