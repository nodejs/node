// Flags: --experimental-workers
'use strict';

var assert = require('assert');
var util = require('util');
var Worker = require('worker');
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

process.on('unhandledRejection', function(e) {
  throw e;
});

var parallelism = 4;
var testsPerThread = Math.ceil(tests.length / parallelism);
for (var i = 0; i < parallelism; ++i) {
  var shareOfTests = tests.slice(i * testsPerThread, (i + 1) * testsPerThread);
  var cur = Promise.resolve();
  shareOfTests.forEach(function(testFile) {
    cur = cur.then(function() {
      return runTestInsideWorker(testFile);
    });
  });
}

function runTestInsideWorker(testFile) {
  return new Promise(function(resolve, reject) {
    console.log(util.format('executing %s', testFile));
    var worker = new Worker(testFile, {keepAlive: false});
    worker.on('error', function(e) {
      console.error(testFile + ' failed');
      reject(e);
    });
    worker.on('exit', function(exitCode) {
      if (exitCode === 0)
        resolve();
      else
        reject(new Error(util.format(
            '%s exited with code %s', testFile, exitCode)));
    });
  });
}
