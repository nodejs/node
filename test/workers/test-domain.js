// Flags: --experimental-workers
var assert = require('assert');
var util = require('util');
var Worker = require('worker');
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
        resolve()
      else
        reject(new Error(util.format(
            '%s exited with code %s', testFile, exitCode)));
    });
  });
}

