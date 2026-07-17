// Flags: --expose-gc
'use strict';
require('../common');
const { testGCProfiler } = require('../common/v8');

testGCProfiler();

for (let i = 0; i < 100; i++) {
  new Array(100);
}

global?.gc();
