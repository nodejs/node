// Flags: --experimental-workers
'use strict';

var assert = require('assert');
var util = require('util');
var Worker = require('worker');
var common = require('../common');
var checks = 0;

var tests = [
  'test/parallel/test-domain-abort-on-uncaught.js',
  'test/parallel/test-domain-crypto.js',
  'test/parallel/test-domain-enter-exit.js',
  'test/parallel/test-domain-exit-dispose-again.js',
  'test/parallel/test-domain-exit-dispose.js',
  'test/parallel/test-domain-from-timer.js',
  'test/parallel/test-domain-http-server.js',
  'test/parallel/test-domain-implicit-fs.js',
  'test/parallel/test-domain.js',
  'test/parallel/test-domain-multi.js',
  'test/parallel/test-domain-nested.js',
  'test/parallel/test-domain-nested-throw.js',
  'test/parallel/test-domain-safe-exit.js',
  'test/parallel/test-domain-stack.js',
  'test/parallel/test-domain-timers.js'
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
