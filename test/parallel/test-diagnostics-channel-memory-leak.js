// Flags: --expose-gc
'use strict';

// This test ensures that diagnostic channel references aren't leaked.

require('../common');
const { ok } = require('assert');

const { subscribe, unsubscribe } = require('diagnostics_channel');

function noop() {}

const heapUsedBefore = process.memoryUsage().heapUsed;

for (let i = 0; i < 1000; i++) {
  subscribe(String(i), noop);
  unsubscribe(String(i), noop);
}

global.gc();

const heapUsedAfter = process.memoryUsage().heapUsed;

ok(heapUsedBefore >= heapUsedAfter);
