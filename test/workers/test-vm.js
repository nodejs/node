// Flags: --experimental-workers --harmony_proxies
var assert = require('assert');
var util = require('util');
var Worker = require('worker');
var checks = 0;

var tests = [
  'test/parallel/test-vm-basic.js',
  'test/parallel/test-vm-context-async-script.js',
  'test/parallel/test-vm-context.js',
  'test/parallel/test-vm-context-property-forwarding.js',
  'test/parallel/test-vm-create-and-run-in-context.js',
  'test/parallel/test-vm-create-context-accessors.js',
  'test/parallel/test-vm-create-context-arg.js',
  'test/parallel/test-vm-create-context-circular-reference.js',
  'test/parallel/test-vm-cross-context.js',
  'test/parallel/test-vm-debug-context.js',
  'test/parallel/test-vm-function-declaration.js',
  'test/parallel/test-vm-global-define-property.js',
  'test/parallel/test-vm-global-identity.js',
  'test/parallel/test-vm-harmony-proxies.js',
  'test/parallel/test-vm-harmony-symbols.js',
  'test/parallel/test-vm-is-context.js',
  'test/parallel/test-vm-new-script-new-context.js',
  'test/parallel/test-vm-new-script-this-context.js',
  // Needs --expose-gc but exposing it for the process causes other tests
  // to fail
  // 'test/parallel/test-vm-run-in-new-context.js',
  'test/parallel/test-vm-static-this.js',
  'test/parallel/test-vm-timeout.js'
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
    })
    worker.on('exit', function(exitCode) {
      if (exitCode === 0)
        resolve()
      else
        reject(new Error(util.format(
            '%s exited with code %s', testFile, exitCode)));
    });
  });
}
