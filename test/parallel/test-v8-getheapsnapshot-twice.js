'use strict';
require('../common');
const v8 = require('v8');

// Regression test for https://github.com/nodejs/node/issues/35559
// It is important that the return value of the first call is not used, i.e.
// that the first snapshot is GC-able while the second one is being created.
v8.getHeapSnapshot();
v8.getHeapSnapshot();
