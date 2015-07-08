// Flags: --experimental-workers
'use strict';

var assert = require('assert');
var util = require('util');
var Worker = require('worker');
var common = require('../common');
var checks = 0;

var tests = [
  'test/parallel/test-process-argv-0.js',
  'test/parallel/test-process-before-exit.js',
  'test/parallel/test-process-binding.js',
  'test/parallel/test-process-config.js',
  // Only main thread can mutate process environment.
  // 'test/parallel/test-process-env.js',
  'test/parallel/test-process-exec-argv.js',
  'test/parallel/test-process-exit-code.js',
  'test/parallel/test-process-exit-from-before-exit.js',
  'test/parallel/test-process-exit.js',
  'test/parallel/test-process-exit-recursive.js',
  'test/parallel/test-process-getgroups.js',
  'test/parallel/test-process-hrtime.js',
  'test/parallel/test-process-kill-null.js',
  'test/parallel/test-process-kill-pid.js',
  'test/parallel/test-process-next-tick.js',
  'test/parallel/test-process-raw-debug.js',
  'test/parallel/test-process-remove-all-signal-listeners.js',
  'test/parallel/test-process-versions.js',
  'test/parallel/test-process-wrap.js'
];

var parallelism = 4;
var testsPerThread = Math.ceil(tests.length / parallelism);
for (var i = 0; i < parallelism; ++i) {
  var shareOfTests = tests.slice(i * testsPerThread, (i + 1) * testsPerThread);
  var cur = Promise.resolve();
  shareOfTests.forEach(function(testFile) {
    cur = cur.then(function() {
      return common.runTestInsideWorker(testFile);
    });
  });
}
