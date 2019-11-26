'use strict';
// Flags: --expose-gc
require('../common');
const assert = require('assert');
const zlib = require('zlib');

// Remove the bufferPoolSize from the expected result. It would otherwise
// pollute the actual proportion.
let bufferPoolSize = new Uint8Array(Buffer.allocUnsafe(1).buffer).length;

// Tests that native zlib handles start out their life as weak handles.
const before = process.memoryUsage().external - bufferPoolSize;
for (let i = 0; i < 100; ++i)
  zlib.createGzip();
bufferPoolSize = new Uint8Array(Buffer.allocUnsafe(1).buffer).length;
const afterCreation = process.memoryUsage().external - bufferPoolSize;
global.gc();
const afterGC = process.memoryUsage().external - bufferPoolSize;

assert((afterGC - before) / (afterCreation - before) <= 0.02,
       `Expected after-GC delta ${afterGC - before} to be less than 2 %` +
       ` of before-GC delta ${afterCreation - before}`);
