// Flags: --experimental-workers
'use strict';

var assert = require('assert');
var util = require('util');
var Worker = require('worker');
var checks = 0;
var tests = [
  'test/parallel/test-zlib-close-after-write.js',
  'test/parallel/test-zlib-convenience-methods.js',
  'test/parallel/test-zlib-dictionary-fail.js',
  'test/parallel/test-zlib-dictionary.js',
  'test/parallel/test-zlib-flush.js',
  'test/parallel/test-zlib-from-gzip.js',
  'test/parallel/test-zlib-from-string.js',
  'test/parallel/test-zlib-invalid-input.js',
  'test/parallel/test-zlib.js',
  'test/parallel/test-zlib-params.js',
  'test/parallel/test-zlib-random-byte-pipes.js',
  'test/parallel/test-zlib-write-after-close.js',
  'test/parallel/test-zlib-write-after-flush.js',
  'test/parallel/test-zlib-zero-byte.js'
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
    worker.on('exit', function(exitCode) {
      if (exitCode === 0)
        resolve();
      else
        reject(new Error(util.format(
            '%s exited with code %s', testFile, exitCode)));
    });
  });
}

