// Skip this test if Node is not compiled with isolates support.
if (!process.features.isolates) return;

var assert = require('assert');

// This is the same test as test-child-process-fork3 except it uses isolates
// instead of processes. process.TEST_ISOLATE is a ghetto method of passing
// some information into the other test.
process.TEST_ISOLATE = true;
require('./test-child-process-fork3');

var numThreads = process.binding('isolates').count();
assert.ok(numThreads > 1);
