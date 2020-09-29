'use strict';
// Flags: --expose-gc

require('../common');

const assert = require('assert');
const fs = require('fs');

const max = 8192;
let now = 1;
for (let i = 0; i < max; i++) {
  fs.openSyncSafe(__filename, 'r');
  now++;
}
global.gc();
assert.strictEqual(now, max);
